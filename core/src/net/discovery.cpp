#include <chorus/net/discovery.hpp>
#include <chorus/net/udp_socket.hpp>

#include <nlohmann/json.hpp>
#include <chrono>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
#else
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <netinet/in.h>
#endif

namespace chorus {

using json = nlohmann::json;

namespace {

inline constexpr size_t kDiscoveryBufferSize = 1024;
inline constexpr uint64_t kPruneThresholdSec = 6;
inline constexpr uint64_t kBroadcastIntervalSec = 2;
inline constexpr const char* kMulticastDiscoveryGroup = "239.255.77.77";

uint64_t get_current_epoch_sec() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(now).count());
}

std::vector<std::string> get_broadcast_target_ips() {
    std::unordered_set<std::string> ips;
    ips.insert("255.255.255.255");
    ips.insert(kMulticastDiscoveryGroup);
    ips.insert("127.0.0.1");

#ifdef _WIN32
    ULONG buf_len = 15000;
    std::vector<uint8_t> buffer(buf_len);
    auto* addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    ULONG ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, addresses, &buf_len);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(buf_len);
        addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, addresses, &buf_len);
    }
    if (ret == NO_ERROR) {
        for (auto* curr = addresses; curr != nullptr; curr = curr->Next) {
            if (curr->OperStatus != IfOperStatusUp || curr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
                continue;
            }
            for (auto* unicast = curr->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
                if (unicast->Address.lpSockaddr != nullptr && unicast->Address.lpSockaddr->sa_family == AF_INET) {
                    auto* sa_in = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
                    uint32_t ip = ntohl(sa_in->sin_addr.s_addr);
                    UINT8 prefix_len = unicast->OnLinkPrefixLength;
                    if (prefix_len > 0 && prefix_len < 32) {
                        uint32_t mask = 0xFFFFFFFFU << (32 - prefix_len);
                        uint32_t bcast = (ip & mask) | (~mask);
                        struct in_addr bcast_addr{};
                        bcast_addr.s_addr = htonl(bcast);
                        std::array<char, INET_ADDRSTRLEN> str_buf{};
                        if (inet_ntop(AF_INET, &bcast_addr, str_buf.data(), str_buf.size()) != nullptr) {
                            ips.insert(str_buf.data());
                        }
                    }
                }
            }
        }
    }
#else
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != -1) {
        for (auto* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
                continue;
            }
            if ((ifa->ifa_flags & IFF_UP) == 0 || (ifa->ifa_flags & IFF_LOOPBACK) != 0) {
                continue;
            }
            if ((ifa->ifa_flags & IFF_BROADCAST) != 0 && ifa->ifa_broadaddr != nullptr) {
                auto* sa_in = reinterpret_cast<sockaddr_in*>(ifa->ifa_broadaddr);
                std::array<char, INET_ADDRSTRLEN> str_buf{};
                if (inet_ntop(AF_INET, &sa_in->sin_addr, str_buf.data(), str_buf.size()) != nullptr) {
                    ips.insert(str_buf.data());
                }
            }
        }
        freeifaddrs(ifaddr);
    }
#endif

    return std::vector<std::string>(ips.begin(), ips.end());
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
    (void)socket_.enable_broadcast(true);
    (void)socket_.join_multicast_group(kMulticastDiscoveryGroup);
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
    (void)socket_.enable_broadcast(true);
    is_running_ = true;
    last_broadcast_sec_ = 0;
    update(); // Send initial beacon immediately
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
        const std::span<const uint8_t> payload_span(
            reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

        const auto targets = get_broadcast_target_ips();
        for (const auto& target_ip : targets) {
            const Endpoint dest{
                .address = target_ip,
                .port = broadcast_port_
            };
            (void)socket_.send_to(payload_span, dest);
        }

        last_broadcast_sec_ = now_sec;
    }
}

}  // namespace chorus
