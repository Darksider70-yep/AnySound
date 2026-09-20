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

}  // namespace

size_t serialize_control_message(const ControlMessage& message, std::span<uint8_t> dest) noexcept {
    try {
        const json j = message_to_json(message);
        const std::string serialized = j.dump();
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
    const json j = message_to_json(message);
    const std::string serialized = j.dump();
    const auto payload_len = static_cast<uint32_t>(serialized.size());

    std::vector<uint8_t> buf(serialized.size() + 4);
    endian::write_u32_be(buf.data(), payload_len);
    std::ranges::copy(serialized, buf.begin() + 4);
    return buf;
}

std::optional<ControlMessage> parse_control_message(std::string_view json_str) noexcept {
    try {
        const auto j = json::parse(json_str);
        if (!j.is_object() || !j.contains("type") || !j["type"].is_string()) {
            return std::nullopt;
        }

        const std::string type = j["type"].get<std::string>();

        if (type == "hello") {
            HelloMessage msg;
            if (j.contains("name") && j["name"].is_string()) {
                msg.name = j["name"].get<std::string>();
            }
            if (j.contains("platform") && j["platform"].is_string()) {
                msg.platform = j["platform"].get<std::string>();
            }
            if (j.contains("protocol") && j["protocol"].is_number()) {
                msg.protocol = j["protocol"].get<uint32_t>();
            }
            if (j.contains("pin") && j["pin"].is_string()) {
                msg.pin = j["pin"].get<std::string>();
            }
            return msg;
        }

        if (type == "welcome") {
            WelcomeMessage msg;
            if (j.contains("sessionId") && j["sessionId"].is_number()) {
                msg.session_id = j["sessionId"].get<uint32_t>();
            }
            if (j.contains("udpPort") && j["udpPort"].is_number()) {
                msg.udp_port = j["udpPort"].get<uint16_t>();
            }
            if (j.contains("sampleRate") && j["sampleRate"].is_number()) {
                msg.sample_rate = j["sampleRate"].get<uint32_t>();
            }
            if (j.contains("channels") && j["channels"].is_number()) {
                msg.channels = j["channels"].get<uint32_t>();
            }
            if (j.contains("frameMs") && j["frameMs"].is_number()) {
                msg.frame_ms = j["frameMs"].get<uint32_t>();
            }
            if (j.contains("targetLatencyMs") && j["targetLatencyMs"].is_number()) {
                msg.target_latency_ms = j["targetLatencyMs"].get<uint64_t>();
            }
            if (j.contains("hostUs") && j["hostUs"].is_number()) {
                msg.host_us = j["hostUs"].get<uint64_t>();
            }
            return msg;
        }

        if (type == "reject") {
            RejectMessage msg;
            if (j.contains("reason") && j["reason"].is_string()) {
                msg.reason = j["reason"].get<std::string>();
            }
            return msg;
        }

        if (type == "set_volume") {
            SetVolumeMessage msg;
            if (j.contains("value") && j["value"].is_number()) {
                msg.value = j["value"].get<float>();
            }
            return msg;
        }

        if (type == "set_mute") {
            SetMuteMessage msg;
            if (j.contains("value") && j["value"].is_boolean()) {
                msg.value = j["value"].get<bool>();
            }
            return msg;
        }

        if (type == "set_offset_ms") {
            SetOffsetMsMessage msg;
            if (j.contains("value") && j["value"].is_number()) {
                msg.value = j["value"].get<int32_t>();
            }
            return msg;
        }

        if (type == "set_target_latency_ms") {
            SetTargetLatencyMsMessage msg;
            if (j.contains("value") && j["value"].is_number()) {
                msg.value = j["value"].get<uint64_t>();
            }
            return msg;
        }

        if (type == "stats") {
            ClientStatsMessage msg;
            if (j.contains("syncErrorUs") && j["syncErrorUs"].is_number()) {
                msg.sync_error_us = j["syncErrorUs"].get<int64_t>();
            }
            if (j.contains("skewPpm") && j["skewPpm"].is_number()) {
                msg.skew_ppm = j["skewPpm"].get<double>();
            }
            if (j.contains("underruns") && j["underruns"].is_number()) {
                msg.underruns = j["underruns"].get<uint64_t>();
            }
            if (j.contains("late") && j["late"].is_number()) {
                msg.late = j["late"].get<uint64_t>();
            }
            if (j.contains("lossPct") && j["lossPct"].is_number()) {
                msg.loss_pct = j["lossPct"].get<double>();
            }
            if (j.contains("bufferMs") && j["bufferMs"].is_number()) {
                msg.buffer_ms = j["bufferMs"].get<uint32_t>();
            }
            return msg;
        }

        if (type == "bye") {
            return ByeMessage{};
        }

        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace chorus
