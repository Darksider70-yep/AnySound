// NOLINTBEGIN
#include <catch2/catch_test_macros.hpp>
#include <chorus/sync/jitter_buffer.hpp>

TEST_CASE("JitterBuffer in-order push and pop", "[sync][jitter]") {
    chorus::JitterBuffer jb(20, 3);
    REQUIRE_FALSE(jb.is_ready());

    std::vector<uint8_t> p1{0x01, 0x02};
    std::vector<uint8_t> p2{0x03, 0x04};
    std::vector<uint8_t> p3{0x05, 0x06};

    chorus::AudioPacket pkt1{.header = {}, .seq = 10, .play_at_host_us = 1000000, .flags = 0, .payload = p1};
    chorus::AudioPacket pkt2{.header = {}, .seq = 11, .play_at_host_us = 1020000, .flags = 0, .payload = p2};
    chorus::AudioPacket pkt3{.header = {}, .seq = 12, .play_at_host_us = 1040000, .flags = 0, .payload = p3};

    REQUIRE(jb.push(pkt1));
    REQUIRE_FALSE(jb.is_ready());  // Needs 3 frames to prebuffer
    REQUIRE(jb.push(pkt2));
    REQUIRE(jb.push(pkt3));
    REQUIRE(jb.is_ready());
    REQUIRE(jb.size() == 3);

    chorus::JitterFrame frame;
    REQUIRE(jb.pop(frame) == chorus::JitterPopResult::Ready);
    REQUIRE(frame.seq == 10);
    REQUIRE(frame.payload == p1);

    REQUIRE(jb.pop(frame) == chorus::JitterPopResult::Ready);
    REQUIRE(frame.seq == 11);

    REQUIRE(jb.pop(frame) == chorus::JitterPopResult::Ready);
    REQUIRE(frame.seq == 12);

    REQUIRE(jb.pop(frame) == chorus::JitterPopResult::Empty);
}

TEST_CASE("JitterBuffer out-of-order reordering and duplicate rejection", "[sync][jitter]") {
    chorus::JitterBuffer jb(20, 2);

    std::vector<uint8_t> p{0xAA};
    chorus::AudioPacket pkt1{.header = {}, .seq = 1, .play_at_host_us = 1000, .flags = 0, .payload = p};
    chorus::AudioPacket pkt2{.header = {}, .seq = 2, .play_at_host_us = 2000, .flags = 0, .payload = p};
    chorus::AudioPacket pkt3{.header = {}, .seq = 3, .play_at_host_us = 3000, .flags = 0, .payload = p};

    // Arrive out of order: 2, then 1, then 3
    REQUIRE(jb.push(pkt2));
    REQUIRE(jb.push(pkt1));
    REQUIRE(jb.push(pkt3));
    REQUIRE(jb.reordered_count() > 0);

    // Duplicate push of pkt2
    REQUIRE_FALSE(jb.push(pkt2));
    REQUIRE(jb.duplicate_count() == 1);

    chorus::JitterFrame frame;
    REQUIRE(jb.pop(frame) == chorus::JitterPopResult::Ready);
    REQUIRE(frame.seq == 1);

    REQUIRE(jb.pop(frame) == chorus::JitterPopResult::Ready);
    REQUIRE(frame.seq == 2);

    REQUIRE(jb.pop(frame) == chorus::JitterPopResult::Ready);
    REQUIRE(frame.seq == 3);
}

TEST_CASE("JitterBuffer missing packet loss PLC detection", "[sync][jitter]") {
    chorus::JitterBuffer jb(20, 2);

    std::vector<uint8_t> p{0xBB};
    chorus::AudioPacket pkt1{.header = {}, .seq = 100, .play_at_host_us = 1000, .flags = 0, .payload = p};
    // Packet 101 is lost in transit!
    chorus::AudioPacket pkt3{.header = {}, .seq = 102, .play_at_host_us = 3000, .flags = 0, .payload = p};

    REQUIRE(jb.push(pkt1));
    REQUIRE(jb.push(pkt3));

    chorus::JitterFrame frame;
    REQUIRE(jb.pop(frame) == chorus::JitterPopResult::Ready);
    REQUIRE(frame.seq == 100);

    // Next pop sees seq 101 is missing, returns LossPlc
    REQUIRE(jb.pop(frame) == chorus::JitterPopResult::LossPlc);
    REQUIRE(jb.lost_plc_count() == 1);

    // After PLC, next pop gives seq 102
    REQUIRE(jb.pop(frame) == chorus::JitterPopResult::Ready);
    REQUIRE(frame.seq == 102);
}
// NOLINTEND
