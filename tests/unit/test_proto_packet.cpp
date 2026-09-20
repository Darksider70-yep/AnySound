// NOLINTBEGIN
#include <catch2/catch_test_macros.hpp>
#include <chorus/proto/packet.hpp>

#include <array>
#include <vector>

TEST_CASE("PacketHeader serialization and parsing", "[proto][header]") {
    chorus::PacketHeader header{
        .magic = chorus::kPacketMagic,
        .version = chorus::kProtocolVersion,
        .type = chorus::PacketType::Ping,
        .session_id = 42
    };

    std::array<uint8_t, 16> buffer{};
    const size_t written = chorus::serialize_header(header, buffer);
    REQUIRE(written == chorus::kCommonHeaderSize);

    const auto parsed = chorus::parse_header(buffer);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->magic == chorus::kPacketMagic);
    REQUIRE(parsed->version == chorus::kProtocolVersion);
    REQUIRE(parsed->type == chorus::PacketType::Ping);
    REQUIRE(parsed->session_id == 42);
}

TEST_CASE("AudioPacket serialization and validation", "[proto][audio]") {
    const std::vector<uint8_t> dummy_payload = {0x01, 0x02, 0x03, 0x04, 0x05};
    chorus::AudioPacket packet{
        .header = {
            .magic = chorus::kPacketMagic,
            .version = chorus::kProtocolVersion,
            .type = chorus::PacketType::Audio,
            .session_id = 101
        },
        .seq = 77,
        .play_at_host_us = 123456789ULL,
        .flags = 0,
        .payload = dummy_payload
    };

    std::array<uint8_t, 128> buffer{};
    const size_t bytes_written = chorus::serialize_audio_packet(packet, buffer);
    REQUIRE(bytes_written == chorus::kAudioHeaderSize + dummy_payload.size());

    const auto parsed = chorus::parse_audio_packet(std::span<const uint8_t>(buffer.data(), bytes_written));
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->seq == 77);
    REQUIRE(parsed->play_at_host_us == 123456789ULL);
    REQUIRE(parsed->payload.size() == dummy_payload.size());
    REQUIRE(std::vector<uint8_t>(parsed->payload.begin(), parsed->payload.end()) == dummy_payload);
}

TEST_CASE("Ping and Pong packet round-trips", "[proto][pingpong]") {
    chorus::PingPacket ping{
        .header = {
            .magic = chorus::kPacketMagic,
            .version = chorus::kProtocolVersion,
            .type = chorus::PacketType::Ping,
            .session_id = 1
        },
        .ping_id = 55,
        .t0 = 1000000ULL
    };

    std::array<uint8_t, 64> ping_buf{};
    const size_t ping_bytes = chorus::serialize_ping_packet(ping, ping_buf);
    REQUIRE(ping_bytes == chorus::kPingPacketSize);

    const auto parsed_ping = chorus::parse_ping_packet(std::span<const uint8_t>(ping_buf.data(), ping_bytes));
    REQUIRE(parsed_ping.has_value());
    REQUIRE(parsed_ping->ping_id == 55);
    REQUIRE(parsed_ping->t0 == 1000000ULL);

    chorus::PongPacket pong{
        .header = {
            .magic = chorus::kPacketMagic,
            .version = chorus::kProtocolVersion,
            .type = chorus::PacketType::Pong,
            .session_id = 1
        },
        .ping_id = 55,
        .t0 = 1000000ULL,
        .t1 = 1010000ULL,
        .t2 = 1010500ULL
    };

    std::array<uint8_t, 64> pong_buf{};
    const size_t pong_bytes = chorus::serialize_pong_packet(pong, pong_buf);
    REQUIRE(pong_bytes == chorus::kPongPacketSize);

    const auto parsed_pong = chorus::parse_pong_packet(std::span<const uint8_t>(pong_buf.data(), pong_bytes));
    REQUIRE(parsed_pong.has_value());
    REQUIRE(parsed_pong->ping_id == 55);
    REQUIRE(parsed_pong->t0 == 1000000ULL);
    REQUIRE(parsed_pong->t1 == 1010000ULL);
    REQUIRE(parsed_pong->t2 == 1010500ULL);
}

TEST_CASE("Malformed packet rejection", "[proto][security]") {
    std::array<uint8_t, 32> bad_magic{};
    chorus::endian::write_u16_be(bad_magic.data(), 0x9999);  // Bad magic
    bad_magic[2] = 1;                                      // Version
    bad_magic[3] = 1;                                      // Type Audio
    REQUIRE_FALSE(chorus::parse_header(bad_magic).has_value());
    REQUIRE_FALSE(chorus::parse_audio_packet(bad_magic).has_value());

    std::array<uint8_t, 4> truncated{};
    REQUIRE_FALSE(chorus::parse_header(truncated).has_value());
    REQUIRE_FALSE(chorus::parse_ping_packet(truncated).has_value());
}
// NOLINTEND
