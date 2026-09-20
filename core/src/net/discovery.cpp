#include <chorus/net/discovery.hpp>
#include <chorus/net/udp_socket.hpp>

#include <nlohmann/json.hpp>
#include <chrono>

namespace chorus {

using json = nlohmann::json;

namespace {

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

    std::array<uint8_t, 1024> recv_buf{};
    Endpoint sender;
    const uint64_t now_sec = get_current_epoch_sec();

    while (true) {
        const int bytes = socket_.receive_from(recv_buf, sender);
        if (bytes <= 0) {
            break;
        }

        try {
            const std::string_view json_view(reinterpret_cast<const char*>(recv_buf.data()), static_cast<size_t>(bytes));
            const auto j = json::parse(json_view);

            if (j.is_object() && j.contains("chorus_beacon") && j["chorus_beacon"].get<bool>()) {
                DiscoveredHost host{
                    .name = j.value("name", "Chorus Host"),
                    .address = sender.address,
                    .tcp_port = j.value("tcp_port", static_cast<uint16_t>(kDefaultTcpControlPort)),
                    .udp_port = j.value("udp_port", static_cast<uint16_t>(kDefaultUdpDataPort)),
                    .protocol = j.value("protocol", static_cast<uint32_t>(kProtocolVersion)),
                    .last_seen_sec = now_sec
                };

                const std::string key = host.address + ":" + std::to_string(host.tcp_port);
                std::lock_guard<std::mutex> lock(hosts_mutex_);
                hosts_[key] = std::move(host);
            }
        } catch (...) {
            // Drop invalid/non-JSON beacon
        }
    }

    // Prune hosts not seen in the last 6 seconds
    {
        std::lock_guard<std::mutex> lock(hosts_mutex_);
        std::erase_if(hosts_, [now_sec](const auto& item) {
            return (now_sec >= item.second.last_seen_sec) && ((now_sec - item.second.last_seen_sec) > 6);
        });
    }
}

std::vector<DiscoveredHost> DiscoveryScanner::discovered_hosts() const {
    std::vector<DiscoveredHost> result;
    std::lock_guard<std::mutex> lock(hosts_mutex_);
    result.reserve(hosts_.size());
    for (const auto& [k, v] : hosts_) {
        result.push_back(v);
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
    if (now_sec - last_broadcast_sec_ >= 2) {
        const json j{
            {"chorus_beacon", true},
            {"name", host_name_},
            {"tcp_port", tcp_port_},
            {"udp_port", udp_port_},
            {"protocol", kProtocolVersion}
        };

        const std::string payload = j.dump();
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
