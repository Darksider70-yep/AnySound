#include <chorus/sync/clock_estimator.hpp>

#include <algorithm>
#include <numeric>

namespace chorus {

ClockEstimator::ClockEstimator(size_t window_size, size_t min_samples_for_sync)
    : window_size_(window_size),
      min_samples_for_sync_(min_samples_for_sync) {}

bool ClockEstimator::record_sample(uint32_t ping_id,
                                   uint64_t t0,
                                   uint64_t t1,
                                   uint64_t t2,
                                   uint64_t t3) noexcept {
    if (t3 < t0 || t2 < t1) {
        return false;
    }

    const uint64_t total_roundtrip = t3 - t0;
    const uint64_t host_processing = t2 - t1;
    if (total_roundtrip < host_processing) {
        return false;
    }

    const auto rtt = static_cast<int64_t>(total_roundtrip - host_processing);

    // Compute offset: ((t1 - t0) + (t2 - t3)) / 2
    const auto diff1 = static_cast<int64_t>(t1) - static_cast<int64_t>(t0);
    const auto diff2 = static_cast<int64_t>(t2) - static_cast<int64_t>(t3);
    const int64_t offset = (diff1 + diff2) / 2;

    ClockSample sample;
    sample.ping_id = ping_id;
    sample.t0 = t0;
    sample.t1 = t1;
    sample.t2 = t2;
    sample.t3 = t3;
    sample.offset_us = offset;
    sample.rtt_us = rtt;

    samples_.push_back(sample);
    if (samples_.size() > window_size_) {
        samples_.pop_front();
    }
    total_samples_received_++;

    update_estimates();
    return true;
}

void ClockEstimator::update_estimates() noexcept {
    if (samples_.empty()) {
        return;
    }

    int64_t min_rtt = samples_.front().rtt_us;
    int64_t total_rtt = 0;
    for (const auto& s : samples_) {
        min_rtt = std::min(min_rtt, s.rtt_us);
        total_rtt += s.rtt_us;
    }
    min_rtt_us_ = min_rtt;
    avg_rtt_us_ = total_rtt / static_cast<int64_t>(samples_.size());

    // Select lowest RTT samples (top 50% lowest delay) for robust offset estimation
    std::vector<ClockSample> sorted_samples(samples_.begin(), samples_.end());
    std::ranges::sort(sorted_samples, [](const ClockSample& a, const ClockSample& b) {
        return a.rtt_us < b.rtt_us;
    });

    const size_t keep_count = std::max<size_t>(1, sorted_samples.size() / 2);
    int64_t offset_sum = 0;
    for (size_t i = 0; i < keep_count; ++i) {
        offset_sum += sorted_samples[i].offset_us;
    }
    estimated_offset_us_ = offset_sum / static_cast<int64_t>(keep_count);

    // Compute clock skew via linear regression of offset over local time t0 if >= 5 samples
    if (samples_.size() >= min_samples_for_sync_) {
        double sum_x = 0.0;
        double sum_y = 0.0;
        const auto base_x = static_cast<double>(samples_.front().t0);

        for (const auto& s : samples_) {
            const double x = static_cast<double>(s.t0) - base_x;
            const double y = static_cast<double>(s.offset_us);
            sum_x += x;
            sum_y += y;
        }

        const auto count = static_cast<double>(samples_.size());
        const double mean_x = sum_x / count;
        const double mean_y = sum_y / count;

        double numerator = 0.0;
        double denominator = 0.0;

        for (const auto& s : samples_) {
            const double x = (static_cast<double>(s.t0) - base_x) - mean_x;
            const double y = static_cast<double>(s.offset_us) - mean_y;
            numerator += x * y;
            denominator += x * x;
        }

        if (denominator > 1e-6) {
            const double slope = numerator / denominator;
            estimated_skew_ppm_ = slope * 1e6;
        }
    }
}

bool ClockEstimator::is_synchronized() const noexcept {
    return samples_.size() >= min_samples_for_sync_;
}

int64_t ClockEstimator::offset_us() const noexcept {
    return estimated_offset_us_;
}

double ClockEstimator::skew_ppm() const noexcept {
    return estimated_skew_ppm_;
}

int64_t ClockEstimator::min_rtt_us() const noexcept {
    return min_rtt_us_;
}

int64_t ClockEstimator::avg_rtt_us() const noexcept {
    return avg_rtt_us_;
}

size_t ClockEstimator::sample_count() const noexcept {
    return samples_.size();
}

uint64_t ClockEstimator::host_to_local_us(uint64_t host_us) const noexcept {
    const auto result = static_cast<int64_t>(host_us) - estimated_offset_us_;
    return (result > 0) ? static_cast<uint64_t>(result) : 0ULL;
}

uint64_t ClockEstimator::local_to_host_us(uint64_t local_us) const noexcept {
    const auto result = static_cast<int64_t>(local_us) + estimated_offset_us_;
    return (result > 0) ? static_cast<uint64_t>(result) : 0ULL;
}

void ClockEstimator::reset() noexcept {
    samples_.clear();
    estimated_offset_us_ = 0;
    estimated_skew_ppm_ = 0.0;
    min_rtt_us_ = 0;
    avg_rtt_us_ = 0;
    total_samples_received_ = 0;
}

}  // namespace chorus
