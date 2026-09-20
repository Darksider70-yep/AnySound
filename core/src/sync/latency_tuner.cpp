#include <chorus/sync/latency_tuner.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace chorus {

LatencyTuner::LatencyTuner(size_t window_size)
    : window_size_(window_size > 5 ? window_size : 5) {}

void LatencyTuner::record_rtt_sample(int64_t rtt_us) {
    if (rtt_us < 0) {
        return;
    }
    rtt_samples_us_.push_back(rtt_us);
    while (rtt_samples_us_.size() > window_size_) {
        rtt_samples_us_.pop_front();
    }
}

void LatencyTuner::record_loss_sample(double loss_pct) {
    if (loss_pct < 0.0) {
        loss_pct = 0.0;
    } else if (loss_pct > 100.0) {
        loss_pct = 100.0;
    }
    loss_samples_.push_back(loss_pct);
    while (loss_samples_.size() > window_size_) {
        loss_samples_.pop_front();
    }
}

void LatencyTuner::reset() {
    rtt_samples_us_.clear();
    loss_samples_.clear();
}

NetworkQualityAssessment LatencyTuner::evaluate() const {
    NetworkQualityAssessment result{};

    // 1. Evaluate Loss Statistics
    double avg_loss = 0.0;
    if (!loss_samples_.empty()) {
        double sum = std::accumulate(loss_samples_.begin(), loss_samples_.end(), 0.0);
        avg_loss = sum / static_cast<double>(loss_samples_.size());
    }
    result.average_loss_pct = avg_loss;

    // Determine Bitrate and FEC Preset
    if (avg_loss < 1.0) {
        result.recommended_preset = BitratePreset::High;
        result.recommended_bitrate_bps = 160000;
        result.recommended_fec_expected_loss_pct = 2;
    } else if (avg_loss <= 5.0) {
        result.recommended_preset = BitratePreset::Standard;
        result.recommended_bitrate_bps = 96000;
        result.recommended_fec_expected_loss_pct = 5;
    } else {
        result.recommended_preset = BitratePreset::Resilient;
        result.recommended_bitrate_bps = 48000;
        result.recommended_fec_expected_loss_pct = static_cast<int>(std::min(avg_loss * 1.5, 25.0));
    }

    // 2. Evaluate RTT & Jitter Statistics
    if (rtt_samples_us_.empty()) {
        result.recommended_target_latency_ms = kDefaultTargetLatencyMs;
        result.p95_rtt_ms = 0.0;
        result.jitter_ms = 0.0;
        return result;
    }

    std::vector<int64_t> sorted_rtt(rtt_samples_us_.begin(), rtt_samples_us_.end());
    std::sort(sorted_rtt.begin(), sorted_rtt.end());

    // 95th percentile RTT
    size_t p95_idx = static_cast<size_t>(static_cast<double>(sorted_rtt.size() - 1) * 0.95);
    double p95_rtt_ms = static_cast<double>(sorted_rtt[p95_idx]) / 1000.0;
    result.p95_rtt_ms = p95_rtt_ms;

    // Calculate mean RTT
    double rtt_sum = 0.0;
    for (int64_t val : sorted_rtt) {
        rtt_sum += static_cast<double>(val) / 1000.0;
    }
    double mean_rtt_ms = rtt_sum / static_cast<double>(sorted_rtt.size());

    // Calculate standard deviation (jitter)
    double variance_sum = 0.0;
    for (int64_t val : sorted_rtt) {
        double diff = (static_cast<double>(val) / 1000.0) - mean_rtt_ms;
        variance_sum += diff * diff;
    }
    double jitter_stddev_ms = std::sqrt(variance_sum / static_cast<double>(sorted_rtt.size()));
    result.jitter_ms = jitter_stddev_ms;

    // Target latency = p95_RTT + 3 * jitter + 40ms safety headroom (clamped 100ms - 1500ms)
    double calculated_latency_ms = p95_rtt_ms + (3.0 * jitter_stddev_ms) + 40.0;
    uint64_t target_clamped = static_cast<uint64_t>(std::clamp(calculated_latency_ms, 100.0, 1500.0));

    // Quantize to nearest 10 ms
    target_clamped = ((target_clamped + 5) / 10) * 10;
    result.recommended_target_latency_ms = target_clamped;

    return result;
}

}  // namespace chorus
