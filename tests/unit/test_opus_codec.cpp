#include <catch2/catch_test_macros.hpp>
#include <chorus/codec/opus_codec.hpp>

#include <cmath>
#include <numbers>
#include <vector>

namespace {
constexpr float kSineFreqHz = 440.0F;
constexpr size_t kWarmupFrameCount = 5;
constexpr float kMinEnergyRatio = 0.70F;
constexpr float kMaxEnergyRatio = 1.30F;
}  // namespace

TEST_CASE("Opus codec initialization and round-trip encode-decode", "[codec][opus]") {
    chorus::OpusEncoderWrap encoder;
    REQUIRE(encoder.init(chorus::kDefaultBitrate, chorus::kDefaultExpectedLossPct));

    chorus::OpusDecoderWrap decoder;
    REQUIRE(decoder.init());

    std::vector<float> pcm_out(static_cast<size_t>(chorus::kFloatsPerFrame));
    std::vector<uint8_t> payload(chorus::kMaxOpusPayloadBytes);

    float total_energy_in = 0.0F;
    float total_energy_out = 0.0F;

    for (size_t frame_idx = 0; frame_idx < kWarmupFrameCount; ++frame_idx) {
        std::vector<float> pcm_in(static_cast<size_t>(chorus::kFloatsPerFrame));
        for (size_t i = 0; i < static_cast<size_t>(chorus::kSamplesPerFramePerChannel); ++i) {
            const size_t global_sample = (frame_idx * static_cast<size_t>(chorus::kSamplesPerFramePerChannel)) + i;
            const auto sample_val = static_cast<float>(std::sin((2.0 * std::numbers::pi * static_cast<double>(kSineFreqHz) * static_cast<double>(global_sample)) / static_cast<double>(chorus::kSampleRate)));
            pcm_in[i * 2] = sample_val;
            pcm_in[(i * 2) + 1] = sample_val;
            total_energy_in += (sample_val * sample_val);
        }

        const int bytes_encoded = encoder.encode(pcm_in, payload);
        REQUIRE(bytes_encoded > 0);
        REQUIRE(bytes_encoded <= static_cast<int>(chorus::kMaxOpusPayloadBytes));

        const int samples_decoded = decoder.decode(std::span<const uint8_t>(payload.data(), static_cast<size_t>(bytes_encoded)), pcm_out);
        REQUIRE(samples_decoded == chorus::kSamplesPerFramePerChannel);

        for (size_t i = 0; i < static_cast<size_t>(chorus::kFloatsPerFrame); i += 2) {
            total_energy_out += (pcm_out[i] * pcm_out[i]);
        }
    }

    const float energy_ratio = total_energy_out / total_energy_in;
    REQUIRE(energy_ratio > kMinEnergyRatio);
    REQUIRE(energy_ratio < kMaxEnergyRatio);
}

TEST_CASE("Opus Packet Loss Concealment (PLC)", "[codec][opus]") {
    chorus::OpusEncoderWrap encoder;
    REQUIRE(encoder.init());

    chorus::OpusDecoderWrap decoder;
    REQUIRE(decoder.init());

    const std::vector<float> pcm_in(static_cast<size_t>(chorus::kFloatsPerFrame), 0.5F);
    std::vector<uint8_t> payload(chorus::kMaxOpusPayloadBytes);
    const int bytes = encoder.encode(pcm_in, payload);
    REQUIRE(bytes > 0);

    // First normal decode
    std::vector<float> pcm_out(static_cast<size_t>(chorus::kFloatsPerFrame));
    REQUIRE(decoder.decode(std::span<const uint8_t>(payload.data(), static_cast<size_t>(bytes)), pcm_out) == chorus::kSamplesPerFramePerChannel);

    // Conceal lost packet with empty payload (PLC)
    std::vector<float> plc_out(static_cast<size_t>(chorus::kFloatsPerFrame));
    const int plc_samples = decoder.decode(std::span<const uint8_t>{}, plc_out);
    REQUIRE(plc_samples == chorus::kSamplesPerFramePerChannel);
}
