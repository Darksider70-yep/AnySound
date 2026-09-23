#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace chorus {

/// @brief Packet types defined in architecture.md section 8.
enum class PacketType : uint8_t {
    Audio = 1,
    Ping = 2,
    Pong = 3
};

inline constexpr uint16_t kPacketMagic = 0x4348;  // 'C' 'H'
inline constexpr size_t kMaxUdpPayloadSize = 1200;

/// @brief Endian serialization helpers (Big-Endian network order).
namespace endian {
    void write_u16_be(uint8_t* dst, uint16_t val) noexcept;
    void write_u32_be(uint8_t* dst, uint32_t val) noexcept;
    void write_u64_be(uint8_t* dst, uint64_t val) noexcept;

    [[nodiscard]] uint16_t read_u16_be(const uint8_t* src) noexcept;
    [[nodiscard]] uint32_t read_u32_be(const uint8_t* src) noexcept;
    [[nodiscard]] uint64_t read_u64_be(const uint8_t* src) noexcept;
}  // namespace endian

/// @brief Socket endpoint representation (IPv4 address + port).
struct Endpoint {
    std::string address{"127.0.0.1"};
    uint16_t port{0};
};

/// @brief Cross-platform RAII UDP socket wrapper.
class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    /// @brief Binds the socket to a local port and optional interface address.
    [[nodiscard]] bool bind(uint16_t port, std::string_view interface_ip = "0.0.0.0");

    /// @brief Sets receive timeout in milliseconds.
    [[nodiscard]] bool set_recv_timeout_ms(int timeout_ms);

    /// @brief Enables or disables SO_BROADCAST on the UDP socket.
    [[nodiscard]] bool enable_broadcast(bool enable = true);

    /// @brief Joins an IPv4 multicast group.
    [[nodiscard]] bool join_multicast_group(std::string_view group_ip);

    /// @brief Sends a datagram to the specified destination.
    [[nodiscard]] bool send_to(std::span<const uint8_t> data, const Endpoint& dest);

    /// @brief Receives a datagram into buffer, recording sender endpoint.
    /// @return Number of bytes received, 0 on timeout, or -1 on error.
    [[nodiscard]] int receive_from(std::span<uint8_t> buffer, Endpoint& sender_out);

    /// @brief Closes the socket.
    void close() noexcept;

    /// @brief Returns true if the socket descriptor is valid.
    [[nodiscard]] bool is_valid() const noexcept;

    /// @brief Returns the local port bound to (if bound with port 0).
    [[nodiscard]] uint16_t local_port() const;

private:
    intptr_t socket_handle_{-1};
};

}  // namespace chorus
