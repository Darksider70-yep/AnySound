#include <chorus/app/app_controller.hpp>

#include <chrono>
#include <cmath>
#include <numbers>

namespace chorus {

namespace {
constexpr double kSineFreqHz = 440.0;
constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr float kSineAmplitude = 0.3F;
}  // namespace

AppController::AppController()
    : capture_ring_(static_cast<size_t>(kSampleRate * kChannels)) {
    (void)discovery_scanner_.start();
    DiagnosticLogger::instance().log(DiagnosticEventType::SessionStarted, "Chorus AppController initialized");
}

AppController::~AppController() {
    stop_host();
    leave_host();
    discovery_scanner_.stop();
    DiagnosticLogger::instance().log(DiagnosticEventType::SessionStopped, "Chorus AppController destroyed");
}

bool AppController::start_host(std::string_view pin,
                               uint64_t target_latency_ms,
                               bool use_test_tone) {
    leave_host();
    stop_host();

    use_test_tone_ = use_test_tone;
    target_latency_ms_ = target_latency_ms;
    tone_phase_ = 0.0;

    if (is_encrypted_) {
        // Derive session key using PIN as salt
        std::array<uint8_t, 32> ikm = {
            0x43, 0x68, 0x6F, 0x72, 0x75, 0x73, 0x53, 0x65,
            0x73, 0x73, 0x69, 0x6F, 0x6E, 0x4B, 0x65, 0x79,
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
        };
        auto key = CryptoChannel::derive_session_key(ikm, pin);
        crypto_channel_.set_key(key);
    }

    if (!host_session_.start(kDefaultTcpControlPort, kDefaultUdpDataPort, pin)) {
        DiagnosticLogger::instance().log(DiagnosticEventType::SessionStopped, "Failed to start HostSession");
        return false;
    }

    if (!use_test_tone_) {
        if (!capture_device_.start_loopback(&capture_ring_)) {
            use_test_tone_ = true;  // Fallback to test tone
            DiagnosticLogger::instance().log(DiagnosticEventType::BufferUnderrun, "Loopback capture unavailable, falling back to test tone");
        }
    }

    if (delayed_host_enabled_) {
        delayed_host_renderer_.set_target_latency_ms(target_latency_ms_);
        (void)delayed_host_renderer_.start();
    }

    (void)discovery_broadcaster_.start("Chorus Host", kDefaultTcpControlPort, kDefaultUdpDataPort);
    role_ = AppRole::Hosting;
    DiagnosticLogger::instance().log(DiagnosticEventType::SessionStarted, "Host session started", std::string(pin));
    return true;
}

void AppController::stop_host() {
    if (role_ == AppRole::Hosting) {
        delayed_host_renderer_.stop();
        discovery_broadcaster_.stop();
        capture_device_.stop();
        host_session_.stop();
        role_ = AppRole::Idle;
        DiagnosticLogger::instance().log(DiagnosticEventType::SessionStopped, "Host session stopped");
    }
}

bool AppController::join_host(std::string_view host_ip,
                              uint16_t tcp_port,
                              std::string_view pin) {
    stop_host();
    leave_host();

    if (is_encrypted_) {
        std::array<uint8_t, 32> ikm = {
            0x43, 0x68, 0x6F, 0x72, 0x75, 0x73, 0x53, 0x65,
            0x73, 0x73, 0x69, 0x6F, 0x6E, 0x4B, 0x65, 0x79,
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
        };
        auto key = CryptoChannel::derive_session_key(ikm, pin);
        crypto_channel_.set_key(key);
    }

    if (!client_session_.connect(host_ip, tcp_port, kDefaultClientDataPort, pin)) {
        DiagnosticLogger::instance().log(DiagnosticEventType::ClientDisconnected, "Failed to connect to host", std::string(host_ip));
        return false;
    }

    role_ = AppRole::Client;
    DiagnosticLogger::instance().log(DiagnosticEventType::ClientConnected, "Client connected to host", std::string(host_ip));
    return true;
}

void AppController::leave_host() {
    if (role_ == AppRole::Client) {
        client_session_.disconnect();
        role_ = AppRole::Idle;
        DiagnosticLogger::instance().log(DiagnosticEventType::ClientDisconnected, "Client left host");
    }
}

void AppController::generate_test_sine(std::span<float> out_pcm) {
    const double phase_inc = (kTwoPi * kSineFreqHz) / static_cast<double>(kSampleRate);
    const size_t frames = out_pcm.size() / static_cast<size_t>(kChannels);
    for (size_t i = 0; i < frames; ++i) {
        const auto sample_val = static_cast<float>(static_cast<double>(kSineAmplitude) * std::sin(tone_phase_));
        tone_phase_ += phase_inc;
        if (tone_phase_ >= kTwoPi) {
            tone_phase_ -= kTwoPi;
        }
        out_pcm[i * 2] = sample_val;
        out_pcm[(i * 2) + 1] = sample_val;
    }
}

void AppController::update() {
    discovery_scanner_.update();

    if (role_ == AppRole::Hosting) {
        discovery_broadcaster_.update();
        host_session_.update();

        // Pull audio frames from capture ring or generate sine tone
        std::vector<float> frame_pcm(static_cast<size_t>(kFloatsPerFrame));
        bool has_frame = false;

        if (use_test_tone_) {
            generate_test_sine(frame_pcm);
            has_frame = true;
        } else if (capture_ring_.size() >= static_cast<size_t>(kFloatsPerFrame)) {
            const size_t read_floats = capture_ring_.read(frame_pcm);
            has_frame = (read_floats == static_cast<size_t>(kFloatsPerFrame));
        }

        if (has_frame) {
            (void)host_session_.broadcast_audio_frame(frame_pcm);
            if (delayed_host_enabled_) {
                auto now_us = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    ).count()
                );
                delayed_host_renderer_.submit_frame(frame_pcm, now_us);
            }
        }
    } else if (role_ == AppRole::Client) {
        client_session_.update();
        auto stats = client_session_.stats();
        latency_tuner_.record_loss_sample(stats.loss_pct);
    }
}

AppSnapshot AppController::snapshot() const {
    AppSnapshot snap;
    snap.role = role_;
    snap.is_encrypted = is_encrypted_;
    snap.delayed_host_enabled = delayed_host_enabled_;
    snap.discovered_hosts = discovery_scanner_.discovered_hosts();
    snap.network_quality = latency_tuner_.evaluate();

    if (role_ == AppRole::Hosting) {
        snap.is_active = host_session_.is_running();
        snap.session_pin = host_session_.pin();
        snap.connected_clients = host_session_.client_list();
    } else if (role_ == AppRole::Client) {
        snap.is_active = client_session_.is_active();
        snap.rejection_reason = client_session_.rejection_reason();
        snap.volume = client_session_.volume();
        snap.is_muted = client_session_.is_muted();
        snap.offset_ms = client_session_.offset_ms();
        snap.client_stats = client_session_.stats();
    }
    return snap;
}

void AppController::set_volume(float volume) {
    if (role_ == AppRole::Client) {
        client_session_.set_volume(volume);
    }
    delayed_host_renderer_.set_volume(volume);
}

void AppController::set_mute(bool mute) {
    if (role_ == AppRole::Client) {
        client_session_.set_mute(mute);
    }
    delayed_host_renderer_.set_mute(mute);
}

void AppController::set_offset_ms(int32_t offset_ms) {
    if (role_ == AppRole::Client) {
        client_session_.set_offset_ms(offset_ms);
    }
}

void AppController::set_delayed_host(bool enable) {
    delayed_host_enabled_ = enable;
    if (role_ == AppRole::Hosting) {
        if (enable) {
            delayed_host_renderer_.set_target_latency_ms(target_latency_ms_);
            (void)delayed_host_renderer_.start();
        } else {
            delayed_host_renderer_.stop();
        }
    }
}

void AppController::set_encrypted(bool enable) {
    is_encrypted_ = enable;
}

bool AppController::set_client_volume(uint32_t client_id, float volume) {
    if (role_ == AppRole::Hosting) {
        return host_session_.set_client_volume(client_id, volume);
    }
    return false;
}

bool AppController::set_client_mute(uint32_t client_id, bool mute) {
    if (role_ == AppRole::Hosting) {
        return host_session_.set_client_mute(client_id, mute);
    }
    return false;
}

bool AppController::set_client_offset_ms(uint32_t client_id, int32_t offset_ms) {
    if (role_ == AppRole::Hosting) {
        return host_session_.set_client_offset_ms(client_id, offset_ms);
    }
    return false;
}

std::string AppController::export_diagnostics() const {
    return DiagnosticLogger::instance().export_json();
}

}  // namespace chorus
