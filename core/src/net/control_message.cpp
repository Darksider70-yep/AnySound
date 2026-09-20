#include <chorus/net/control_message.hpp>
#include <chorus/net/udp_socket.hpp>

#include <nlohmann/json.hpp>
#include <algorithm>

namespace chorus {

using json = nlohmann::json;

namespace {

json message_to_json(const ControlMessage& message) {
    return std::visit([](const auto& msg) -> json {
        using T = std::decay_t<decltype(msg)>;
        if constexpr (std::is_same_v<T, HelloMessage>) {
            return json{
                {"type", "hello"},
                {"name", msg.name},
                {"platform", msg.platform},
                {"protocol", msg.protocol},
                {"pin", msg.pin}
            };
        } else if constexpr (std::is_same_v<T, WelcomeMessage>) {
            return json{
                {"type", "welcome"},
                {"sessionId", msg.session_id},
                {"udpPort", msg.udp_port},
                {"sampleRate", msg.sample_rate},
                {"channels", msg.channels},
                {"frameMs", msg.frame_ms},
                {"targetLatencyMs", msg.target_latency_ms},
                {"hostUs", msg.host_us}
            };
        } else if constexpr (std::is_same_v<T, RejectMessage>) {
            return json{
                {"type", "reject"},
                {"reason", msg.reason}
            };
        } else if constexpr (std::is_same_v<T, SetVolumeMessage>) {
            return json{
                {"type", "set_volume"},
                {"value", msg.value}
            };
        } else if constexpr (std::is_same_v<T, SetMuteMessage>) {
            return json{
                {"type", "set_mute"},
                {"value", msg.value}
            };
        } else if constexpr (std::is_same_v<T, SetOffsetMsMessage>) {
            return json{
                {"type", "set_offset_ms"},
                {"value", msg.value}
            };
        } else if constexpr (std::is_same_v<T, SetTargetLatencyMsMessage>) {
            return json{
                {"type", "set_target_latency_ms"},
                {"value", msg.value}
            };
        } else if constexpr (std::is_same_v<T, ClientStatsMessage>) {
            return json{
                {"type", "stats"},
                {"syncErrorUs", msg.sync_error_us},
                {"skewPpm", msg.skew_ppm},
                {"underruns", msg.underruns},
                {"late", msg.late},
                {"lossPct", msg.loss_pct},
                {"bufferMs", msg.buffer_ms}
            };
        } else if constexpr (std::is_same_v<T, ByeMessage>) {
            return json{{"type", "bye"}};
        }
        return json{};
    }, message);
}

HelloMessage parse_hello(const json& json_obj) {
    HelloMessage msg;
    if (json_obj.contains("name") && json_obj["name"].is_string()) {
        msg.name = json_obj["name"].get<std::string>();
    }
    if (json_obj.contains("platform") && json_obj["platform"].is_string()) {
        msg.platform = json_obj["platform"].get<std::string>();
    }
    if (json_obj.contains("protocol") && json_obj["protocol"].is_number()) {
        msg.protocol = json_obj["protocol"].get<uint32_t>();
    }
    if (json_obj.contains("pin") && json_obj["pin"].is_string()) {
        msg.pin = json_obj["pin"].get<std::string>();
    }
    return msg;
}

WelcomeMessage parse_welcome(const json& json_obj) {
    WelcomeMessage msg;
    if (json_obj.contains("sessionId") && json_obj["sessionId"].is_number()) {
        msg.session_id = json_obj["sessionId"].get<uint32_t>();
    }
    if (json_obj.contains("udpPort") && json_obj["udpPort"].is_number()) {
        msg.udp_port = json_obj["udpPort"].get<uint16_t>();
    }
    if (json_obj.contains("sampleRate") && json_obj["sampleRate"].is_number()) {
        msg.sample_rate = json_obj["sampleRate"].get<uint32_t>();
    }
    if (json_obj.contains("channels") && json_obj["channels"].is_number()) {
        msg.channels = json_obj["channels"].get<uint32_t>();
    }
    if (json_obj.contains("frameMs") && json_obj["frameMs"].is_number()) {
        msg.frame_ms = json_obj["frameMs"].get<uint32_t>();
    }
    if (json_obj.contains("targetLatencyMs") && json_obj["targetLatencyMs"].is_number()) {
        msg.target_latency_ms = json_obj["targetLatencyMs"].get<uint64_t>();
    }
    if (json_obj.contains("hostUs") && json_obj["hostUs"].is_number()) {
        msg.host_us = json_obj["hostUs"].get<uint64_t>();
    }
    return msg;
}

ClientStatsMessage parse_stats(const json& json_obj) {
    ClientStatsMessage msg;
    if (json_obj.contains("syncErrorUs") && json_obj["syncErrorUs"].is_number()) {
        msg.sync_error_us = json_obj["syncErrorUs"].get<int64_t>();
    }
    if (json_obj.contains("skewPpm") && json_obj["skewPpm"].is_number()) {
        msg.skew_ppm = json_obj["skewPpm"].get<double>();
    }
    if (json_obj.contains("underruns") && json_obj["underruns"].is_number()) {
        msg.underruns = json_obj["underruns"].get<uint64_t>();
    }
    if (json_obj.contains("late") && json_obj["late"].is_number()) {
        msg.late = json_obj["late"].get<uint64_t>();
    }
    if (json_obj.contains("lossPct") && json_obj["lossPct"].is_number()) {
        msg.loss_pct = json_obj["lossPct"].get<double>();
    }
    if (json_obj.contains("bufferMs") && json_obj["bufferMs"].is_number()) {
        msg.buffer_ms = json_obj["bufferMs"].get<uint32_t>();
    }
    return msg;
}

}  // namespace

size_t serialize_control_message(const ControlMessage& message, std::span<uint8_t> dest) noexcept {
    try {
        const json json_obj = message_to_json(message);
        const std::string serialized = json_obj.dump();
        const size_t payload_len = serialized.size();
        const size_t total_len = payload_len + 4;

        if (payload_len > kMaxControlMessageSize || dest.size() < total_len) {
            return 0;
        }

        endian::write_u32_be(dest.data(), static_cast<uint32_t>(payload_len));
        std::ranges::copy(serialized, dest.begin() + 4);
        return total_len;
    } catch (...) {
        return 0;
    }
}

std::vector<uint8_t> serialize_control_message_vec(const ControlMessage& message) {
    const json json_obj = message_to_json(message);
    const std::string serialized = json_obj.dump();
    const auto payload_len = static_cast<uint32_t>(serialized.size());

    std::vector<uint8_t> buf(serialized.size() + 4);
    endian::write_u32_be(buf.data(), payload_len);
    std::ranges::copy(serialized, buf.begin() + 4);
    return buf;
}

std::optional<ControlMessage> parse_control_message(std::string_view json_str) noexcept {
    try {
        const auto json_obj = json::parse(json_str);
        if (!json_obj.is_object() || !json_obj.contains("type") || !json_obj["type"].is_string()) {
            return std::nullopt;
        }

        const std::string type_name = json_obj["type"].get<std::string>();

        if (type_name == "hello") {
            return parse_hello(json_obj);
        }
        if (type_name == "welcome") {
            return parse_welcome(json_obj);
        }
        if (type_name == "reject") {
            const std::string reason = (json_obj.contains("reason") && json_obj["reason"].is_string())
                ? json_obj["reason"].get<std::string>() : "bad_pin";
            return RejectMessage{.reason = reason};
        }
        if (type_name == "set_volume") {
            const float val = (json_obj.contains("value") && json_obj["value"].is_number())
                ? json_obj["value"].get<float>() : 1.0F;
            return SetVolumeMessage{.value = val};
        }
        if (type_name == "set_mute") {
            const bool val = (json_obj.contains("value") && json_obj["value"].is_boolean())
                ? json_obj["value"].get<bool>() : false;
            return SetMuteMessage{.value = val};
        }
        if (type_name == "set_offset_ms") {
            const int32_t val = (json_obj.contains("value") && json_obj["value"].is_number())
                ? json_obj["value"].get<int32_t>() : 0;
            return SetOffsetMsMessage{.value = val};
        }
        if (type_name == "set_target_latency_ms") {
            const uint64_t val = (json_obj.contains("value") && json_obj["value"].is_number())
                ? json_obj["value"].get<uint64_t>() : kDefaultTargetLatencyMs;
            return SetTargetLatencyMsMessage{.value = val};
        }
        if (type_name == "stats") {
            return parse_stats(json_obj);
        }
        if (type_name == "bye") {
            return ByeMessage{};
        }

        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace chorus
