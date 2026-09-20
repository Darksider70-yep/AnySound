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
std::atomic<bool> g_stop{false};

void signal_handler(int) {
    g_stop.store(true);
}
}  // namespace

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string target_ip = "127.0.0.1";
    uint16_t target_port = chorus::kDefaultUdpDataPort;
    bool test_tone = false;
    int duration_sec = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test-tone") {
            test_tone = true;
        } else if (arg == "--duration" && i + 1 < argc) {
            duration_sec = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: chorus_host [client-ip] [client-port] [--test-tone] [--duration <sec>]\n";
            return 0;
        } else if (i == 1 && arg[0] != '-') {
            target_ip = arg;
        } else if (i == 2 && arg[0] != '-') {
            target_port = static_cast<uint16_t>(std::stoi(arg));
        }
    }

    std::cout << "========================================\n";
    std::cout << "Chorus Host Audio Streamer (Phase 1)\n";
    std::cout << "Target: " << target_ip << ":" << target_port << "\n";
    std::cout << "Mode: " << (test_tone ? "440Hz Test Sine Generator" : "WASAPI System Loopback Capture") << "\n";
    if (duration_sec > 0) {
        std::cout << "Duration: " << duration_sec << " seconds\n";
    }
    std::cout << "========================================\n" << std::flush;

    chorus::OpusEncoderWrap encoder;
    if (!encoder.init(chorus::kDefaultBitrate, 5)) {
        std::cerr << "Failed to initialize Opus encoder.\n" << std::flush;
        return 1;
    }

    chorus::UdpSocket socket;
    if (!socket.is_valid()) {
        std::cerr << "Failed to create UDP socket.\n" << std::flush;
        return 1;
    }

    // Allocate 1 second capacity SPSC ring buffer for PCM samples
    chorus::SpscRing<float> capture_ring(chorus::kSampleRate * chorus::kChannels);
    chorus::AudioCaptureDevice capture_device;

    if (!test_tone) {
        if (!capture_device.start_loopback(&capture_ring)) {
            std::cerr << "Failed to start audio loopback capture. Falling back to test tone generator.\n" << std::flush;
            test_tone = true;
        } else {
            std::cout << "WASAPI loopback audio capture active.\n" << std::flush;
        }
    }

    std::vector<float> frame_pcm(chorus::kFloatsPerFrame);
    std::vector<uint8_t> opus_payload(chorus::kMaxOpusPayloadBytes);
    std::vector<uint8_t> packet_buf(chorus::kMaxUdpPayloadSize);

    const chorus::Endpoint dest{target_ip, target_port};
    uint32_t seq = 0;
    uint32_t session_id = 1;
    uint64_t total_bytes_sent = 0;

    auto start_time = std::chrono::steady_clock::now();
    auto last_stats_time = start_time;
    uint32_t frames_in_window = 0;
    double tone_phase = 0.0;

    while (!g_stop.load()) {
        if (duration_sec > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();
            if (elapsed >= duration_sec) {
                break;
            }
        }
        bool frame_ready = false;

        if (test_tone) {
            // Generate 20ms of 440 Hz stereo sine tone
            constexpr double kPhaseIncrement = 2.0 * std::numbers::pi * 440.0 / static_cast<double>(chorus::kSampleRate);
            for (size_t i = 0; i < chorus::kSamplesPerFramePerChannel; ++i) {
                const float s = static_cast<float>(0.3 * std::sin(tone_phase));
                tone_phase += kPhaseIncrement;
                if (tone_phase >= 2.0 * std::numbers::pi) {
                    tone_phase -= 2.0 * std::numbers::pi;
                }
                frame_pcm[i * 2] = s;
                frame_pcm[i * 2 + 1] = s;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(chorus::kFrameDurationMs));
            frame_ready = true;
        } else {
            // Read exactly one 20ms frame (1920 floats) from the capture ring
            if (capture_ring.size() >= static_cast<size_t>(chorus::kFloatsPerFrame)) {
                size_t read_floats = capture_ring.read(frame_pcm);
                if (read_floats == static_cast<size_t>(chorus::kFloatsPerFrame)) {
                    frame_ready = true;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }

        if (frame_ready) {
            int payload_bytes = encoder.encode(frame_pcm, opus_payload);
            if (payload_bytes > 0) {
                // Construct UDP packet (Common Header 8 bytes + Audio Payload)
                // [0-1]: Magic (0x4348)
                // [2]: Version (1)
                // [3]: Type (1 = Audio)
                // [4-7]: SessionId
                // [8-11]: Seq
                // [12-19]: PlayAtHostUs (0 for phase 1)
                // [20]: Flags (0)
                // [21-22]: PayloadLen
                // [23...]: Opus payload
                chorus::endian::write_u16_be(packet_buf.data(), chorus::kPacketMagic);
                packet_buf[2] = chorus::kProtocolVersion;
                packet_buf[3] = static_cast<uint8_t>(chorus::PacketType::Audio);
                chorus::endian::write_u32_be(packet_buf.data() + 4, session_id);
                chorus::endian::write_u32_be(packet_buf.data() + 8, seq++);
                chorus::endian::write_u64_be(packet_buf.data() + 12, 0);  // playAtHostUs
                packet_buf[20] = 0;                                       // flags
                chorus::endian::write_u16_be(packet_buf.data() + 21, static_cast<uint16_t>(payload_bytes));

                std::copy_n(opus_payload.data(), payload_bytes, packet_buf.data() + 23);

                const size_t total_packet_size = 23 + static_cast<size_t>(payload_bytes);
                if (socket.send_to(std::span<const uint8_t>(packet_buf.data(), total_packet_size), dest)) {
                    total_bytes_sent += total_packet_size;
                    frames_in_window++;
                }
            }
        }

        // Print stats every 2 seconds
        auto now = std::chrono::steady_clock::now();
        if (now - last_stats_time >= std::chrono::seconds(2)) {
            const double elapsed_s = std::chrono::duration<double>(now - last_stats_time).count();
            const double fps = static_cast<double>(frames_in_window) / elapsed_s;
            const double kbps = (static_cast<double>(total_bytes_sent * 8) / 1000.0) / elapsed_s;

            std::cout << "[Host] Sent " << seq << " frames | Rate: " << fps << " fps | Bandwidth: "
                      << kbps << " kbps\n";

            frames_in_window = 0;
            total_bytes_sent = 0;
            last_stats_time = now;
        }
    }

    std::cout << "\nStopping Chorus Host...\n";
    capture_device.stop();
    return 0;
}
