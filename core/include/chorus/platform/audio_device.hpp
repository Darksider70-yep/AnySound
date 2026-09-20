#pragma once

#include <chorus/codec/opus_codec.hpp>
#include <chorus/playback/spsc_ring.hpp>

#include <cstdint>
#include <memory>

namespace chorus {
class TimelineBuffer;

/// @brief Audio capture device wrapping miniaudio WASAPI loopback capture.
/// Reads system audio output and pushes PCM float samples into an SPSC ring buffer.
class AudioCaptureDevice {
public:
    AudioCaptureDevice();
    ~AudioCaptureDevice();

    AudioCaptureDevice(const AudioCaptureDevice&) = delete;
    AudioCaptureDevice& operator=(const AudioCaptureDevice&) = delete;
    AudioCaptureDevice(AudioCaptureDevice&& other) noexcept;
    AudioCaptureDevice& operator=(AudioCaptureDevice&& other) noexcept;

    /// @brief Starts system loopback audio capture.
    /// @param ring Target lock-free ring buffer where captured floats are stored.
    [[nodiscard]] bool start_loopback(SpscRing<float>* ring);

    /// @brief Stops audio capture.
    void stop();

    /// @brief Returns true if capture is actively running.
    [[nodiscard]] bool is_running() const noexcept;

    /// @brief Returns total audio frames captured.
    [[nodiscard]] uint64_t frames_captured() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// @brief Audio playback device wrapping miniaudio output playback.
/// Pulls PCM float samples from an SPSC ring buffer and renders to speakers.
class AudioPlaybackDevice {
public:
    AudioPlaybackDevice();
    ~AudioPlaybackDevice();

    AudioPlaybackDevice(const AudioPlaybackDevice&) = delete;
    AudioPlaybackDevice& operator=(const AudioPlaybackDevice&) = delete;
    AudioPlaybackDevice(AudioPlaybackDevice&& other) noexcept;
    AudioPlaybackDevice& operator=(AudioPlaybackDevice&& other) noexcept;

    /// @brief Starts audio playback from an SPSC ring buffer.
    /// @param ring Source lock-free ring buffer providing audio floats.
    [[nodiscard]] bool start_playback(SpscRing<float>* ring);

    /// @brief Starts audio playback from a scheduled TimelineBuffer.
    /// @param timeline Scheduled timeline buffer providing audio floats at target timestamps.
    [[nodiscard]] bool start_playback(TimelineBuffer* timeline);

    /// @brief Stops audio playback.
    void stop();

    /// @brief Returns true if playback is actively running.
    [[nodiscard]] bool is_running() const noexcept;

    /// @brief Returns total audio frames rendered.
    [[nodiscard]] uint64_t frames_rendered() const noexcept;

    /// @brief Returns number of underrun frames encountered.
    [[nodiscard]] uint64_t underruns() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace chorus
