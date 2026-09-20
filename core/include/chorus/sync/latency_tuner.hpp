#pragma once

#include <chorus/core.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>

namespace chorus {

enum class BitratePreset : uint8_t {
    Resilient,  // 48 kbps, high FEC
    Standard,   // 96 kbps, moderate FEC
    High        // 160 kbps, low FEC
};

struct NetworkQualityAssessment {
    uint64_t recommended_target_latency_ms{kDefaultTargetLatencyMs};
    BitratePreset recommended_preset{BitratePreset::Standard};
    int recommended_bitrate_bps{96000};
    int recommended_fec_expected_loss_pct{5};
    double p95_rtt_ms{0.0};
    double jitter_ms{0.0};
    double average_loss_pct{0.0};
};

/// @brief Adaptive latency auto-tuner and bitrate optimizer based on real-time RTT, jitter, and packet loss telemetry.
class LatencyTuner {
public:
    explicit LatencyTuner(size_t window_size = 50);
    ~LatencyTuner() = default;

    /// @brief Records a round-trip time sample in microseconds.
    void record_rtt_sample(int64_t rtt_us);

    /// @brief Records client packet loss telemetry in percentage (0.0 to 100.0).
    void record_loss_sample(double loss_pct);

    /// @brief Evaluates collected network telemetry and computes recommendations.
    [[nodiscard]] NetworkQualityAssessment evaluate() const;

    /// @brief Resets history.
    void reset();

private:
    size_t window_size_{50};
    std::deque<int64_t> rtt_samples_us_;
    std::deque<double> loss_samples_;
};

}  // namespace chorus
