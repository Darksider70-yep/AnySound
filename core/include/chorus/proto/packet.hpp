#pragma once

#include <chorus/core.hpp>
#include <chorus/net/udp_socket.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace chorus {

/// @brief Header sizes and offsets according to architecture.md section 8.
inline constexpr size_t kCommonHeaderSize = 8;
inline constexpr size_t kAudioHeaderSize = 23;
inline constexpr size_t kPingPacketSize = 20;
inline constexpr size_t kPongPacketSize = 36;

/// @brief Common 8-byte UDP packet header.
struct PacketHeader {
    uint16_t magic{kPacketMagic};
    uint8_t version{kProtocolVersion};
    PacketType type{PacketType::Audio};
    uint32_t session_id{0};
};

/// @brief Audio packet structure (Type = 1).
struct AudioPacket {
    PacketHeader header;
    uint32_t seq{0};
    uint64_t play_at_host_us{0};
    uint8_t flags{0};
    std::span<const uint8_t> payload{};
};

/// @brief Clock sync PING packet (Type = 2, Client -> Host).
struct PingPacket {
    PacketHeader header;
    uint32_t ping_id{0};
    uint64_t t0{0};  // Client send time in local steady_clock microseconds
};

/// @brief Clock sync PONG packet (Type = 3, Host -> Client).
struct PongPacket {
    PacketHeader header;
    uint32_t ping_id{0};
    uint64_t t0{0};  // Echoed client send time
    uint64_t t1{0};  // Host receive time in host steady_clock microseconds
    uint64_t t2{0};  // Host send time in host steady_clock microseconds
};

/// @brief Serializes a common header into destination buffer.
/// @return Number of bytes written (8), or 0 if buffer too small.
size_t serialize_header(const PacketHeader& header, std::span<uint8_t> dest) noexcept;

/// @brief Parses a common header from raw packet bytes.
std::optional<PacketHeader> parse_header(std::span<const uint8_t> src) noexcept;

/// @brief Serializes an AudioPacket into destination buffer.
/// @return Total serialized packet size, or 0 on error.
size_t serialize_audio_packet(const AudioPacket& packet, std::span<uint8_t> dest) noexcept;

/// @brief Parses an AudioPacket from raw packet bytes.
std::optional<AudioPacket> parse_audio_packet(std::span<const uint8_t> src) noexcept;

/// @brief Serializes a PingPacket into destination buffer.
size_t serialize_ping_packet(const PingPacket& packet, std::span<uint8_t> dest) noexcept;

/// @brief Parses a PingPacket from raw packet bytes.
std::optional<PingPacket> parse_ping_packet(std::span<const uint8_t> src) noexcept;

/// @brief Serializes a PongPacket into destination buffer.
size_t serialize_pong_packet(const PongPacket& packet, std::span<uint8_t> dest) noexcept;

/// @brief Parses a PongPacket from raw packet bytes.
std::optional<PongPacket> parse_pong_packet(std::span<const uint8_t> src) noexcept;

}  // namespace chorus
