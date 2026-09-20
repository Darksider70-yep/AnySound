#include <chorus/playback/delayed_host_renderer.hpp>

#include <algorithm>
#include <vector>

namespace chorus {

DelayedHostRenderer::DelayedHostRenderer(uint64_t target_latency_ms)
    : target_latency_ms_(target_latency_ms) {}

DelayedHostRenderer::~DelayedHostRenderer() {
    stop();
}

bool DelayedHostRenderer::start() {
    if (is_running_) {
        return true;
    }
    timeline_buffer_.reset();
    bool ok = playback_device_.start_playback(&timeline_buffer_);
    if (ok) {
        is_running_ = true;
    }
    return ok;
}

void DelayedHostRenderer::stop() {
    if (!is_running_) {
        return;
    }
    playback_device_.stop();
    timeline_buffer_.reset();
    is_running_ = false;
}

void DelayedHostRenderer::submit_frame(std::span<const float> frame_pcm, uint64_t capture_host_us) {
    if (!is_running_) {
        return;
    }

    uint64_t play_at_host_us = capture_host_us + (target_latency_ms_ * 1000ULL);

    if (is_muted_) {
        std::vector<float> silence(frame_pcm.size(), 0.0F);
        (void)timeline_buffer_.insert_frame(play_at_host_us, silence);
    } else if (volume_ < 0.999F) {
        std::vector<float> scaled(frame_pcm.size());
        for (size_t i = 0; i < frame_pcm.size(); ++i) {
            scaled[i] = frame_pcm[i] * volume_;
        }
        (void)timeline_buffer_.insert_frame(play_at_host_us, scaled);
    } else {
        (void)timeline_buffer_.insert_frame(play_at_host_us, frame_pcm);
    }
}

void DelayedHostRenderer::set_target_latency_ms(uint64_t target_latency_ms) {
    target_latency_ms_ = target_latency_ms;
}

void DelayedHostRenderer::set_volume(float volume) {
    volume_ = std::clamp(volume, 0.0F, 1.0F);
}

void DelayedHostRenderer::set_mute(bool mute) {
    is_muted_ = mute;
}

uint64_t DelayedHostRenderer::frames_rendered() const noexcept {
    return playback_device_.frames_rendered();
}

}  // namespace chorus
