// NOLINTBEGIN
#include <catch2/catch_test_macros.hpp>
#include <chorus/net/udp_socket.hpp>

#include <array>
#include <vector>

namespace {
constexpr size_t kU64BufferSize = 8;
constexpr uint16_t kTestU16 = 0x1234;
constexpr uint32_t kTestU32 = 0x12345678;
constexpr uint64_t kTestU64 = 0x0123456789ABCDEFULL;
constexpr int kRecvTimeoutMs = 500;
constexpr size_t kRecvBufferSize = 64;
}  // namespace

TEST_CASE("Endian helpers serialization and deserialization", "[net][endian]") {
    std::array<uint8_t, kU64BufferSize> buffer{};

    // 16-bit
    chorus::endian::write_u16_be(buffer.data(), kTestU16);
    REQUIRE(chorus::endian::read_u16_be(buffer.data()) == kTestU16);
    REQUIRE(buffer[0] == 0x12);
    REQUIRE(buffer[1] == 0x34);

    // 32-bit
    chorus::endian::write_u32_be(buffer.data(), kTestU32);
    REQUIRE(chorus::endian::read_u32_be(buffer.data()) == kTestU32);
    REQUIRE(buffer[0] == 0x12);
    REQUIRE(buffer[1] == 0x34);
    REQUIRE(buffer[2] == 0x56);
    REQUIRE(buffer[3] == 0x78);

    // 64-bit
    chorus::endian::write_u64_be(buffer.data(), kTestU64);
    REQUIRE(chorus::endian::read_u64_be(buffer.data()) == kTestU64);
}

TEST_CASE("UDP loopback send and receive", "[net][udp]") {
    chorus::UdpSocket receiver;
    REQUIRE(receiver.bind(0, "127.0.0.1"));
    const uint16_t receiver_port = receiver.local_port();
    REQUIRE(receiver_port > 0);
    REQUIRE(receiver.set_recv_timeout_ms(kRecvTimeoutMs));

    chorus::UdpSocket sender;
    const std::vector<uint8_t> payload = {0x43, 0x48, 0x01, 0x01, 0xDE, 0xAD, 0xBE, 0xEF};

    const chorus::Endpoint dest{.address = "127.0.0.1", .port = receiver_port};
    REQUIRE(sender.send_to(payload, dest));

    std::array<uint8_t, kRecvBufferSize> recv_buf{};
    chorus::Endpoint sender_out;
    const int bytes_recvd = receiver.receive_from(recv_buf, sender_out);

    REQUIRE(bytes_recvd == static_cast<int>(payload.size()));
    REQUIRE(std::vector<uint8_t>(recv_buf.begin(), recv_buf.begin() + bytes_recvd) == payload);
}
// NOLINTEND
