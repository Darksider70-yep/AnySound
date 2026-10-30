#include <chorus/playback/timeline_buffer.hpp>

#include <algorithm>
#include <cmath>

namespace chorus {

TimelineBuffer::TimelineBuffer(size_t capacity_frames)
    : capacity_samples_(capacity_frames * static_cast<size_t>(kChannels)),
      sample_buffer_(capacity_samples_, 0.0F),
      valid_mask_(std::make_unique<std::atomic<uint8_t>[]>(capacity_frames)) {
    for (size_t i = 0; i < capacity_frames; ++i) {
        valid_mask_[i].store(0, std::memory_order_relaxed);
    }
}

bool TimelineBuffer::insert_frame(uint64_t local_play_us, std::span<const float> pcm_in) noexcept {
    if (pcm_in.empty() || (pcm_in.size() % static_cast<size_t>(kChannels) != 0)) {
        return false;
    }

    if (!is_anchored_.load(std::memory_order_acquire)) {
        anchor_local_us_.store(local_play_us, std::memory_order_release);
        playhead_sample_index_.store(0, std::memory_order_release);
        is_anchored_.store(true, std::memory_order_release);
    }

    const uint64_t anchor_us = anchor_local_us_.load(std::memory_order_acquire);
    if (local_play_us < anchor_us) {
        late_frames_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    const uint64_t elapsed_us = local_play_us - anchor_us;
    const uint64_t target_frame_idx = (elapsed_us * static_cast<uint64_t>(kSampleRate)) / 1000000ULL;

    if (target_frame_idx < playhead_sample_index_.load(std::memory_order_relaxed)) {
        late_frames_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    const size_t total_frames_capacity = capacity_samples_ / static_cast<size_t>(kChannels);
    const size_t frame_count = pcm_in.size() / static_cast<size_t>(kChannels);

    for (size_t i = 0; i < frame_count; ++i) {
        const size_t current_f = (target_frame_idx + i) % total_frames_capacity;
        const size_t base_idx = current_f * static_cast<size_t>(kChannels);

        sample_buffer_[base_idx] = pcm_in[i * 2];
        sample_buffer_[base_idx + 1] = pcm_in[(i * 2) + 1];
        valid_mask_[current_f].store(1, std::memory_order_release);
    }

    return true;
}

TimelineReadResult TimelineBuffer::read_samples(std::span<float> out_pcm, uint64_t current_local_us) noexcept {
    if (!is_anchored_.load(std::memory_order_acquire) || out_pcm.empty()) {
        std::ranges::fill(out_pcm, 0.0F);
        return TimelineReadResult::Empty;
    }

    const uint64_t anchor_us = anchor_local_us_.load(std::memory_order_acquire);
    if (current_local_us < anchor_us) {
        // Still in prebuffering window before scheduled anchor playout
        std::ranges::fill(out_pcm, 0.0F);
        return TimelineReadResult::Empty;
    }

    const uint64_t elapsed_us = current_local_us - anchor_us;
    const uint64_t target_playhead_idx = (elapsed_us * static_cast<uint64_t>(kSampleRate)) / 1000000ULL;
    const uint64_t current_playhead = playhead_sample_index_.load(std::memory_order_relaxed);

    uint64_t start_idx = target_playhead_idx;
    if (current_playhead > 0) {
        const int64_t lag = static_cast<int64_t>(target_playhead_idx) - static_cast<int64_t>(current_playhead);
        // If scheduling lag is within +/- 10ms (480 samples), maintain continuous readout without artificial gaps
        if (std::abs(lag) < 480) {
            start_idx = current_playhead;
        }
    }

    const size_t needed_frames = out_pcm.size() / static_cast<size_t>(kChannels);
    const size_t total_frames_capacity = capacity_samples_ / static_cast<size_t>(kChannels);

    bool has_gap = false;
    for (size_t i = 0; i < needed_frames; ++i) {
        const size_t f = (start_idx + i) % total_frames_capacity;
        const size_t base_idx = f * static_cast<size_t>(kChannels);

        if (valid_mask_[f].load(std::memory_order_acquire) == 1) {
            out_pcm[i * 2] = sample_buffer_[base_idx];
            out_pcm[(i * 2) + 1] = sample_buffer_[base_idx + 1];
            valid_mask_[f].store(0, std::memory_order_release);
        } else {
            out_pcm[i * 2] = 0.0F;
            out_pcm[(i * 2) + 1] = 0.0F;
            has_gap = true;
        }
    }

    playhead_sample_index_.store(start_idx + needed_frames, std::memory_order_relaxed);

    if (has_gap) {
        gaps_.fetch_add(1, std::memory_order_relaxed);
        return TimelineReadResult::Gap;
    }

    return TimelineReadResult::Ok;
}

uint64_t TimelineBuffer::late_frames_dropped() const noexcept {
    return late_frames_.load(std::memory_order_relaxed);
}

uint64_t TimelineBuffer::gaps_detected() const noexcept {
    return gaps_.load(std::memory_order_relaxed);
}

bool TimelineBuffer::is_active() const noexcept {
    return is_anchored_.load(std::memory_order_relaxed);
}

void TimelineBuffer::reset() noexcept {
    is_anchored_.store(false, std::memory_order_release);
    anchor_local_us_.store(0, std::memory_order_release);
    playhead_sample_index_.store(0, std::memory_order_release);
    late_frames_.store(0, std::memory_order_relaxed);
    gaps_.store(0, std::memory_order_relaxed);
    std::ranges::fill(sample_buffer_, 0.0F);
    const size_t total_frames_capacity = capacity_samples_ / static_cast<size_t>(kChannels);
    for (size_t i = 0; i < total_frames_capacity; ++i) {
        valid_mask_[i].store(0, std::memory_order_relaxed);
    }
}

}  // namespace chorus