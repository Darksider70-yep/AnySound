#include <chorus/proto/packet.hpp>

#include <algorithm>

namespace chorus {

namespace {
constexpr size_t kHeaderOffsetMagic = 0;
constexpr size_t kHeaderOffsetVersion = 2;
constexpr size_t kHeaderOffsetType = 3;
constexpr size_t kHeaderOffsetSessionId = 4;

constexpr size_t kAudioOffsetSeq = 8;
constexpr size_t kAudioOffsetPlayAtUs = 12;
constexpr size_t kAudioOffsetFlags = 20;
constexpr size_t kAudioOffsetPayloadLen = 21;
constexpr size_t kAudioOffsetPayload = 23;

constexpr size_t kPingOffsetPingId = 8;
constexpr size_t kPingOffsetT0 = 12;

constexpr size_t kPongOffsetPingId = 8;
constexpr size_t kPongOffsetT0 = 12;
constexpr size_t kPongOffsetT1 = 20;
constexpr size_t kPongOffsetT2 = 28;
}  // namespace

size_t serialize_header(const PacketHeader& header, std::span<uint8_t> dest) noexcept {
    if (dest.size() < kCommonHeaderSize) {
        return 0;
    }
    endian::write_u16_be(dest.data() + kHeaderOffsetMagic, header.magic);
    dest[kHeaderOffsetVersion] = header.version;
    dest[kHeaderOffsetType] = static_cast<uint8_t>(header.type);
    endian::write_u32_be(dest.data() + kHeaderOffsetSessionId, header.session_id);
    return kCommonHeaderSize;
}

std::optional<PacketHeader> parse_header(std::span<const uint8_t> src) noexcept {
    if (src.size() < kCommonHeaderSize) {
        return std::nullopt;
    }
    const uint16_t magic = endian::read_u16_be(src.data() + kHeaderOffsetMagic);
    const uint8_t version = src[kHeaderOffsetVersion];
    const uint8_t raw_type = src[kHeaderOffsetType];

    if (magic != kPacketMagic || version != kProtocolVersion) {
        return std::nullopt;
    }
    if (raw_type < static_cast<uint8_t>(PacketType::Audio) ||
        raw_type > static_cast<uint8_t>(PacketType::Pong)) {
        return std::nullopt;
    }

    PacketHeader header;
    header.magic = magic;
    header.version = version;
    header.type = static_cast<PacketType>(raw_type);
    header.session_id = endian::read_u32_be(src.data() + kHeaderOffsetSessionId);
    return header;
}

size_t serialize_audio_packet(const AudioPacket& packet, std::span<uint8_t> dest) noexcept {
    const size_t total_needed = kAudioHeaderSize + packet.payload.size();
    if (dest.size() < total_needed || packet.payload.size() > kMaxUdpPayloadSize) {
        return 0;
    }

    if (serialize_header(packet.header, dest) == 0) {
        return 0;
    }

    endian::write_u32_be(dest.data() + kAudioOffsetSeq, packet.seq);
    endian::write_u64_be(dest.data() + kAudioOffsetPlayAtUs, packet.play_at_host_us);
    dest[kAudioOffsetFlags] = packet.flags;
    endian::write_u16_be(dest.data() + kAudioOffsetPayloadLen,
                         static_cast<uint16_t>(packet.payload.size()));

    std::ranges::copy(packet.payload, dest.begin() + static_cast<ptrdiff_t>(kAudioOffsetPayload));
    return total_needed;
}

std::optional<AudioPacket> parse_audio_packet(std::span<const uint8_t> src) noexcept {
    if (src.size() < kAudioHeaderSize) {
        return std::nullopt;
    }

    const auto header_opt = parse_header(src);
    if (!header_opt || header_opt->type != PacketType::Audio) {
        return std::nullopt;
    }

    const uint16_t payload_len = endian::read_u16_be(src.data() + kAudioOffsetPayloadLen);
    if (kAudioHeaderSize + static_cast<size_t>(payload_len) > src.size()) {
        return std::nullopt;
    }

    AudioPacket packet;
    packet.header = *header_opt;
    packet.seq = endian::read_u32_be(src.data() + kAudioOffsetSeq);
    packet.play_at_host_us = endian::read_u64_be(src.data() + kAudioOffsetPlayAtUs);
    packet.flags = src[kAudioOffsetFlags];
    packet.payload = std::span<const uint8_t>(src.data() + kAudioOffsetPayload, payload_len);
    return packet;
}

size_t serialize_ping_packet(const PingPacket& packet, std::span<uint8_t> dest) noexcept {
    if (dest.size() < kPingPacketSize) {
        return 0;
    }
    if (serialize_header(packet.header, dest) == 0) {
        return 0;
    }
    endian::write_u32_be(dest.data() + kPingOffsetPingId, packet.ping_id);
    endian::write_u64_be(dest.data() + kPingOffsetT0, packet.t0);
    return kPingPacketSize;
}

std::optional<PingPacket> parse_ping_packet(std::span<const uint8_t> src) noexcept {
    if (src.size() < kPingPacketSize) {
        return std::nullopt;
    }

    const auto header_opt = parse_header(src);
    if (!header_opt || header_opt->type != PacketType::Ping) {
        return std::nullopt;
    }

    PingPacket packet;
    packet.header = *header_opt;
    packet.ping_id = endian::read_u32_be(src.data() + kPingOffsetPingId);
    packet.t0 = endian::read_u64_be(src.data() + kPingOffsetT0);
    return packet;
}

size_t serialize_pong_packet(const PongPacket& packet, std::span<uint8_t> dest) noexcept {
    if (dest.size() < kPongPacketSize) {
        return 0;
    }
    if (serialize_header(packet.header, dest) == 0) {
        return 0;
    }
    endian::write_u32_be(dest.data() + kPongOffsetPingId, packet.ping_id);
    endian::write_u64_be(dest.data() + kPongOffsetT0, packet.t0);
    endian::write_u64_be(dest.data() + kPongOffsetT1, packet.t1);
    endian::write_u64_be(dest.data() + kPongOffsetT2, packet.t2);
    return kPongPacketSize;
}

std::optional<PongPacket> parse_pong_packet(std::span<const uint8_t> src) noexcept {
    if (src.size() < kPongPacketSize) {
        return std::nullopt;
    }

    const auto header_opt = parse_header(src);
    if (!header_opt || header_opt->type != PacketType::Pong) {
        return std::nullopt;
    }

    PongPacket packet;
    packet.header = *header_opt;
    packet.ping_id = endian::read_u32_be(src.data() + kPongOffsetPingId);
    packet.t0 = endian::read_u64_be(src.data() + kPongOffsetT0);
    packet.t1 = endian::read_u64_be(src.data() + kPongOffsetT1);
    packet.t2 = endian::read_u64_be(src.data() + kPongOffsetT2);
    return packet;
}

}  // namespace chorus
