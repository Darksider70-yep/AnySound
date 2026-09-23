#include <chorus/net/udp_socket.hpp>

#include <array>

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

namespace {
constexpr uint32_t kShift24 = 24;
constexpr uint32_t kShift16 = 16;
constexpr uint32_t kShift8 = 8;
constexpr uint32_t kByteMask = 0xFF;
constexpr int kBitsPerByte = 8;
constexpr int kU64ByteCount = 8;
}  // namespace

void write_u16_be(uint8_t* dst, uint16_t val) noexcept {
    dst[0] = static_cast<uint8_t>((val >> kShift8) & kByteMask);
    dst[1] = static_cast<uint8_t>(val & kByteMask);
}

void write_u32_be(uint8_t* dst, uint32_t val) noexcept {
    dst[0] = static_cast<uint8_t>((val >> kShift24) & kByteMask);
    dst[1] = static_cast<uint8_t>((val >> kShift16) & kByteMask);
    dst[2] = static_cast<uint8_t>((val >> kShift8) & kByteMask);
    dst[3] = static_cast<uint8_t>(val & kByteMask);
}

void write_u64_be(uint8_t* dst, uint64_t val) noexcept {
    for (int i = kU64ByteCount - 1; i >= 0; --i) {
        dst[(kU64ByteCount - 1) - i] = static_cast<uint8_t>((val >> (i * kBitsPerByte)) & kByteMask);
    }
}

uint16_t read_u16_be(const uint8_t* src) noexcept {
    return static_cast<uint16_t>((static_cast<uint16_t>(src[0]) << kShift8) |
                                 static_cast<uint16_t>(src[1]));
}

uint32_t read_u32_be(const uint8_t* src) noexcept {
    return (static_cast<uint32_t>(src[0]) << kShift24) |
           (static_cast<uint32_t>(src[1]) << kShift16) |
           (static_cast<uint32_t>(src[2]) << kShift8) |
           static_cast<uint32_t>(src[3]);
}

uint64_t read_u64_be(const uint8_t* src) noexcept {
    uint64_t val = 0;
    for (int i = 0; i < kU64ByteCount; ++i) {
        val = (val << kBitsPerByte) | static_cast<uint64_t>(src[i]);
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
        WinsockInit(const WinsockInit&) = delete;
        WinsockInit& operator=(const WinsockInit&) = delete;
        WinsockInit(WinsockInit&&) = delete;
        WinsockInit& operator=(WinsockInit&&) = delete;
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
    const sock_t initial_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (initial_socket != kInvalidSock) {
        socket_handle_ = static_cast<intptr_t>(initial_socket);
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
        const auto socket_fd = static_cast<sock_t>(socket_handle_);
#ifdef _WIN32
        closesocket(socket_fd);
#else
        ::close(socket_fd);
#endif
        socket_handle_ = -1;
    }
}

bool UdpSocket::bind(uint16_t port, std::string_view interface_ip) {
    if (!is_valid()) {
        return false;
    }

    const auto socket_fd = static_cast<sock_t>(socket_handle_);

#ifdef _WIN32
    const BOOL reuse = TRUE;
    (void)setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    const int reuse = 1;
    (void)setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
    (void)setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (interface_ip == "0.0.0.0" || interface_ip.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        std::string ip_str(interface_ip);
        if (inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr) <= 0) {
            return false;
        }
    }

    const int result_code = ::bind(socket_fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    return (result_code != kSocketError);
}

bool UdpSocket::set_recv_timeout_ms(int timeout_ms) {
    if (!is_valid()) {
        return false;
    }
    const auto socket_fd = static_cast<sock_t>(socket_handle_);
#ifdef _WIN32
    const auto time_val = static_cast<DWORD>(timeout_ms);
    const int result_code = setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO,
                                      reinterpret_cast<const char*>(&time_val), sizeof(time_val));
#else
    struct timeval time_val{};
    time_val.tv_sec = timeout_ms / 1000;
    time_val.tv_usec = (timeout_ms % 1000) * 1000;
    const int result_code = setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &time_val, sizeof(time_val));
#endif
    return (result_code != kSocketError);
}

bool UdpSocket::enable_broadcast(bool enable) {
    if (!is_valid()) {
        return false;
    }
    const auto socket_fd = static_cast<sock_t>(socket_handle_);
#ifdef _WIN32
    const BOOL opt = enable ? TRUE : FALSE;
    const int result_code = setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST,
                                       reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    const int opt = enable ? 1 : 0;
    const int result_code = setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST,
                                       &opt, sizeof(opt));
#endif
    return (result_code != kSocketError);
}

bool UdpSocket::join_multicast_group(std::string_view group_ip) {
    if (!is_valid()) {
        return false;
    }
    struct ip_mreq mreq{};
    std::string ip_str(group_ip);
    if (inet_pton(AF_INET, ip_str.c_str(), &mreq.imr_multiaddr) <= 0) {
        return false;
    }
    mreq.imr_interface.s_addr = INADDR_ANY;
    const auto socket_fd = static_cast<sock_t>(socket_handle_);
    const int result_code = setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                                       reinterpret_cast<const char*>(&mreq), sizeof(mreq));
    return (result_code != kSocketError);
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

    const auto socket_fd = static_cast<sock_t>(socket_handle_);
    const int sent = sendto(
        socket_fd,
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

    const auto socket_fd = static_cast<sock_t>(socket_handle_);
    const int received = recvfrom(
        socket_fd,
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

    std::array<char, INET_ADDRSTRLEN> ip_buf{};
    inet_ntop(AF_INET, &addr.sin_addr, ip_buf.data(), sizeof(ip_buf));
    sender_out.address = ip_buf.data();
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
    const auto socket_fd = static_cast<sock_t>(socket_handle_);
    if (getsockname(socket_fd, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
        return ntohs(addr.sin_port);
    }
    return 0;
}

}  // namespace chorus
