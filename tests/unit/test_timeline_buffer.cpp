// NOLINTBEGIN
#include <catch2/catch_test_macros.hpp>
#include <chorus/playback/timeline_buffer.hpp>

#include <vector>

TEST_CASE("TimelineBuffer in-order playout scheduling", "[playback][timeline]") {
    chorus::TimelineBuffer buffer(48000);  // 1 second capacity
    REQUIRE_FALSE(buffer.is_active());

    std::vector<float> frame1(chorus::kFloatsPerFrame, 0.25F);
    std::vector<float> frame2(chorus::kFloatsPerFrame, 0.75F);

    // Insert first frame at local microsecond 1,000,000 (1.0s)
    constexpr uint64_t kFirstFrameUs = 1000000ULL;
    REQUIRE(buffer.insert_frame(kFirstFrameUs, frame1));
    REQUIRE(buffer.is_active());

    // Insert second frame at 1,020,000 (1.020s, exactly 20ms later = 960 frames)
    constexpr uint64_t kSecondFrameUs = 1020000ULL;
    REQUIRE(buffer.insert_frame(kSecondFrameUs, frame2));

    // Read first frame
    std::vector<float> out(chorus::kFloatsPerFrame, 0.0F);
    const auto result1 = buffer.read_samples(out, kFirstFrameUs);
    REQUIRE(result1 == chorus::TimelineReadResult::Ok);
    REQUIRE(out[0] == 0.25F);
    REQUIRE(out[1] == 0.25F);

    // Read second frame
    const auto result2 = buffer.read_samples(out, kSecondFrameUs);
    REQUIRE(result2 == chorus::TimelineReadResult::Ok);
    REQUIRE(out[0] == 0.75F);
    REQUIRE(out[1] == 0.75F);
}

TEST_CASE("TimelineBuffer late frame rejection and gap detection", "[playback][timeline]") {
    chorus::TimelineBuffer buffer(48000);

    std::vector<float> frame(chorus::kFloatsPerFrame, 0.5F);
    REQUIRE(buffer.insert_frame(1000000ULL, frame));

    // Read 2 frames (advancing playhead 40 ms past anchor)
    std::vector<float> read_buf(chorus::kFloatsPerFrame * 2);
    const auto read_res = buffer.read_samples(read_buf, 1000000ULL);
    REQUIRE(read_res == chorus::TimelineReadResult::Gap);  // Second frame was missing, gap flagged

    // Now try to insert a frame scheduled at 1,010,000 (10ms past anchor, which is already consumed)
    REQUIRE_FALSE(buffer.insert_frame(1010000ULL, frame));
    REQUIRE(buffer.late_frames_dropped() == 1);
}
// NOLINTEND
