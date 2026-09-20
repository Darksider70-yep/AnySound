#include <chorus/sync/latency_tuner.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace chorus;

TEST_CASE("LatencyTuner RTT jitter and safety headroom adaptation", "[latency_tuner]") {
    LatencyTuner tuner(30);

    // Feed steady 10 ms RTT with minor +/- 2 ms jitter
    for (int i = 0; i < 30; ++i) {
        int64_t rtt_us = 10000 + ((i % 5) - 2) * 1000;
        tuner.record_rtt_sample(rtt_us);
    }

    auto assessment = tuner.evaluate();
    REQUIRE(assessment.p95_rtt_ms > 8.0);
    REQUIRE(assessment.p95_rtt_ms < 15.0);
    // Recommended latency should be clamped within bounds and include safety headroom
    REQUIRE(assessment.recommended_target_latency_ms >= 100);
    REQUIRE(assessment.recommended_target_latency_ms <= 300);
}

TEST_CASE("LatencyTuner Bitrate adaptation based on packet loss", "[latency_tuner]") {
    LatencyTuner tuner(20);

    // 1. Clean link: 0% loss -> High preset (160 kbps)
    for (int i = 0; i < 20; ++i) {
        tuner.record_loss_sample(0.0);
    }
    auto assessment_clean = tuner.evaluate();
    REQUIRE(assessment_clean.recommended_preset == BitratePreset::High);
    REQUIRE(assessment_clean.recommended_bitrate_bps == 160000);

    // 2. Moderate loss: 3% loss -> Standard preset (96 kbps)
    tuner.reset();
    for (int i = 0; i < 20; ++i) {
        tuner.record_loss_sample(3.0);
    }
    auto assessment_mod = tuner.evaluate();
    REQUIRE(assessment_mod.recommended_preset == BitratePreset::Standard);
    REQUIRE(assessment_mod.recommended_bitrate_bps == 96000);

    // 3. Heavy loss: 10% loss -> Resilient preset (48 kbps)
    tuner.reset();
    for (int i = 0; i < 20; ++i) {
        tuner.record_loss_sample(10.0);
    }
    auto assessment_lossy = tuner.evaluate();
    REQUIRE(assessment_lossy.recommended_preset == BitratePreset::Resilient);
    REQUIRE(assessment_lossy.recommended_bitrate_bps == 48000);
    REQUIRE(assessment_lossy.recommended_fec_expected_loss_pct >= 10);
}
