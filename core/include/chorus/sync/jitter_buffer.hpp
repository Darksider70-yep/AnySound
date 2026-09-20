#pragma once

#include <chorus/net/udp_socket.hpp>
#include <chorus/proto/packet.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace chorus {

/// @brief Result of popping a frame from JitterBuffer.
enum class JitterPopResult {
    Ready,       ///< Next in-order frame is ready
    LossPlc,     ///< Packet was lost or missed deadline; caller should run Opus PLC
    Empty        ///< Buffer is empty / waiting for initial prebuffer
};

/// @brief Item stored in JitterBuffer.
struct JitterFrame {
    uint32_t seq{0};
    uint64_t play_at_host_us{0};
    uint8_t flags{0};
    std::vector<uint8_t> payload;
};

/// @brief Pure sequence-indexed jitter buffer for out-of-order reordering and loss detection.
/// Pure logic (zero OS/socket dependencies) for deterministic unit testing.
class JitterBuffer {
public:
    explicit JitterBuffer(size_t max_buffered_frames = 50, size_t prebuffer_frames = 5);
    ~JitterBuffer() = default;

    JitterBuffer(const JitterBuffer&) = delete;
    JitterBuffer& operator=(const JitterBuffer&) = delete;
    JitterBuffer(JitterBuffer&&) noexcept = default;
    JitterBuffer& operator=(JitterBuffer&&) noexcept = default;

    /// @brief Inserts an incoming AudioPacket into the jitter buffer.
    /// @return True if accepted, false if duplicate or discarded as too old.
    bool push(const AudioPacket& packet);

    /// @brief Attempts to retrieve the next in-sequence audio frame.
    /// @param frame_out Receives the audio frame data if JitterPopResult::Ready.
    /// @return Pop result status (Ready, LossPlc, Empty).
    JitterPopResult pop(JitterFrame& frame_out);

    /// @brief Returns true if buffer has received initial packets and is active.
    [[nodiscard]] bool is_ready() const noexcept;

    /// @brief Returns number of buffered frames currently queued.
    [[nodiscard]] size_t size() const noexcept;

    /// @brief Total packets received.
    [[nodiscard]] uint64_t total_received() const noexcept;

    /// @brief Total out-of-order packets successfully reordered.
    [[nodiscard]] uint64_t reordered_count() const noexcept;

    /// @brief Total duplicate packets dropped.
    [[nodiscard]] uint64_t duplicate_count() const noexcept;

    /// @brief Total missing sequence gaps flagged as lost (triggering PLC).
    [[nodiscard]] uint64_t lost_plc_count() const noexcept;

    /// @brief Resets jitter buffer state.
    void reset() noexcept;

private:
    const size_t max_buffered_frames_;
    const size_t prebuffer_frames_;

    std::map<uint32_t, JitterFrame> queue_;
    bool is_initialized_{false};
    uint32_t next_expected_seq_{0};

    uint64_t total_received_{0};
    uint64_t reordered_count_{0};
    uint64_t duplicate_count_{0};
    uint64_t lost_plc_count_{0};
};

}  // namespace chorus
