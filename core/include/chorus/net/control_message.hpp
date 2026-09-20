#pragma once

#include <chorus/core.hpp>
#include <chorus/codec/opus_codec.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace chorus {

inline constexpr size_t kMaxControlMessageSize = 65536;  // 64 KB max

struct HelloMessage {
    std::string name{"Chorus Client"};
    std::string platform{"Windows"};
    uint32_t protocol{kProtocolVersion};
    std::string pin;
};

struct WelcomeMessage {
    uint32_t session_id{1};
    uint16_t udp_port{kDefaultUdpDataPort};
    uint32_t sample_rate{static_cast<uint32_t>(kSampleRate)};
    uint32_t channels{static_cast<uint32_t>(kChannels)};
    uint32_t frame_ms{static_cast<uint32_t>(kFrameDurationMs)};
    uint64_t target_latency_ms{kDefaultTargetLatencyMs};
    uint64_t host_us{0};
};

struct RejectMessage {
    std::string reason{"bad_pin"};  // "bad_pin", "version", "full"
};

struct SetVolumeMessage {
    float value{1.0F};  // 0.0 to 1.0
};

struct SetMuteMessage {
    bool value{false};
};

struct SetOffsetMsMessage {
    int32_t value{0};  // -500 to +500 ms
};

struct SetTargetLatencyMsMessage {
    uint64_t value{kDefaultTargetLatencyMs};
};

struct ClientStatsMessage {
    int64_t sync_error_us{0};
    double skew_ppm{0.0};
    uint64_t underruns{0};
    uint64_t late{0};
    double loss_pct{0.0};
    uint32_t buffer_ms{0};
};

struct ByeMessage {};

using ControlMessage = std::variant<
    HelloMessage,
    WelcomeMessage,
    RejectMessage,
    SetVolumeMessage,
    SetMuteMessage,
    SetOffsetMsMessage,
    SetTargetLatencyMsMessage,
    ClientStatsMessage,
    ByeMessage
>;

/// @brief Serializes a control message to a length-prefixed JSON byte stream (u32 length + JSON payload).
/// @return Number of bytes written to dest (including 4-byte prefix), or 0 on error.
size_t serialize_control_message(const ControlMessage& message, std::span<uint8_t> dest) noexcept;

/// @brief Serializes a control message into a std::vector<uint8_t> buffer.
std::vector<uint8_t> serialize_control_message_vec(const ControlMessage& message);

/// @brief Parses a control message from a JSON string or raw payload.
std::optional<ControlMessage> parse_control_message(std::string_view json_str) noexcept;

}  // namespace chorus
