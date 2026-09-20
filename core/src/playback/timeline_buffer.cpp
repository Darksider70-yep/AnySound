#include <chorus/playback/timeline_buffer.hpp>

#include <algorithm>

namespace chorus {

TimelineBuffer::TimelineBuffer(size_t capacity_frames)
    : capacity_samples_(capacity_frames * static_cast<size_t>(kChannels)),
      sample_buffer_(capacity_samples_, 0.0F),
      valid_mask_(capacity_frames, 0) {}

bool TimelineBuffer::insert_frame(uint64_t local_play_us, std::span<const float> pcm_in) noexcept {
    if (pcm_in.size() != static_cast<size_t>(kFloatsPerFrame)) {
        return false;
    }

    if (!is_anchored_.load(std::memory_order_acquire)) {
        anchor_local_us_ = local_play_us;
        playhead_sample_index_ = 0;
        is_anchored_.store(true, std::memory_order_release);
    }

    if (local_play_us < anchor_local_us_) {
        late_frames_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    const uint64_t elapsed_us = local_play_us - anchor_local_us_;
    const uint64_t target_frame_idx = (elapsed_us * static_cast<uint64_t>(kSampleRate)) / 1000000ULL;

    if (target_frame_idx < playhead_sample_index_) {
        late_frames_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    const size_t total_frames_capacity = capacity_samples_ / static_cast<size_t>(kChannels);
    const size_t frame_count = static_cast<size_t>(kSamplesPerFramePerChannel);

    for (size_t i = 0; i < frame_count; ++i) {
        const size_t current_f = (target_frame_idx + i) % total_frames_capacity;
        const size_t base_idx = current_f * static_cast<size_t>(kChannels);

        sample_buffer_[base_idx] = pcm_in[i * 2];
        sample_buffer_[base_idx + 1] = pcm_in[(i * 2) + 1];
        valid_mask_[current_f] = 1;
    }

    return true;
}

TimelineReadResult TimelineBuffer::read_samples(std::span<float> out_pcm, uint64_t current_local_us) noexcept {
    (void)current_local_us;

    if (!is_anchored_.load(std::memory_order_acquire) || out_pcm.empty()) {
        std::ranges::fill(out_pcm, 0.0F);
        return TimelineReadResult::Empty;
    }

    const size_t needed_frames = out_pcm.size() / static_cast<size_t>(kChannels);
    const size_t total_frames_capacity = capacity_samples_ / static_cast<size_t>(kChannels);

    bool has_gap = false;
    for (size_t i = 0; i < needed_frames; ++i) {
        const size_t f = (playhead_sample_index_ + i) % total_frames_capacity;
        const size_t base_idx = f * static_cast<size_t>(kChannels);

        if (valid_mask_[f] == 1) {
            out_pcm[i * 2] = sample_buffer_[base_idx];
            out_pcm[(i * 2) + 1] = sample_buffer_[base_idx + 1];
            valid_mask_[f] = 0;  // Mark consumed
        } else {
            out_pcm[i * 2] = 0.0F;
            out_pcm[(i * 2) + 1] = 0.0F;
            has_gap = true;
        }
    }

    playhead_sample_index_ += needed_frames;

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
    anchor_local_us_ = 0;
    playhead_sample_index_ = 0;
    late_frames_.store(0, std::memory_order_relaxed);
    gaps_.store(0, std::memory_order_relaxed);
    std::ranges::fill(sample_buffer_, 0.0F);
    std::ranges::fill(valid_mask_, 0);
}

}  // namespace chorus
