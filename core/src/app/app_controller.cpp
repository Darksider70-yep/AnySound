#include <chorus/app/app_controller.hpp>

#include <cmath>
#include <numbers>

namespace chorus {

namespace {
constexpr double kSineFreqHz = 440.0;
constexpr double kTwoPi = 2.0 * std::numbers::pi;
}  // namespace

AppController::AppController()
    : capture_ring_(static_cast<size_t>(kSampleRate * kChannels)) {
    (void)discovery_scanner_.start();
}

AppController::~AppController() {
    stop_host();
    leave_host();
    discovery_scanner_.stop();
}

bool AppController::start_host(std::string_view pin,
                               [[maybe_unused]] uint64_t target_latency_ms,
                               bool use_test_tone) {
    leave_host();
    stop_host();

    use_test_tone_ = use_test_tone;
    tone_phase_ = 0.0;

    if (!host_session_.start(kDefaultTcpControlPort, kDefaultUdpDataPort, pin)) {
        return false;
    }

    if (!use_test_tone_) {
        if (!capture_device_.start_loopback(&capture_ring_)) {
            use_test_tone_ = true;  // Fallback to test tone
        }
    }

    (void)discovery_broadcaster_.start("Chorus Host", kDefaultTcpControlPort, kDefaultUdpDataPort);
    role_ = AppRole::Hosting;
    return true;
}

void AppController::stop_host() {
    if (role_ == AppRole::Hosting) {
        discovery_broadcaster_.stop();
        capture_device_.stop();
        host_session_.stop();
        role_ = AppRole::Idle;
    }
}

bool AppController::join_host(std::string_view host_ip,
                              uint16_t tcp_port,
                              std::string_view pin) {
    stop_host();
    leave_host();

    if (!client_session_.connect(host_ip, tcp_port, 47802, pin)) {
        return false;
    }

    role_ = AppRole::Client;
    return true;
}

void AppController::leave_host() {
    if (role_ == AppRole::Client) {
        client_session_.disconnect();
        role_ = AppRole::Idle;
    }
}

void AppController::generate_test_sine(std::span<float> out_pcm) {
    const double phase_inc = (kTwoPi * kSineFreqHz) / static_cast<double>(kSampleRate);
    const size_t frames = out_pcm.size() / static_cast<size_t>(kChannels);
    for (size_t i = 0; i < frames; ++i) {
        const auto s = static_cast<float>(0.3 * std::sin(tone_phase_));
        tone_phase_ += phase_inc;
        if (tone_phase_ >= kTwoPi) {
            tone_phase_ -= kTwoPi;
        }
        out_pcm[i * 2] = s;
        out_pcm[i * 2 + 1] = s;
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
        }
    } else if (role_ == AppRole::Client) {
        client_session_.update();
    }
}

AppSnapshot AppController::snapshot() const {
    AppSnapshot snap;
    snap.role = role_;
    snap.discovered_hosts = discovery_scanner_.discovered_hosts();

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
}

void AppController::set_mute(bool mute) {
    if (role_ == AppRole::Client) {
        client_session_.set_mute(mute);
    }
}

void AppController::set_offset_ms(int32_t offset_ms) {
    if (role_ == AppRole::Client) {
        client_session_.set_offset_ms(offset_ms);
    }
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

}  // namespace chorus
