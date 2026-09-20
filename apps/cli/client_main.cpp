#include <chorus/codec/opus_codec.hpp>
#include <chorus/core.hpp>
#include <chorus/net/udp_socket.hpp>
#include <chorus/playback/spsc_ring.hpp>
#include <chorus/platform/audio_device.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kDefaultRecvTimeoutMs = 100;
constexpr size_t kAudioHeaderSize = 23;
constexpr size_t kAudioHeaderOffsetMagic = 0;
constexpr size_t kAudioHeaderOffsetVersion = 2;
constexpr size_t kAudioHeaderOffsetType = 3;
constexpr size_t kAudioHeaderOffsetPayloadLen = 21;
constexpr size_t kPrebufferFrameCount = 15;

std::atomic<bool> g_stop_requested{false};

void signal_handler(int) {
    g_stop_requested.store(true);
}

struct ClientConfig {
    uint16_t listen_port{chorus::kDefaultUdpDataPort};
    int duration_sec{0};
    bool show_help{false};
};

ClientConfig parse_client_args(int argc, char* argv[]) {
    ClientConfig config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--duration" && (i + 1 < argc)) {
            config.duration_sec = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            config.show_help = true;
            return config;
        } else if (i == 1 && arg[0] != '-') {
            config.listen_port = static_cast<uint16_t>(std::stoi(arg));
        }
    }
    return config;
}

bool validate_and_decode_packet(std::span<const uint8_t> packet_data,
                                chorus::OpusDecoderWrap& decoder,
                                std::span<float> pcm_decoded) {
    if (packet_data.size() < kAudioHeaderSize) {
        return false;
    }

    const uint16_t magic = chorus::endian::read_u16_be(packet_data.data() + kAudioHeaderOffsetMagic);
    const uint8_t version = packet_data[kAudioHeaderOffsetVersion];
    const uint8_t type = packet_data[kAudioHeaderOffsetType];

    if (magic != chorus::kPacketMagic || version != chorus::kProtocolVersion ||
        type != static_cast<uint8_t>(chorus::PacketType::Audio)) {
        return false;
    }

    const uint16_t payload_len = chorus::endian::read_u16_be(packet_data.data() + kAudioHeaderOffsetPayloadLen);
    if ((kAudioHeaderSize + static_cast<size_t>(payload_len)) > packet_data.size()) {
        return false;
    }

    const std::span<const uint8_t> opus_payload(packet_data.data() + kAudioHeaderSize, payload_len);
    const int samples = decoder.decode(opus_payload, pcm_decoded);
    return (samples == chorus::kSamplesPerFramePerChannel);
}

}  // namespace

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    const ClientConfig config = parse_client_args(argc, argv);
    if (config.show_help) {
        std::cout << "Usage: chorus_client [listen-port] [--duration <sec>]\n";
        return 0;
    }

    std::cout << "========================================\n";
    std::cout << "Chorus Client Audio Receiver (Phase 1)\n";
    std::cout << "Listening on UDP port: " << config.listen_port << "\n";
    if (config.duration_sec > 0) {
        std::cout << "Duration: " << config.duration_sec << " seconds\n";
    }
    std::cout << "========================================\n" << std::flush;

    chorus::UdpSocket socket;
    if (!socket.bind(config.listen_port, "0.0.0.0")) {
        std::cerr << "Failed to bind UDP socket on port " << config.listen_port << "\n" << std::flush;
        return 1;
    }
    if (!socket.set_recv_timeout_ms(kDefaultRecvTimeoutMs)) {
        std::cerr << "Warning: Failed to set socket recv timeout\n" << std::flush;
    }

    chorus::OpusDecoderWrap decoder;
    if (!decoder.init()) {
        std::cerr << "Failed to initialize Opus decoder.\n" << std::flush;
        return 1;
    }

    const size_t ring_capacity = static_cast<size_t>(chorus::kSampleRate) * static_cast<size_t>(chorus::kChannels);
    chorus::SpscRing<float> playout_ring(ring_capacity);
    chorus::AudioPlaybackDevice playback_device;

    std::vector<uint8_t> packet_buf(chorus::kMaxUdpPayloadSize);
    std::vector<float> pcm_decoded(static_cast<size_t>(chorus::kFloatsPerFrame));

    uint64_t packets_received = 0;
    bool playback_started = false;
    const size_t prebuffer_target = kPrebufferFrameCount * static_cast<size_t>(chorus::kFloatsPerFrame);

    const auto start_time = std::chrono::steady_clock::now();
    auto last_stats_time = start_time;
    uint32_t packets_in_window = 0;

    std::cout << "Waiting for incoming audio stream...\n" << std::flush;

    while (!g_stop_requested.load()) {
        if (config.duration_sec > 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();
            if (elapsed >= config.duration_sec) {
                break;
            }
        }

        chorus::Endpoint sender;
        const int bytes = socket.receive_from(packet_buf, sender);

        if (bytes > 0) {
            const std::span<const uint8_t> packet_span(packet_buf.data(), static_cast<size_t>(bytes));
            if (validate_and_decode_packet(packet_span, decoder, pcm_decoded)) {
                playout_ring.write(pcm_decoded);
                packets_received++;
                packets_in_window++;

                if (!playback_started && (playout_ring.size() >= prebuffer_target)) {
                    if (playback_device.start_playback(&playout_ring)) {
                        playback_started = true;
                        std::cout << "Pre-buffer reached 300 ms. Audio playback active!\n" << std::flush;
                    }
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - last_stats_time >= std::chrono::seconds(2)) {
            if (packets_received > 0) {
                const double elapsed_s = std::chrono::duration<double>(now - last_stats_time).count();
                const double fps = static_cast<double>(packets_in_window) / elapsed_s;
                const size_t buffered_ms = (playout_ring.size() * 1000) / (static_cast<size_t>(chorus::kSampleRate) * static_cast<size_t>(chorus::kChannels));

                std::cout << "[Client] Recv " << packets_received << " frames | Rate: " << fps
                          << " fps | Buffer: " << buffered_ms << " ms | Underruns: "
                          << playback_device.underruns() << "\n" << std::flush;
            }
            packets_in_window = 0;
            last_stats_time = now;
        }
    }

    std::cout << "\nStopping Chorus Client...\n" << std::flush;
    playback_device.stop();
    return 0;
}
