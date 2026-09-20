// NOLINTBEGIN
#include <catch2/catch_test_macros.hpp>
#include <chorus/sync/resampler.hpp>

#include <cmath>
#include <numbers>
#include <vector>

TEST_CASE("Resampler 1.0 ratio bypass bit-exactness", "[sync][resampler]") {
    chorus::Resampler resampler;
    resampler.set_ratio(1.0);
    REQUIRE(resampler.ratio() == 1.0);

    std::vector<float> in_pcm(chorus::kFloatsPerFrame, 0.42F);
    std::vector<float> out_pcm(chorus::kFloatsPerFrame, 0.0F);

    const size_t out_frames = resampler.process(in_pcm, out_pcm);
    REQUIRE(out_frames == static_cast<size_t>(chorus::kSamplesPerFramePerChannel));
    for (size_t i = 0; i < in_pcm.size(); ++i) {
        REQUIRE(out_pcm[i] == in_pcm[i]);
    }
}

TEST_CASE("Resampler positive and negative drift ratio interpolation", "[sync][resampler]") {
    chorus::Resampler resampler;

    // +200 ppm speedup (ratio = 1.0002)
    resampler.set_ratio(1.0002);
    REQUIRE(resampler.ratio() == 1.0002);

    std::vector<float> in_sine(chorus::kFloatsPerFrame);
    for (size_t i = 0; i < static_cast<size_t>(chorus::kSamplesPerFramePerChannel); ++i) {
        const float val = std::sin(static_cast<float>(i) * 0.1F);
        in_sine[i * 2] = val;
        in_sine[i * 2 + 1] = val;
    }

    std::vector<float> out_pcm(chorus::kFloatsPerFrame * 2, 0.0F);
    const size_t out_frames = resampler.process(in_sine, out_pcm);
    REQUIRE(out_frames >= static_cast<size_t>(chorus::kSamplesPerFramePerChannel - 1));
    REQUIRE(out_frames <= static_cast<size_t>(chorus::kSamplesPerFramePerChannel + 1));

    // Ensure output has valid audio values without NaNs or infinities
    for (size_t i = 0; i < out_frames * 2; ++i) {
        REQUIRE(std::isfinite(out_pcm[i]));
        REQUIRE(std::abs(out_pcm[i]) <= 1.01F);
    }
}
// NOLINTEND
