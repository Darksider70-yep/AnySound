#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace chorus {

/// @brief Represents a single NTP-style PING/PONG timing sample.
struct ClockSample {
    uint32_t ping_id{0};
    uint64_t t0{0};  // Client send time (local us)
    uint64_t t1{0};  // Host receive time (host us)
    uint64_t t2{0};  // Host send time (host us)
    uint64_t t3{0};  // Client receive time (local us)

    int64_t offset_us{0};  // ((t1 - t0) + (t2 - t3)) / 2
    int64_t rtt_us{0};     // (t3 - t0) - (t2 - t1)
};

/// @brief Pure NTP-style clock offset and clock skew estimator.
/// Thread-safe for usage on client network/worker thread.
/// Zero OS/socket dependencies (deterministic and testable against simulated networks).
class ClockEstimator {
public:
    explicit ClockEstimator(size_t window_size = 20, size_t min_samples_for_sync = 5);
    ~ClockEstimator() = default;

    /// @brief Records a completed PING/PONG round-trip measurement.
    /// @return True if sample was accepted (valid RTT > 0), false if dropped.
    bool record_sample(uint32_t ping_id, uint64_t t0, uint64_t t1, uint64_t t2, uint64_t t3) noexcept;

    /// @brief Returns true if enough consistent samples have been collected.
    [[nodiscard]] bool is_synchronized() const noexcept;

    /// @brief Returns current estimated clock offset (Host time - Local time) in microseconds.
    [[nodiscard]] int64_t offset_us() const noexcept;

    /// @brief Returns current estimated clock skew in parts-per-million (ppm).
    [[nodiscard]] double skew_ppm() const noexcept;

    /// @brief Returns minimum RTT observed in current window in microseconds.
    [[nodiscard]] int64_t min_rtt_us() const noexcept;

    /// @brief Returns average RTT observed in current window in microseconds.
    [[nodiscard]] int64_t avg_rtt_us() const noexcept;

    /// @brief Returns total valid samples accepted.
    [[nodiscard]] size_t sample_count() const noexcept;

    /// @brief Converts a host timestamp to estimated local steady_clock microseconds.
    [[nodiscard]] uint64_t host_to_local_us(uint64_t host_us) const noexcept;

    /// @brief Converts a local steady_clock timestamp to estimated host microseconds.
    [[nodiscard]] uint64_t local_to_host_us(uint64_t local_us) const noexcept;

    /// @brief Resets estimator history.
    void reset() noexcept;

private:
    void update_estimates() noexcept;

    const size_t window_size_;
    const size_t min_samples_for_sync_;

    std::deque<ClockSample> samples_;
    int64_t estimated_offset_us_{0};
    double estimated_skew_ppm_{0.0};
    int64_t min_rtt_us_{0};
    int64_t avg_rtt_us_{0};
    size_t total_samples_received_{0};
};

}  // namespace chorus
