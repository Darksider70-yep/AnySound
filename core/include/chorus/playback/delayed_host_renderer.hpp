#pragma once

#include <chorus/core.hpp>
#include <chorus/playback/timeline_buffer.hpp>
#include <chorus/platform/audio_device.hpp>

#include <cstdint>
#include <span>

namespace chorus {

/// @brief Delayed host renderer allowing the host's physical speakers to play captured audio
/// delayed by target_latency_ms in lockstep synchronization with remote clients.
class DelayedHostRenderer {
public:
    explicit DelayedHostRenderer(uint64_t target_latency_ms = kDefaultTargetLatencyMs);
    ~DelayedHostRenderer();

    DelayedHostRenderer(const DelayedHostRenderer&) = delete;
    DelayedHostRenderer& operator=(const DelayedHostRenderer&) = delete;
    DelayedHostRenderer(DelayedHostRenderer&&) = delete;
    DelayedHostRenderer& operator=(DelayedHostRenderer&&) = delete;

    /// @brief Starts delayed local playback on the host output device.
    [[nodiscard]] bool start();

    /// @brief Stops delayed local playback.
    void stop();

    /// @brief Submits a captured PCM frame to be scheduled for delayed local playout.
    /// @param frame_pcm 1920 interleaved stereo float samples (20 ms @ 48kHz).
    /// @param capture_host_us Capture timestamp in host microseconds.
    void submit_frame(std::span<const float> frame_pcm, uint64_t capture_host_us);

    /// @brief Adjusts the target latency in milliseconds.
    void set_target_latency_ms(uint64_t target_latency_ms);

    /// @brief Sets the local host playback volume (0.0 to 1.0).
    void set_volume(float volume);

    /// @brief Sets the local host playback mute state.
    void set_mute(bool mute);

    /// @brief Returns true if delayed rendering is actively running.
    [[nodiscard]] bool is_running() const noexcept { return is_running_; }

    /// @brief Returns total frames rendered locally.
    [[nodiscard]] uint64_t frames_rendered() const noexcept;

private:
    uint64_t target_latency_ms_{kDefaultTargetLatencyMs};
    TimelineBuffer timeline_buffer_;
    AudioPlaybackDevice playback_device_;
    float volume_{1.0F};
    bool is_muted_{false};
    bool is_running_{false};
};

}  // namespace chorus
