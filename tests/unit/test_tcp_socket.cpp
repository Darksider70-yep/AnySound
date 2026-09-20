// NOLINTBEGIN
#include <catch2/catch_test_macros.hpp>
#include <chorus/net/tcp_socket.hpp>

#include <chrono>
#include <thread>

TEST_CASE("TcpListener and TcpStream loopback connection and framing", "[net][tcp]") {
    chorus::TcpListener listener;
    REQUIRE(listener.listen(0, "127.0.0.1"));
    const uint16_t port = listener.port();
    REQUIRE(port > 0);

    chorus::TcpStream client;
    REQUIRE(client.connect(chorus::Endpoint{.address = "127.0.0.1", .port = port}, 1000));
    REQUIRE(client.is_connected());

    // Accept on server
    auto server_stream = listener.accept_client();
    REQUIRE(server_stream != nullptr);
    REQUIRE(server_stream->is_connected());

    // Send message from client to server
    const chorus::HelloMessage hello{.name = "Framing Test", .platform = "Win", .protocol = 1, .pin = "9999"};
    REQUIRE(client.send_message(hello));

    // Wait a brief moment for OS network stack
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Read on server
    const auto msgs = server_stream->read_messages();
    REQUIRE(msgs.size() == 1);
    REQUIRE(std::holds_alternative<chorus::HelloMessage>(msgs[0]));
    REQUIRE(std::get<chorus::HelloMessage>(msgs[0]).name == "Framing Test");

    // Server sends welcome to client
    const chorus::WelcomeMessage welcome{.session_id = 42, .udp_port = 47801};
    REQUIRE(server_stream->send_message(welcome));

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const auto client_msgs = client.read_messages();
    REQUIRE(client_msgs.size() == 1);
    REQUIRE(std::holds_alternative<chorus::WelcomeMessage>(client_msgs[0]));
    REQUIRE(std::get<chorus::WelcomeMessage>(client_msgs[0]).session_id == 42);
}
// NOLINTEND
