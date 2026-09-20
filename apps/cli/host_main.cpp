#include <chorus/codec/opus_codec.hpp>
#include <chorus/core.hpp>
#include <chorus/net/udp_socket.hpp>
#include <chorus/playback/spsc_ring.hpp>
#include <chorus/platform/audio_device.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <numbers>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr size_t kAudioHeaderOffsetMagic = 0;
constexpr size_t kAudioHeaderOffsetVersion = 2;
constexpr size_t kAudioHeaderOffsetType = 3;
constexpr size_t kAudioHeaderOffsetSessionId = 4;
constexpr size_t kAudioHeaderOffsetSeq = 8;
constexpr size_t kAudioHeaderOffsetPlayAtUs = 12;
constexpr size_t kAudioHeaderOffsetFlags = 20;
constexpr size_t kAudioHeaderOffsetPayloadLen = 21;
constexpr size_t kAudioHeaderSize = 23;

constexpr double kDefaultToneFreqHz = 440.0;
constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr float kToneAmplitude = 0.3F;
constexpr uint32_t kDefaultSessionId = 1;

std::atomic<bool> g_stop_requested{false};

void signal_handler(int) {
    g_stop_requested.store(true);
}

struct HostConfig {
    std::string target_ip{"127.0.0.1"};
    uint16_t target_port{chorus::kDefaultUdpDataPort};
    bool test_tone{false};
    int duration_sec{0};
    bool show_help{false};
};

HostConfig parse_host_args(int argc, char* argv[]) {
    HostConfig config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--test-tone") {
            config.test_tone = true;
        } else if (arg == "--duration" && (i + 1 < argc)) {
            config.duration_sec = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            config.show_help = true;
            return config;
        } else if (i == 1 && arg[0] != '-') {
            config.target_ip = arg;
        } else if (i == 2 && arg[0] != '-') {
            config.target_port = static_cast<uint16_t>(std::stoi(arg));
        }
    }
    return config;
}

void generate_sine_samples(std::span<float> frame_pcm, double& tone_phase) {
    const double phase_increment = (kTwoPi * kDefaultToneFreqHz) / static_cast<double>(chorus::kSampleRate);
    for (size_t i = 0; i < static_cast<size_t>(chorus::kSamplesPerFramePerChannel); ++i) {
        const auto sample_val = static_cast<float>(static_cast<double>(kToneAmplitude) * std::sin(tone_phase));
        tone_phase += phase_increment;
        if (tone_phase >= kTwoPi) {
            tone_phase -= kTwoPi;
        }
        frame_pcm[i * 2] = sample_val;
        frame_pcm[(i * 2) + 1] = sample_val;
    }
}

size_t build_audio_packet(std::span<uint8_t> packet_buf,
                          std::span<const uint8_t> opus_payload,
                          uint32_t session_id,
                          uint32_t seq) {
    chorus::endian::write_u16_be(packet_buf.data() + kAudioHeaderOffsetMagic, chorus::kPacketMagic);
    packet_buf[kAudioHeaderOffsetVersion] = chorus::kProtocolVersion;
    packet_buf[kAudioHeaderOffsetType] = static_cast<uint8_t>(chorus::PacketType::Audio);
    chorus::endian::write_u32_be(packet_buf.data() + kAudioHeaderOffsetSessionId, session_id);
    chorus::endian::write_u32_be(packet_buf.data() + kAudioHeaderOffsetSeq, seq);
    chorus::endian::write_u64_be(packet_buf.data() + kAudioHeaderOffsetPlayAtUs, 0);
    packet_buf[kAudioHeaderOffsetFlags] = 0;
    chorus::endian::write_u16_be(packet_buf.data() + kAudioHeaderOffsetPayloadLen,
                                 static_cast<uint16_t>(opus_payload.size()));

    std::copy(opus_payload.begin(), opus_payload.end(), packet_buf.begin() + static_cast<ptrdiff_t>(kAudioHeaderSize));
    return kAudioHeaderSize + opus_payload.size();
}

}  // namespace

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    const HostConfig config = parse_host_args(argc, argv);
    if (config.show_help) {
        std::cout << "Usage: chorus_host [client-ip] [client-port] [--test-tone] [--duration <sec>]\n";
        return 0;
    }

    std::cout << "========================================\n";
    std::cout << "Chorus Host Audio Streamer (Phase 1)\n";
    std::cout << "Target: " << config.target_ip << ":" << config.target_port << "\n";
    std::cout << "Mode: " << (config.test_tone ? "440Hz Test Sine Generator" : "WASAPI System Loopback Capture") << "\n";
    if (config.duration_sec > 0) {
        std::cout << "Duration: " << config.duration_sec << " seconds\n";
    }
    std::cout << "========================================\n" << std::flush;

    chorus::OpusEncoderWrap encoder;
    if (!encoder.init(chorus::kDefaultBitrate, chorus::kDefaultExpectedLossPct)) {
        std::cerr << "Failed to initialize Opus encoder.\n" << std::flush;
        return 1;
    }

    chorus::UdpSocket socket;
    if (!socket.is_valid()) {
        std::cerr << "Failed to create UDP socket.\n" << std::flush;
        return 1;
    }

    const size_t ring_capacity = static_cast<size_t>(chorus::kSampleRate) * static_cast<size_t>(chorus::kChannels);
    chorus::SpscRing<float> capture_ring(ring_capacity);
    chorus::AudioCaptureDevice capture_device;

    bool active_test_tone = config.test_tone;
    if (!active_test_tone) {
        if (!capture_device.start_loopback(&capture_ring)) {
            std::cerr << "Failed to start audio loopback capture. Falling back to test tone generator.\n" << std::flush;
            active_test_tone = true;
        } else {
            std::cout << "WASAPI loopback audio capture active.\n" << std::flush;
        }
    }

    std::vector<float> frame_pcm(static_cast<size_t>(chorus::kFloatsPerFrame));
    std::vector<uint8_t> opus_payload(chorus::kMaxOpusPayloadBytes);
    std::vector<uint8_t> packet_buf(chorus::kMaxUdpPayloadSize);

    const chorus::Endpoint destination{.address = config.target_ip, .port = config.target_port};
    uint32_t seq = 0;
    uint64_t total_bytes_sent = 0;

    const auto start_time = std::chrono::steady_clock::now();
    auto last_stats_time = start_time;
    uint32_t frames_in_window = 0;
    double tone_phase = 0.0;

    while (!g_stop_requested.load()) {
        if (config.duration_sec > 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();
            if (elapsed >= config.duration_sec) {
                break;
            }
        }

        bool frame_ready = false;
        if (active_test_tone) {
            generate_sine_samples(frame_pcm, tone_phase);
            std::this_thread::sleep_for(std::chrono::milliseconds(chorus::kFrameDurationMs));
            frame_ready = true;
        } else {
            if (capture_ring.size() >= static_cast<size_t>(chorus::kFloatsPerFrame)) {
                const size_t read_floats = capture_ring.read(frame_pcm);
                if (read_floats == static_cast<size_t>(chorus::kFloatsPerFrame)) {
                    frame_ready = true;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }

        if (frame_ready) {
            const int payload_bytes = encoder.encode(frame_pcm, opus_payload);
            if (payload_bytes > 0) {
                const std::span<const uint8_t> payload_span(opus_payload.data(), static_cast<size_t>(payload_bytes));
                const size_t packet_len = build_audio_packet(packet_buf, payload_span, kDefaultSessionId, seq++);
                if (socket.send_to(std::span<const uint8_t>(packet_buf.data(), packet_len), destination)) {
                    total_bytes_sent += packet_len;
                    frames_in_window++;
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - last_stats_time >= std::chrono::seconds(2)) {
            const double elapsed_s = std::chrono::duration<double>(now - last_stats_time).count();
            const double fps = static_cast<double>(frames_in_window) / elapsed_s;
            const double kbps = (static_cast<double>(total_bytes_sent * 8) / 1000.0) / elapsed_s;

            std::cout << "[Host] Sent " << seq << " frames | Rate: " << fps << " fps | Bandwidth: "
                      << kbps << " kbps\n" << std::flush;

            frames_in_window = 0;
            total_bytes_sent = 0;
            last_stats_time = now;
        }
    }

    std::cout << "\nStopping Chorus Host...\n" << std::flush;
    capture_device.stop();
    return 0;
}
