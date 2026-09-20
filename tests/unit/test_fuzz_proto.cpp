// NOLINTBEGIN
#include <catch2/catch_test_macros.hpp>
#include <chorus/proto/packet.hpp>

#include <random>
#include <vector>

TEST_CASE("Fuzz testing packet header, audio, ping, and pong deserializers", "[proto][fuzz]") {
    std::mt19937 gen(999);
    std::uniform_int_distribution<int> len_dist(0, 1500);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    constexpr int kFuzzIterations = 10000;

    for (int i = 0; i < kFuzzIterations; ++i) {
        const size_t len = static_cast<size_t>(len_dist(gen));
        std::vector<uint8_t> fuzz_data(len);
        for (size_t j = 0; j < len; ++j) {
            fuzz_data[j] = static_cast<uint8_t>(byte_dist(gen));
        }

        const std::span<const uint8_t> span(fuzz_data.data(), fuzz_data.size());

        // Parsers must NEVER crash, throw, buffer overrun, or perform UB on random data
        const auto hdr = chorus::parse_header(span);
        const auto audio = chorus::parse_audio_packet(span);
        const auto ping = chorus::parse_ping_packet(span);
        const auto pong = chorus::parse_pong_packet(span);

        (void)hdr;
        (void)audio;
        (void)ping;
        (void)pong;
    }

    REQUIRE(true);
}
// NOLINTEND
