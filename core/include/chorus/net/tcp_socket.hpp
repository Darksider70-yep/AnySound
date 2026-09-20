#pragma once

#include <chorus/net/control_message.hpp>
#include <chorus/net/udp_socket.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace chorus {

inline constexpr int kDefaultConnectTimeoutMs = 3000;

/// @brief Represents an active TCP connection between host and client.
class TcpStream {
public:
    TcpStream();
    explicit TcpStream(intptr_t raw_fd, Endpoint peer);
    ~TcpStream();

    TcpStream(const TcpStream&) = delete;
    TcpStream& operator=(const TcpStream&) = delete;
    TcpStream(TcpStream&& other) noexcept;
    TcpStream& operator=(TcpStream&& other) noexcept;

    /// @brief Connects to a remote host endpoint.
    [[nodiscard]] bool connect(const Endpoint& endpoint, int timeout_ms = kDefaultConnectTimeoutMs);

    /// @brief Sends a framed ControlMessage (length prefix + JSON).
    [[nodiscard]] bool send_message(const ControlMessage& message);

    /// @brief Reads and parses pending framed ControlMessages without blocking indefinitely.
    /// @return List of newly completed ControlMessages.
    std::vector<ControlMessage> read_messages();

    /// @brief Closes the connection.
    void close() noexcept;

    /// @brief Returns true if connection is active.
    [[nodiscard]] bool is_connected() const noexcept;

    /// @brief Returns peer endpoint.
    [[nodiscard]] Endpoint peer() const;

private:
    intptr_t socket_fd_{-1};
    Endpoint peer_{};
    std::vector<uint8_t> rx_buffer_;
};

/// @brief Listens for incoming TCP client connections.
class TcpListener {
public:
    TcpListener();
    ~TcpListener();

    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;
    TcpListener(TcpListener&& other) noexcept;
    TcpListener& operator=(TcpListener&& other) noexcept;

    /// @brief Binds and listens on the specified port.
    [[nodiscard]] bool listen(uint16_t port, std::string_view interface_ip = "0.0.0.0");

    /// @brief Non-blocking accept for an incoming client connection.
    /// @return Unique pointer to TcpStream, or nullptr if no connection pending.
    [[nodiscard]] std::unique_ptr<TcpStream> accept_client() const;

    /// @brief Closes listener socket.
    void close() noexcept;

    /// @brief Returns true if listening.
    [[nodiscard]] bool is_listening() const noexcept;

    /// @brief Returns bound port.
    [[nodiscard]] uint16_t port() const;

private:
    intptr_t socket_fd_{-1};
    uint16_t port_{0};
};

}  // namespace chorus
