#include <chorus/sync/jitter_buffer.hpp>

namespace chorus {

JitterBuffer::JitterBuffer(size_t max_buffered_frames, size_t prebuffer_frames)
    : max_buffered_frames_(max_buffered_frames),
      prebuffer_frames_(prebuffer_frames) {}

bool JitterBuffer::push(const AudioPacket& packet) {
    total_received_++;

    if (!is_initialized_) {
        JitterFrame frame{
            .seq = packet.seq,
            .play_at_host_us = packet.play_at_host_us,
            .flags = packet.flags,
            .payload = std::vector<uint8_t>(packet.payload.begin(), packet.payload.end())
        };
        queue_[packet.seq] = std::move(frame);

        if (queue_.size() >= prebuffer_frames_) {
            next_expected_seq_ = queue_.begin()->first;
            is_initialized_ = true;
        }
        return true;
    }

    if (packet.seq < next_expected_seq_) {
        duplicate_count_++;
        return false;
    }

    if (queue_.contains(packet.seq)) {
        duplicate_count_++;
        return false;
    }

    if (packet.seq > next_expected_seq_) {
        reordered_count_++;
    }

    JitterFrame frame{
        .seq = packet.seq,
        .play_at_host_us = packet.play_at_host_us,
        .flags = packet.flags,
        .payload = std::vector<uint8_t>(packet.payload.begin(), packet.payload.end())
    };
    queue_[packet.seq] = std::move(frame);

    // Limit buffer capacity
    while (queue_.size() > max_buffered_frames_) {
        queue_.erase(queue_.begin());
    }

    return true;
}

JitterPopResult JitterBuffer::pop(JitterFrame& frame_out) {
    if (!is_initialized_) {
        if (queue_.size() >= prebuffer_frames_) {
            next_expected_seq_ = queue_.begin()->first;
            is_initialized_ = true;
        } else {
            return JitterPopResult::Empty;
        }
    }

    if (queue_.empty()) {
        return JitterPopResult::Empty;
    }

    const auto it = queue_.begin();
    if (it->first == next_expected_seq_) {
        frame_out = std::move(it->second);
        queue_.erase(it);
        next_expected_seq_++;
        return JitterPopResult::Ready;
    }

    if (it->first > next_expected_seq_) {
        // Missing frame in sequence - declare packet loss so caller synthesizes PLC
        lost_plc_count_++;
        next_expected_seq_++;
        return JitterPopResult::LossPlc;
    }

    // In case next_expected_seq_ is somehow ahead of queue head
    queue_.erase(it);
    return pop(frame_out);
}

bool JitterBuffer::is_ready() const noexcept {
    return is_initialized_;
}

size_t JitterBuffer::size() const noexcept {
    return queue_.size();
}

uint64_t JitterBuffer::total_received() const noexcept {
    return total_received_;
}

uint64_t JitterBuffer::reordered_count() const noexcept {
    return reordered_count_;
}

uint64_t JitterBuffer::duplicate_count() const noexcept {
    return duplicate_count_;
}

uint64_t JitterBuffer::lost_plc_count() const noexcept {
    return lost_plc_count_;
}

void JitterBuffer::reset() noexcept {
    queue_.clear();
    is_initialized_ = false;
    next_expected_seq_ = 0;
    total_received_ = 0;
    reordered_count_ = 0;
    duplicate_count_ = 0;
    lost_plc_count_ = 0;
}

}  // namespace chorus
