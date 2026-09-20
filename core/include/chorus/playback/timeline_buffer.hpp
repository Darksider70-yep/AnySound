#pragma once

#include <chorus/codec/opus_codec.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace chorus {

/// @brief Result of a read operation from TimelineBuffer.
enum class TimelineReadResult {
    Ok,       ///< Valid audio samples read
    Gap,      ///< Gap in timeline (packet lost/delayed, caller should run PLC or fill silence)
    Empty     ///< Buffer has not started or is starved
};

/// @brief Scheduled timeline playout buffer.
/// Maps incoming PCM frames stamped with microsecond playout targets into a continuous
/// discrete audio frame timeline.
/// Thread affinity:
/// - Producer thread (network worker) calls insert_frame().
/// - Consumer thread (real-time audio callback) calls read_samples().
class TimelineBuffer {
public:
    explicit TimelineBuffer(size_t capacity_frames = 48000 * 2);  // 2 seconds capacity
    ~TimelineBuffer() = default;

    TimelineBuffer(const TimelineBuffer&) = delete;
    TimelineBuffer& operator=(const TimelineBuffer&) = delete;
    TimelineBuffer(TimelineBuffer&&) = delete;
    TimelineBuffer& operator=(TimelineBuffer&&) = delete;

    /// @brief Inserts a decoded 20ms audio frame (960 stereo samples) at target local microsecond.
    /// @param local_play_us Target playout time in local steady_clock microseconds.
    /// @param pcm_in 1920 interleaved stereo float samples.
    /// @return True if inserted, false if late and dropped.
    bool insert_frame(uint64_t local_play_us, std::span<const float> pcm_in) noexcept;

    /// @brief Reads samples into output buffer for real-time playout.
    /// Safe for use in hard real-time audio callbacks (zero allocations, lock-free).
    /// @param out_pcm Output buffer to receive interleaved float samples.
    /// @param current_local_us Current audio hardware output timestamp.
    /// @return Result status (Ok, Gap, Empty).
    TimelineReadResult read_samples(std::span<float> out_pcm, uint64_t current_local_us) noexcept;

    /// @brief Returns total frames dropped due to arriving past their playout deadline.
    [[nodiscard]] uint64_t late_frames_dropped() const noexcept;

    /// @brief Returns total timeline gaps detected.
    [[nodiscard]] uint64_t gaps_detected() const noexcept;

    /// @brief Returns true if timeline has been anchored and is active.
    [[nodiscard]] bool is_active() const noexcept;

    /// @brief Resets buffer timeline and counters.
    void reset() noexcept;

private:
    const size_t capacity_samples_;
    std::vector<float> sample_buffer_;
    std::vector<uint8_t> valid_mask_;  // 1 if sample is populated, 0 if gap/empty

    std::atomic<bool> is_anchored_{false};
    uint64_t anchor_local_us_{0};
    uint64_t playhead_sample_index_{0};

    std::atomic<uint64_t> late_frames_{0};
    std::atomic<uint64_t> gaps_{0};
};

}  // namespace chorus
