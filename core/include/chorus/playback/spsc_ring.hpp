#pragma once

#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

namespace chorus {

/// @brief Thread-safe Single-Producer Single-Consumer (SPSC) lock-free ring buffer.
/// Safe for use in hard real-time audio callbacks (zero allocation, zero blocking).
///
/// Thread affinity:
/// - One producer thread calls push / write.
/// - One consumer thread calls pop / read.
template <typename T>
class SpscRing {
public:
    explicit SpscRing(size_t capacity)
        : capacity_(capacity + 1),  // 1 slot kept empty to distinguish full from empty
          buffer_(capacity + 1),
          head_(0),
          tail_(0) {}

    ~SpscRing() = default;

    // Non-copyable and non-movable for real-time safety
    SpscRing(const SpscRing&) = delete;
    SpscRing& operator=(const SpscRing&) = delete;
    SpscRing(SpscRing&&) = delete;
    SpscRing& operator=(SpscRing&&) = delete;

    /// @brief Returns the maximum number of items that can be stored.
    [[nodiscard]] size_t capacity() const noexcept {
        return capacity_ - 1;
    }

    /// @brief Returns the number of items available to read.
    /// Thread affinity: Consumer or Producer (approximate if called by producer).
    [[nodiscard]] size_t size() const noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        if (head >= tail) {
            return head - tail;
        }
        return capacity_ + head - tail;
    }

    /// @brief Returns the available write space in the buffer.
    /// Thread affinity: Producer.
    [[nodiscard]] size_t available_write() const noexcept {
        return capacity() - size();
    }

    /// @brief Writes a contiguous slice of items into the ring buffer.
    /// @param src The source span to copy from.
    /// @return The actual number of elements successfully written.
    /// Thread affinity: Producer only.
    size_t write(std::span<const T> src) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        // Acquire tail from consumer to know how much free space is available
        const size_t tail = tail_.load(std::memory_order_acquire);

        size_t free_space = 0;
        if (head >= tail) {
            free_space = (capacity_ - 1) - (head - tail);
        } else {
            free_space = tail - head - 1;
        }

        const size_t count_to_write = (src.size() < free_space) ? src.size() : free_space;
        if (count_to_write == 0) {
            return 0;
        }

        const size_t first_chunk = (count_to_write < capacity_ - head) ? count_to_write : (capacity_ - head);
        const size_t second_chunk = count_to_write - first_chunk;

        for (size_t i = 0; i < first_chunk; ++i) {
            buffer_[head + i] = src[i];
        }
        for (size_t i = 0; i < second_chunk; ++i) {
            buffer_[i] = src[first_chunk + i];
        }

        const size_t next_head = (head + count_to_write) % capacity_;
        // Release write: ensures buffer writes are visible to consumer before updating head
        head_.store(next_head, std::memory_order_release);
        return count_to_write;
    }

    /// @brief Reads a contiguous slice of items from the ring buffer into dest.
    /// @param dest The destination span to fill.
    /// @return The actual number of elements successfully read.
    /// Thread affinity: Consumer only.
    size_t read(std::span<T> dest) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        // Acquire head from producer to know how many elements are ready
        const size_t head = head_.load(std::memory_order_acquire);

        size_t available = 0;
        if (head >= tail) {
            available = head - tail;
        } else {
            available = capacity_ + head - tail;
        }

        const size_t count_to_read = (dest.size() < available) ? dest.size() : available;
        if (count_to_read == 0) {
            return 0;
        }

        const size_t first_chunk = (count_to_read < capacity_ - tail) ? count_to_read : (capacity_ - tail);
        const size_t second_chunk = count_to_read - first_chunk;

        for (size_t i = 0; i < first_chunk; ++i) {
            dest[i] = buffer_[tail + i];
        }
        for (size_t i = 0; i < second_chunk; ++i) {
            dest[first_chunk + i] = buffer_[i];
        }

        const size_t next_tail = (tail + count_to_read) % capacity_;
        // Release read: ensures consumer is done reading slots before updating tail
        tail_.store(next_tail, std::memory_order_release);
        return count_to_read;
    }

    /// @brief Clears the ring buffer by aligning tail to head.
    /// Thread affinity: Consumer or during initialization.
    void reset() noexcept {
        tail_.store(head_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

private:
    const size_t capacity_;
    std::vector<T> buffer_;

    // Align to separate cache lines (64 bytes) to prevent false sharing
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};

}  // namespace chorus
