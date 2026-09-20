#include <chorus/net/discovery.hpp>
#include <chorus/net/udp_socket.hpp>

#include <nlohmann/json.hpp>
#include <chrono>

namespace chorus {

using json = nlohmann::json;

namespace {

inline constexpr size_t kDiscoveryBufferSize = 1024;
inline constexpr uint64_t kPruneThresholdSec = 6;
inline constexpr uint64_t kBroadcastIntervalSec = 2;

uint64_t get_current_epoch_sec() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(now).count());
}

}  // namespace

// -----------------------------------------------------------------------------
// DiscoveryScanner
// -----------------------------------------------------------------------------

DiscoveryScanner::DiscoveryScanner() = default;

DiscoveryScanner::~DiscoveryScanner() {
    stop();
}

bool DiscoveryScanner::start(uint16_t port) {
    stop();
    if (!socket_.bind(port, "0.0.0.0")) {
        return false;
    }
    (void)socket_.set_recv_timeout_ms(1);
    is_running_ = true;
    return true;
}

void DiscoveryScanner::stop() {
    if (is_running_) {
        is_running_ = false;
        socket_.close();
        std::lock_guard<std::mutex> lock(hosts_mutex_);
        hosts_.clear();
    }
}

void DiscoveryScanner::update() {
    if (!is_running_) {
        return;
    }

    std::array<uint8_t, kDiscoveryBufferSize> recv_buf{};
    Endpoint sender;
    const uint64_t now_sec = get_current_epoch_sec();

    while (true) {
        const int bytes = socket_.receive_from(recv_buf, sender);
        if (bytes <= 0) {
            break;
        }

        try {
            const std::string_view json_view(reinterpret_cast<const char*>(recv_buf.data()), static_cast<size_t>(bytes));
            const auto json_obj = json::parse(json_view);

            if (json_obj.is_object() && json_obj.contains("chorus_beacon") && json_obj["chorus_beacon"].get<bool>()) {
                DiscoveredHost host{
                    .name = json_obj.value("name", "Chorus Host"),
                    .address = sender.address,
                    .tcp_port = json_obj.value("tcp_port", static_cast<uint16_t>(kDefaultTcpControlPort)),
                    .udp_port = json_obj.value("udp_port", static_cast<uint16_t>(kDefaultUdpDataPort)),
                    .protocol = json_obj.value("protocol", static_cast<uint32_t>(kProtocolVersion)),
                    .last_seen_sec = now_sec
                };

                const std::string key = host.address + ":" + std::to_string(host.tcp_port);
                std::lock_guard<std::mutex> lock(hosts_mutex_);
                hosts_[key] = std::move(host);
            }
        } catch (const std::exception&) {
            // Malformed or non-JSON beacon packet, safely drop
            continue;
        }
    }

    // Prune hosts not seen in the last 6 seconds
    {
        std::lock_guard<std::mutex> lock(hosts_mutex_);
        std::erase_if(hosts_, [now_sec](const auto& item) {
            return (now_sec >= item.second.last_seen_sec) && ((now_sec - item.second.last_seen_sec) > kPruneThresholdSec);
        });
    }
}

std::vector<DiscoveredHost> DiscoveryScanner::discovered_hosts() const {
    std::vector<DiscoveredHost> result;
    std::lock_guard<std::mutex> lock(hosts_mutex_);
    result.reserve(hosts_.size());
    for (const auto& [host_key, host_val] : hosts_) {
        (void)host_key;
        result.push_back(host_val);
    }
    return result;
}

// -----------------------------------------------------------------------------
// DiscoveryBroadcaster
// -----------------------------------------------------------------------------

DiscoveryBroadcaster::DiscoveryBroadcaster() = default;

DiscoveryBroadcaster::~DiscoveryBroadcaster() {
    stop();
}

bool DiscoveryBroadcaster::start(std::string_view host_name,
                                 uint16_t tcp_port,
                                 uint16_t udp_port,
                                 uint16_t broadcast_port) {
    stop();
    host_name_ = std::string(host_name);
    tcp_port_ = tcp_port;
    udp_port_ = udp_port;
    broadcast_port_ = broadcast_port;

    if (!socket_.bind(0, "0.0.0.0")) {
        return false;
    }
    is_running_ = true;
    last_broadcast_sec_ = 0;
    return true;
}

void DiscoveryBroadcaster::stop() {
    if (is_running_) {
        is_running_ = false;
        socket_.close();
    }
}

void DiscoveryBroadcaster::update() {
    if (!is_running_) {
        return;
    }

    const uint64_t now_sec = get_current_epoch_sec();
    if (now_sec - last_broadcast_sec_ >= kBroadcastIntervalSec) {
        const json json_obj{
            {"chorus_beacon", true},
            {"name", host_name_},
            {"tcp_port", tcp_port_},
            {"udp_port", udp_port_},
            {"protocol", kProtocolVersion}
        };

        const std::string payload = json_obj.dump();
        const Endpoint broadcast_dest{
            .address = "127.0.0.1",  // Localhost broadcast for test / 255.255.255.255 for LAN
            .port = broadcast_port_
        };

        (void)socket_.send_to(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(payload.data()), payload.size()),
            broadcast_dest
        );

        last_broadcast_sec_ = now_sec;
    }
}

}  // namespace chorus
