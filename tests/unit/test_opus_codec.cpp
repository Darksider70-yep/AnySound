#include <catch2/catch_test_macros.hpp>
#include <chorus/codec/opus_codec.hpp>

#include <cmath>
#include <numbers>
#include <vector>

TEST_CASE("Opus codec initialization and round-trip encode-decode", "[codec][opus]") {
    chorus::OpusEncoderWrap encoder;
    REQUIRE(encoder.init(96000, 5));

    chorus::OpusDecoderWrap decoder;
    REQUIRE(decoder.init());

    // Generate a 440 Hz test sine wave for 5 frames (100ms) to allow Opus filter warmup
    constexpr size_t kNumFrames = 5;
    std::vector<float> pcm_out(chorus::kFloatsPerFrame);
    std::vector<uint8_t> payload(chorus::kMaxOpusPayloadBytes);

    float total_energy_in = 0.0f;
    float total_energy_out = 0.0f;

    for (size_t f = 0; f < kNumFrames; ++f) {
        std::vector<float> pcm_in(chorus::kFloatsPerFrame);
        for (size_t i = 0; i < chorus::kSamplesPerFramePerChannel; ++i) {
            const size_t global_sample = f * chorus::kSamplesPerFramePerChannel + i;
            const float sample = std::sin(2.0f * std::numbers::pi_v<float> * 440.0f * static_cast<float>(global_sample) / static_cast<float>(chorus::kSampleRate));
            pcm_in[i * 2] = sample;      // Left
            pcm_in[i * 2 + 1] = sample;  // Right
            total_energy_in += sample * sample;
        }

        int bytes_encoded = encoder.encode(pcm_in, payload);
        REQUIRE(bytes_encoded > 0);
        REQUIRE(bytes_encoded <= static_cast<int>(chorus::kMaxOpusPayloadBytes));

        int samples_decoded = decoder.decode(std::span<const uint8_t>(payload.data(), static_cast<size_t>(bytes_encoded)), pcm_out);
        REQUIRE(samples_decoded == chorus::kSamplesPerFramePerChannel);

        for (size_t i = 0; i < chorus::kFloatsPerFrame; i += 2) {
            total_energy_out += pcm_out[i] * pcm_out[i];
        }
    }

    // Verify energy preservation within 15% after codec warmup
    const float energy_ratio = total_energy_out / total_energy_in;
    REQUIRE(energy_ratio > 0.70f);
    REQUIRE(energy_ratio < 1.30f);
}

TEST_CASE("Opus Packet Loss Concealment (PLC)", "[codec][opus]") {
    chorus::OpusEncoderWrap encoder;
    REQUIRE(encoder.init());

    chorus::OpusDecoderWrap decoder;
    REQUIRE(decoder.init());

    std::vector<float> pcm_in(chorus::kFloatsPerFrame, 0.5f);
    std::vector<uint8_t> payload(chorus::kMaxOpusPayloadBytes);
    int bytes = encoder.encode(pcm_in, payload);
    REQUIRE(bytes > 0);

    // First normal decode
    std::vector<float> pcm_out(chorus::kFloatsPerFrame);
    REQUIRE(decoder.decode(std::span<const uint8_t>(payload.data(), static_cast<size_t>(bytes)), pcm_out) == chorus::kSamplesPerFramePerChannel);

    // Conceal lost packet with empty payload (PLC)
    std::vector<float> plc_out(chorus::kFloatsPerFrame);
    int plc_samples = decoder.decode(std::span<const uint8_t>{}, plc_out);
    REQUIRE(plc_samples == chorus::kSamplesPerFramePerChannel);
}
