#include <chorus/codec/opus_codec.hpp>
#include <chorus/core.hpp>
#include <chorus/net/udp_socket.hpp>
#include <chorus/playback/spsc_ring.hpp>
#include <chorus/platform/audio_device.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {
std::atomic<bool> g_stop{false};

void signal_handler(int) {
    g_stop.store(true);
}
}  // namespace

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    uint16_t listen_port = chorus::kDefaultUdpDataPort;
    int duration_sec = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--duration" && i + 1 < argc) {
            duration_sec = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: chorus_client [listen-port] [--duration <sec>]\n";
            return 0;
        } else if (i == 1 && arg[0] != '-') {
            listen_port = static_cast<uint16_t>(std::stoi(arg));
        }
    }

    std::cout << "========================================\n";
    std::cout << "Chorus Client Audio Receiver (Phase 1)\n";
    std::cout << "Listening on UDP port: " << listen_port << "\n";
    if (duration_sec > 0) {
        std::cout << "Duration: " << duration_sec << " seconds\n";
    }
    std::cout << "========================================\n" << std::flush;

    chorus::UdpSocket socket;
    if (!socket.bind(listen_port, "0.0.0.0")) {
        std::cerr << "Failed to bind UDP socket on port " << listen_port << "\n" << std::flush;
        return 1;
    }
    if (!socket.set_recv_timeout_ms(100)) {
        std::cerr << "Warning: Failed to set socket recv timeout\n" << std::flush;
    }

    chorus::OpusDecoderWrap decoder;
    if (!decoder.init()) {
        std::cerr << "Failed to initialize Opus decoder.\n" << std::flush;
        return 1;
    }

    // 1 second capacity buffer (fixed ~300ms pre-fill buffer target)
    constexpr size_t kBufferFloats = chorus::kSampleRate * chorus::kChannels;  // 96000 floats
    chorus::SpscRing<float> playout_ring(kBufferFloats);
    chorus::AudioPlaybackDevice playback_device;

    std::vector<uint8_t> packet_buf(chorus::kMaxUdpPayloadSize);
    std::vector<float> pcm_decoded(chorus::kFloatsPerFrame);

    uint64_t packets_received = 0;
    uint64_t bytes_received = 0;
    bool playback_started = false;
    constexpr size_t kPrebufferFloats = 15 * chorus::kFloatsPerFrame;  // 300 ms

    auto start_time = std::chrono::steady_clock::now();
    auto last_stats_time = start_time;
    uint32_t packets_in_window = 0;

    std::cout << "Waiting for incoming audio stream...\n" << std::flush;

    while (!g_stop.load()) {
        if (duration_sec > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();
            if (elapsed >= duration_sec) {
                break;
            }
        }
        chorus::Endpoint sender;
        const int bytes = socket.receive_from(packet_buf, sender);

        if (bytes > 0 && static_cast<size_t>(bytes) >= 23) {
            // Validate header
            const uint16_t magic = chorus::endian::read_u16_be(packet_buf.data());
            const uint8_t version = packet_buf[2];
            const uint8_t type = packet_buf[3];

            if (magic == chorus::kPacketMagic && version == chorus::kProtocolVersion &&
                type == static_cast<uint8_t>(chorus::PacketType::Audio)) {

                const uint32_t seq = chorus::endian::read_u32_be(packet_buf.data() + 8);
                const uint16_t payload_len = chorus::endian::read_u16_be(packet_buf.data() + 21);

                if (23 + static_cast<size_t>(payload_len) <= static_cast<size_t>(bytes)) {
                    std::span<const uint8_t> opus_payload(packet_buf.data() + 23, payload_len);

                    int samples = decoder.decode(opus_payload, pcm_decoded);
                    if (samples == chorus::kSamplesPerFramePerChannel) {
                        playout_ring.write(pcm_decoded);
                        packets_received++;
                        packets_in_window++;
                        bytes_received += static_cast<size_t>(bytes);

                        // Start playback once pre-buffer reaches 300 ms
                        if (!playback_started && playout_ring.size() >= kPrebufferFloats) {
                            if (playback_device.start_playback(&playout_ring)) {
                                playback_started = true;
                                std::cout << "Pre-buffer reached 300 ms. Audio playback active!\n";
                            }
                        }
                    }
                }
                (void)seq;
            }
        }

        // Periodic stats
        auto now = std::chrono::steady_clock::now();
        if (now - last_stats_time >= std::chrono::seconds(2)) {
            if (packets_received > 0) {
                const double elapsed_s = std::chrono::duration<double>(now - last_stats_time).count();
                const double fps = static_cast<double>(packets_in_window) / elapsed_s;
                const size_t buffered_ms = (playout_ring.size() * 1000) / (chorus::kSampleRate * chorus::kChannels);

                std::cout << "[Client] Recv " << packets_received << " frames | Rate: " << fps
                          << " fps | Buffer: " << buffered_ms << " ms | Underruns: "
                          << playback_device.underruns() << "\n";
            }
            packets_in_window = 0;
            last_stats_time = now;
        }
    }

    std::cout << "\nStopping Chorus Client...\n";
    playback_device.stop();
    return 0;
}
