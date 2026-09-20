#include <chorus/net/udp_socket.hpp>

#include <cstring>
#include <iostream>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using sock_t = SOCKET;
    constexpr sock_t kInvalidSock = INVALID_SOCKET;
    constexpr int kSocketError = SOCKET_ERROR;
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <unistd.h>
    using sock_t = int;
    constexpr sock_t kInvalidSock = -1;
    constexpr int kSocketError = -1;
#endif

namespace chorus {

namespace endian {

void write_u16_be(uint8_t* dst, uint16_t val) noexcept {
    dst[0] = static_cast<uint8_t>((val >> 8) & 0xFF);
    dst[1] = static_cast<uint8_t>(val & 0xFF);
}

void write_u32_be(uint8_t* dst, uint32_t val) noexcept {
    dst[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
    dst[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    dst[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    dst[3] = static_cast<uint8_t>(val & 0xFF);
}

void write_u64_be(uint8_t* dst, uint64_t val) noexcept {
    for (int i = 7; i >= 0; --i) {
        dst[7 - i] = static_cast<uint8_t>((val >> (i * 8)) & 0xFF);
    }
}

uint16_t read_u16_be(const uint8_t* src) noexcept {
    return static_cast<uint16_t>((static_cast<uint16_t>(src[0]) << 8) |
                                 static_cast<uint16_t>(src[1]));
}

uint32_t read_u32_be(const uint8_t* src) noexcept {
    return (static_cast<uint32_t>(src[0]) << 24) |
           (static_cast<uint32_t>(src[1]) << 16) |
           (static_cast<uint32_t>(src[2]) << 8) |
           static_cast<uint32_t>(src[3]);
}

uint64_t read_u64_be(const uint8_t* src) noexcept {
    uint64_t val = 0;
    for (int i = 0; i < 8; ++i) {
        val = (val << 8) | static_cast<uint64_t>(src[i]);
    }
    return val;
}

}  // namespace endian

namespace {
#ifdef _WIN32
    struct WinsockInit {
        WinsockInit() {
            WSADATA wsa_data;
            WSAStartup(MAKEWORD(2, 2), &wsa_data);
        }
        ~WinsockInit() {
            WSACleanup();
        }
    };

    void ensure_winsock_initialized() {
        static WinsockInit init;
        (void)init;
    }
#else
    void ensure_winsock_initialized() {}
#endif
}  // namespace

UdpSocket::UdpSocket() {
    ensure_winsock_initialized();
    sock_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s != kInvalidSock) {
        socket_handle_ = static_cast<intptr_t>(s);
    }
}

UdpSocket::~UdpSocket() {
    close();
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept : socket_handle_(other.socket_handle_) {
    other.socket_handle_ = -1;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        socket_handle_ = other.socket_handle_;
        other.socket_handle_ = -1;
    }
    return *this;
}

bool UdpSocket::is_valid() const noexcept {
    return socket_handle_ != -1 && static_cast<sock_t>(socket_handle_) != kInvalidSock;
}

void UdpSocket::close() noexcept {
    if (is_valid()) {
        const sock_t s = static_cast<sock_t>(socket_handle_);
#ifdef _WIN32
        closesocket(s);
#else
        ::close(s);
#endif
        socket_handle_ = -1;
    }
}

bool UdpSocket::bind(uint16_t port, std::string_view ip) {
    if (!is_valid()) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (ip == "0.0.0.0" || ip.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        std::string ip_str(ip);
        if (inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr) <= 0) {
            return false;
        }
    }

    const sock_t s = static_cast<sock_t>(socket_handle_);
    const int rc = ::bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    return (rc != kSocketError);
}

bool UdpSocket::set_recv_timeout_ms(int timeout_ms) {
    if (!is_valid()) {
        return false;
    }
    const sock_t s = static_cast<sock_t>(socket_handle_);
#ifdef _WIN32
    DWORD tv = static_cast<DWORD>(timeout_ms);
    const int rc = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    const int rc = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    return (rc != kSocketError);
}

bool UdpSocket::send_to(std::span<const uint8_t> data, const Endpoint& dest) {
    if (!is_valid() || data.empty()) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest.port);
    if (inet_pton(AF_INET, dest.address.c_str(), &addr.sin_addr) <= 0) {
        return false;
    }

    const sock_t s = static_cast<sock_t>(socket_handle_);
    const int sent = sendto(
        s,
        reinterpret_cast<const char*>(data.data()),
        static_cast<int>(data.size()),
        0,
        reinterpret_cast<const sockaddr*>(&addr),
        sizeof(addr)
    );

    return (sent == static_cast<int>(data.size()));
}

int UdpSocket::receive_from(std::span<uint8_t> buffer, Endpoint& sender_out) {
    if (!is_valid() || buffer.empty()) {
        return -1;
    }

    sockaddr_in addr{};
#ifdef _WIN32
    int addr_len = sizeof(addr);
#else
    socklen_t addr_len = sizeof(addr);
#endif

    const sock_t s = static_cast<sock_t>(socket_handle_);
    const int received = recvfrom(
        s,
        reinterpret_cast<char*>(buffer.data()),
        static_cast<int>(buffer.size()),
        0,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_len
    );

    if (received < 0) {
#ifdef _WIN32
        const int err = WSAGetLastError();
        if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
            return 0;  // timeout
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // timeout
        }
#endif
        return -1;
    }

    char ip_buf[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &addr.sin_addr, ip_buf, sizeof(ip_buf));
    sender_out.address = ip_buf;
    sender_out.port = ntohs(addr.sin_port);

    return received;
}

uint16_t UdpSocket::local_port() const {
    if (!is_valid()) {
        return 0;
    }
    sockaddr_in addr{};
#ifdef _WIN32
    int addr_len = sizeof(addr);
#else
    socklen_t addr_len = sizeof(addr);
#endif
    const sock_t s = static_cast<sock_t>(socket_handle_);
    if (getsockname(s, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
        return ntohs(addr.sin_port);
    }
    return 0;
}

}  // namespace chorus
