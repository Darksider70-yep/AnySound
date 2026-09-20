#pragma once

#include <chorus/core.hpp>
#include <chorus/net/udp_socket.hpp>

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace chorus {

inline constexpr uint16_t kDefaultDiscoveryPort = 47803;

struct DiscoveredHost {
    std::string name{"Chorus Host"};
    std::string address{"127.0.0.1"};
    uint16_t tcp_port{kDefaultTcpControlPort};
    uint16_t udp_port{kDefaultUdpDataPort};
    uint32_t protocol{kProtocolVersion};
    uint64_t last_seen_sec{0};
};

/// @brief Discovery scanner listening for host advertisement beacons on LAN.
class DiscoveryScanner {
public:
    DiscoveryScanner();
    ~DiscoveryScanner();

    DiscoveryScanner(const DiscoveryScanner&) = delete;
    DiscoveryScanner& operator=(const DiscoveryScanner&) = delete;
    DiscoveryScanner(DiscoveryScanner&&) = delete;
    DiscoveryScanner& operator=(DiscoveryScanner&&) = delete;

    /// @brief Starts scanner on discovery port.
    [[nodiscard]] bool start(uint16_t port = kDefaultDiscoveryPort);

    /// @brief Stops scanner.
    void stop();

    /// @brief Non-blocking tick to process received beacons and prune stale hosts (> 5s).
    void update();

    /// @brief Returns list of currently active hosts discovered on LAN.
    [[nodiscard]] std::vector<DiscoveredHost> discovered_hosts() const;

private:
    UdpSocket socket_;
    mutable std::mutex hosts_mutex_;
    std::map<std::string, DiscoveredHost> hosts_;
    bool is_running_{false};
};

/// @brief Discovery broadcaster sending periodic beacons when hosting.
class DiscoveryBroadcaster {
public:
    DiscoveryBroadcaster();
    ~DiscoveryBroadcaster();

    DiscoveryBroadcaster(const DiscoveryBroadcaster&) = delete;
    DiscoveryBroadcaster& operator=(const DiscoveryBroadcaster&) = delete;
    DiscoveryBroadcaster(DiscoveryBroadcaster&&) = delete;
    DiscoveryBroadcaster& operator=(DiscoveryBroadcaster&&) = delete;

    /// @brief Starts periodic beacon broadcast.
    [[nodiscard]] bool start(std::string_view host_name,
                             uint16_t tcp_port = kDefaultTcpControlPort,
                             uint16_t udp_port = kDefaultUdpDataPort,
                             uint16_t broadcast_port = kDefaultDiscoveryPort);

    /// @brief Stops broadcaster.
    void stop();

    /// @brief Non-blocking tick to broadcast beacon every 1.5 seconds.
    void update();

private:
    std::string host_name_{"Chorus Host"};
    uint16_t tcp_port_{kDefaultTcpControlPort};
    uint16_t udp_port_{kDefaultUdpDataPort};
    uint16_t broadcast_port_{kDefaultDiscoveryPort};
    UdpSocket socket_;
    bool is_running_{false};
    uint64_t last_broadcast_sec_{0};
};

}  // namespace chorus
