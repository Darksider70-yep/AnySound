#include <chorus/net/tcp_socket.hpp>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using sock_t = SOCKET;
    constexpr sock_t kInvalidSocket = INVALID_SOCKET;
    constexpr int kSocketError = SOCKET_ERROR;
#else
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <unistd.h>
    using sock_t = int;
    constexpr sock_t kInvalidSocket = -1;
    constexpr int kSocketError = -1;
#endif

#include <array>

namespace chorus {

namespace {

inline constexpr int kMillisPerSecond = 1000;
inline constexpr size_t kRecvBufferSize = 4096;

#ifdef _WIN32
struct WinsockInit {
    WinsockInit() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    ~WinsockInit() {
        WSACleanup();
    }
    WinsockInit(const WinsockInit&) = delete;
    WinsockInit& operator=(const WinsockInit&) = delete;
    WinsockInit(WinsockInit&&) = delete;
    WinsockInit& operator=(WinsockInit&&) = delete;
};

void ensure_winsock_initialized() {
    static WinsockInit init;
}
#else
void ensure_winsock_initialized() {}
#endif

bool set_nonblocking(sock_t socket_fd, bool non_blocking) noexcept {
#ifdef _WIN32
    u_long mode = non_blocking ? 1 : 0;
    return (ioctlsocket(socket_fd, static_cast<long>(FIONBIO), &mode) == 0);
#else
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) return false;
    flags = non_blocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return (fcntl(socket_fd, F_SETFL, flags) == 0);
#endif
}

}  // namespace

// -----------------------------------------------------------------------------
// TcpStream
// -----------------------------------------------------------------------------

TcpStream::TcpStream() {
    ensure_winsock_initialized();
}

TcpStream::TcpStream(intptr_t raw_fd, Endpoint peer)
    : socket_fd_(raw_fd), peer_(std::move(peer)) {
    ensure_winsock_initialized();
    if (socket_fd_ != -1) {
        set_nonblocking(static_cast<sock_t>(socket_fd_), true);
    }
}

TcpStream::~TcpStream() {
    close();
}

TcpStream::TcpStream(TcpStream&& other) noexcept
    : socket_fd_(other.socket_fd_),
      peer_(std::move(other.peer_)),
      rx_buffer_(std::move(other.rx_buffer_)) {
    other.socket_fd_ = -1;
}

TcpStream& TcpStream::operator=(TcpStream&& other) noexcept {
    if (this != &other) {
        close();
        socket_fd_ = other.socket_fd_;
        peer_ = std::move(other.peer_);
        rx_buffer_ = std::move(other.rx_buffer_);
        other.socket_fd_ = -1;
    }
    return *this;
}

bool TcpStream::connect(const Endpoint& endpoint, int timeout_ms) {
    close();
    peer_ = endpoint;

    const sock_t socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd == kInvalidSocket) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint.port);
    if (inet_pton(AF_INET, endpoint.address.c_str(), &addr.sin_addr) <= 0) {
#ifdef _WIN32
        closesocket(socket_fd);
#else
        ::close(socket_fd);
#endif
        return false;
    }

    // Set non-blocking to honor timeout
    set_nonblocking(socket_fd, true);

    int res = ::connect(socket_fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (res == kSocketError) {
#ifdef _WIN32
        const int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
            closesocket(socket_fd);
            return false;
        }
#else
        if (errno != EINPROGRESS) {
            ::close(socket_fd);
            return false;
        }
#endif
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(socket_fd, &write_fds);

        fd_set except_fds;
        FD_ZERO(&except_fds);
        FD_SET(socket_fd, &except_fds);

        timeval time_val{};
        time_val.tv_sec = timeout_ms / kMillisPerSecond;
        time_val.tv_usec = (timeout_ms % kMillisPerSecond) * kMillisPerSecond;

        res = select(static_cast<int>(socket_fd + 1), nullptr, &write_fds, &except_fds, &time_val);
        if (res <= 0 || FD_ISSET(socket_fd, &except_fds)) {
#ifdef _WIN32
            closesocket(socket_fd);
#else
            ::close(socket_fd);
#endif
            return false;
        }

        int so_error = 0;
#ifdef _WIN32
        int len = sizeof(so_error);
        if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &len) != 0 || so_error != 0) {
            closesocket(socket_fd);
            return false;
        }
#else
        socklen_t len = sizeof(so_error);
        if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &so_error, &len) != 0 || so_error != 0) {
            ::close(socket_fd);
            return false;
        }
#endif
    }

    // Disable Nagle's algorithm for low-latency control messages
    int nodelay = 1;
    setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

    socket_fd_ = static_cast<intptr_t>(socket_fd);
    return true;
}

bool TcpStream::send_message(const ControlMessage& message) {
    if (!is_connected()) {
        return false;
    }

    const std::vector<uint8_t> payload = serialize_control_message_vec(message);
    if (payload.empty()) {
        return false;
    }

    const auto socket_fd = static_cast<sock_t>(socket_fd_);
    size_t total_sent = 0;

    while (total_sent < payload.size()) {
        const int sent = send(socket_fd, reinterpret_cast<const char*>(payload.data() + total_sent),
                              static_cast<int>(payload.size() - total_sent), 0);
        if (sent <= 0) {
#ifdef _WIN32
            const int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                continue;
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
#endif
            close();
            return false;
        }
        total_sent += static_cast<size_t>(sent);
    }
    return true;
}

std::vector<ControlMessage> TcpStream::read_messages() {
    std::vector<ControlMessage> result;
    if (!is_connected()) {
        return result;
    }

    const auto socket_fd = static_cast<sock_t>(socket_fd_);
    std::array<char, kRecvBufferSize> recv_buf{};

    while (true) {
        const int bytes = recv(socket_fd, recv_buf.data(), static_cast<int>(recv_buf.size()), 0);
        if (bytes > 0) {
            rx_buffer_.insert(rx_buffer_.end(), recv_buf.begin(), recv_buf.begin() + bytes);
        } else if (bytes == 0) {
            // Peer cleanly closed connection
            close();
            break;
        } else {
#ifdef _WIN32
            const int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                break;
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
#endif
            // Connection error / drop
            close();
            break;
        }
    }

    // Extract length-prefixed messages
    while (rx_buffer_.size() >= 4) {
        const uint32_t payload_len = endian::read_u32_be(rx_buffer_.data());
        if (payload_len > kMaxControlMessageSize) {
            close();
            break;
        }

        if (rx_buffer_.size() < 4 + static_cast<size_t>(payload_len)) {
            break;  // Incomplete frame, wait for more data
        }

        const std::string_view json_view(
            reinterpret_cast<const char*>(rx_buffer_.data() + 4),
            payload_len
        );

        auto msg_opt = parse_control_message(json_view);
        if (msg_opt.has_value()) {
            result.push_back(std::move(msg_opt.value()));
        }

        rx_buffer_.erase(rx_buffer_.begin(), rx_buffer_.begin() + 4 + static_cast<ptrdiff_t>(payload_len));
    }

    return result;
}

void TcpStream::close() noexcept {
    if (is_connected()) {
        const auto socket_fd = static_cast<sock_t>(socket_fd_);
#ifdef _WIN32
        closesocket(socket_fd);
#else
        ::close(socket_fd);
#endif
        socket_fd_ = -1;
    }
}

bool TcpStream::is_connected() const noexcept {
    return (socket_fd_ != -1);
}

Endpoint TcpStream::peer() const {
    return peer_;
}

// -----------------------------------------------------------------------------
// TcpListener
// -----------------------------------------------------------------------------

TcpListener::TcpListener() {
    ensure_winsock_initialized();
}

TcpListener::~TcpListener() {
    close();
}

TcpListener::TcpListener(TcpListener&& other) noexcept
    : socket_fd_(other.socket_fd_), port_(other.port_) {
    other.socket_fd_ = -1;
    other.port_ = 0;
}

TcpListener& TcpListener::operator=(TcpListener&& other) noexcept {
    if (this != &other) {
        close();
        socket_fd_ = other.socket_fd_;
        port_ = other.port_;
        other.socket_fd_ = -1;
        other.port_ = 0;
    }
    return *this;
}

bool TcpListener::listen(uint16_t port, std::string_view interface_ip) {
    close();

    const sock_t socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd == kInvalidSocket) {
        return false;
    }

    int opt = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (interface_ip == "0.0.0.0" || interface_ip.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        std::string ip_str(interface_ip);
        if (inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr) <= 0) {
#ifdef _WIN32
            closesocket(socket_fd);
#else
            ::close(socket_fd);
#endif
            return false;
        }
    }

    if (::bind(socket_fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == kSocketError) {
#ifdef _WIN32
        closesocket(socket_fd);
#else
        ::close(socket_fd);
#endif
        return false;
    }

    if (::listen(socket_fd, SOMAXCONN) == kSocketError) {
#ifdef _WIN32
        closesocket(socket_fd);
#else
        ::close(socket_fd);
#endif
        return false;
    }

    set_nonblocking(socket_fd, true);

    uint16_t actual_port = port;
    if (actual_port == 0) {
        sockaddr_in bound_addr{};
#ifdef _WIN32
        int bound_len = sizeof(bound_addr);
#else
        socklen_t bound_len = sizeof(bound_addr);
#endif
        if (getsockname(socket_fd, reinterpret_cast<sockaddr*>(&bound_addr), &bound_len) == 0) {
            actual_port = ntohs(bound_addr.sin_port);
        }
    }

    socket_fd_ = static_cast<intptr_t>(socket_fd);
    port_ = actual_port;
    return true;
}

std::unique_ptr<TcpStream> TcpListener::accept_client() const {
    if (!is_listening()) {
        return nullptr;
    }

    const auto socket_fd = static_cast<sock_t>(socket_fd_);
    sockaddr_in client_addr{};
#ifdef _WIN32
    int addr_len = sizeof(client_addr);
#else
    socklen_t addr_len = sizeof(client_addr);
#endif

    const sock_t client_fd = accept(socket_fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
    if (client_fd == kInvalidSocket) {
        return nullptr;
    }

    std::array<char, INET_ADDRSTRLEN> ip_buf{};
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf.data(), sizeof(ip_buf));

    const Endpoint peer_endpoint{
        .address = ip_buf.data(),
        .port = ntohs(client_addr.sin_port)
    };

    return std::make_unique<TcpStream>(static_cast<intptr_t>(client_fd), peer_endpoint);
}

void TcpListener::close() noexcept {
    if (is_listening()) {
        const auto socket_fd = static_cast<sock_t>(socket_fd_);
#ifdef _WIN32
        closesocket(socket_fd);
#else
        ::close(socket_fd);
#endif
        socket_fd_ = -1;
        port_ = 0;
    }
}

bool TcpListener::is_listening() const noexcept {
    return (socket_fd_ != -1);
}

uint16_t TcpListener::port() const {
    return port_;
}

}  // namespace chorus
