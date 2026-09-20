#include <catch2/catch_test_macros.hpp>
#include <chorus/codec/opus_codec.hpp>
#include <chorus/sync/drift_controller.hpp>
#include <chorus/sync/jitter_buffer.hpp>
#include <chorus/sync/resampler.hpp>

#include <random>
#include <vector>

TEST_CASE("Fault Injection: 5% packet loss with Opus PLC recovery", "[sync][fault]") {
    chorus::OpusEncoderWrap encoder;
    REQUIRE(encoder.init(96000, 5));

    chorus::OpusDecoderWrap decoder;
    REQUIRE(decoder.init());

    chorus::JitterBuffer jb(30, 3);

    std::vector<float> pcm_in(chorus::kFloatsPerFrame, 0.3F);
    std::vector<uint8_t> opus_buf(chorus::kMaxOpusPayloadBytes);

    std::mt19937 gen(42);
    std::uniform_real_distribution<double> loss_dist(0.0, 1.0);

    constexpr uint32_t kTotalFrames = 200;
    uint32_t frames_sent = 0;
    uint32_t frames_dropped_by_network = 0;

    for (uint32_t seq = 1; seq <= kTotalFrames; ++seq) {
        const int bytes = encoder.encode(pcm_in, opus_buf);
        REQUIRE(bytes > 0);

        // 5% simulated loss
        if (loss_dist(gen) < 0.05) {
            frames_dropped_by_network++;
            continue;
        }

        const chorus::AudioPacket pkt{
            .header = {},
            .seq = seq,
            .play_at_host_us = 1000000ULL + (static_cast<uint64_t>(seq) * 20000ULL),
            .flags = 0,
            .payload = std::span<const uint8_t>(opus_buf.data(), static_cast<size_t>(bytes))
        };
        jb.push(pkt);
        frames_sent++;
    }

    REQUIRE(frames_dropped_by_network > 0);

    // Consume from JitterBuffer and decode (using PLC on loss)
    std::vector<float> pcm_decoded(chorus::kFloatsPerFrame);
    uint32_t plc_count = 0;
    uint32_t valid_count = 0;

    chorus::JitterFrame frame;
    while (true) {
        const auto res = jb.pop(frame);
        if (res == chorus::JitterPopResult::Empty) {
            break;
        }

        if (res == chorus::JitterPopResult::LossPlc) {
            const int samples = decoder.decode_plc(pcm_decoded);
            REQUIRE(samples == chorus::kSamplesPerFramePerChannel);
            plc_count++;
        } else if (res == chorus::JitterPopResult::Ready) {
            const int samples = decoder.decode(frame.payload, pcm_decoded);
            REQUIRE(samples == chorus::kSamplesPerFramePerChannel);
            valid_count++;
        }
    }

    REQUIRE(plc_count > 0);
    REQUIRE(valid_count + plc_count == frames_sent + frames_dropped_by_network);
}

TEST_CASE("Fault Injection: 2-second outage recovery", "[sync][fault]") {
    chorus::DriftController drift;

    // Normal operation (phase error < 1ms)
    drift.update(500, 10.0, 0.02);
    REQUIRE_FALSE(drift.needs_hard_resync());

    // 2-second network blackout causes large timeline phase lag (2,000,000 us = 2000 ms)
    drift.update(2000000, 10.0, 2.0);
    REQUIRE(drift.needs_hard_resync());

    // System detects outage, performs hard resync within < 3 s, and resets state smoothly
    drift.clear_resync();
    REQUIRE_FALSE(drift.needs_hard_resync());
    REQUIRE(drift.resampler_ratio() == 1.0);
}
// NOLINTEND
