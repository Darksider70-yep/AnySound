// NOLINTBEGIN
#include <catch2/catch_test_macros.hpp>
#include <chorus/sync/clock_estimator.hpp>

#include <cmath>
#include <random>

TEST_CASE("ClockEstimator ideal network precision", "[sync][clock]") {
    chorus::ClockEstimator estimator;
    REQUIRE_FALSE(estimator.is_synchronized());

    constexpr int64_t kTrueOffsetUs = 500000;   // Host is 500 ms ahead
    constexpr uint64_t kOneWayDelayUs = 10000;  // 10 ms one-way delay

    for (uint32_t i = 1; i <= 10; ++i) {
        const uint64_t t0 = 1000000ULL * i;
        const uint64_t t1 = t0 + kTrueOffsetUs + kOneWayDelayUs;
        const uint64_t t2 = t1 + 500;  // 500 us processing time on host
        const uint64_t t3 = t0 + (2 * kOneWayDelayUs) + 500;

        REQUIRE(estimator.record_sample(i, t0, t1, t2, t3));
    }

    REQUIRE(estimator.is_synchronized());
    REQUIRE(std::abs(estimator.offset_us() - kTrueOffsetUs) < 50);  // Within 50 us
    REQUIRE(estimator.min_rtt_us() == 20000);                      // 20 ms RTT
}

TEST_CASE("ClockEstimator with simulated network jitter and outliers", "[sync][clock]") {
    chorus::ClockEstimator estimator(30, 5);

    constexpr int64_t kTrueOffsetUs = -250000;  // Host is 250 ms behind
    constexpr uint64_t kBaseDelayUs = 8000;     // 8 ms base delay

    std::mt19937 gen(1337);
    std::uniform_int_distribution<int64_t> jitter_dist(0, 15000);  // 0 to 15 ms random jitter

    for (uint32_t i = 1; i <= 25; ++i) {
        const uint64_t t0 = 1000000ULL * i;
        const int64_t jitter1 = (i == 12) ? 150000 : jitter_dist(gen);  // Sample 12 is a huge outlier (150 ms spike)
        const int64_t jitter2 = jitter_dist(gen);

        const uint64_t t1 = static_cast<uint64_t>(static_cast<int64_t>(t0) + kTrueOffsetUs + static_cast<int64_t>(kBaseDelayUs) + jitter1);
        const uint64_t t2 = t1 + 300;
        const uint64_t t3 = static_cast<uint64_t>(static_cast<int64_t>(t0) + static_cast<int64_t>(kBaseDelayUs * 2) + jitter1 + jitter2 + 300);

        estimator.record_sample(i, t0, t1, t2, t3);
    }

    REQUIRE(estimator.is_synchronized());
    // Estimator filters out the lowest RTT samples, so estimated offset should be close to true offset within ~8ms jitter
    const int64_t error = std::abs(estimator.offset_us() - kTrueOffsetUs);
    REQUIRE(error < 10000);  // < 10 ms error on jittery link
}

TEST_CASE("ClockEstimator skew linear regression", "[sync][clock]") {
    chorus::ClockEstimator estimator(40, 5);

    constexpr int64_t kBaseOffsetUs = 100000;
    constexpr double kSimulatedSkewPpm = 60.0;  // 60 ppm clock drift
    constexpr uint64_t kDelayUs = 5000;

    for (uint32_t i = 1; i <= 30; ++i) {
        const uint64_t t0 = 1000000ULL * i;
        // offset drifts over time: base + (t0 * skew_ppm / 1e6)
        const auto drift = static_cast<int64_t>((static_cast<double>(t0) * kSimulatedSkewPpm) / 1e6);
        const int64_t current_offset = kBaseOffsetUs + drift;

        const uint64_t t1 = static_cast<uint64_t>(static_cast<int64_t>(t0) + current_offset + static_cast<int64_t>(kDelayUs));
        const uint64_t t2 = t1 + 200;
        const uint64_t t3 = t0 + (2 * kDelayUs) + 200;

        estimator.record_sample(i, t0, t1, t2, t3);
    }

    REQUIRE(estimator.is_synchronized());
    // Skew estimate should be very close to 60 ppm (within 10 ppm)
    REQUIRE(std::abs(estimator.skew_ppm() - kSimulatedSkewPpm) < 10.0);
}
// NOLINTEND
