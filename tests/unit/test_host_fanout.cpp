// NOLINTBEGIN
#include <catch2/catch_test_macros.hpp>
#include <chorus/session/client_session.hpp>
#include <chorus/session/host_session.hpp>

#include <chrono>
#include <thread>
#include <vector>

TEST_CASE("HostSession multi-client authentication and fan-out broadcast", "[session][fanout]") {
    constexpr uint16_t kTestTcpPort = 47850;
    constexpr uint16_t kTestUdpPort = 47851;
    constexpr std::string_view kTestPin = "7788";

    chorus::HostSession host(300);
    REQUIRE(host.start(kTestTcpPort, kTestUdpPort, kTestPin));
    REQUIRE(host.pin() == "7788");

    // Connect Client 1 (Correct PIN)
    chorus::ClientSession client1("Client 1");
    REQUIRE(client1.connect("127.0.0.1", kTestTcpPort, 47860, kTestPin));

    // Connect Client 2 (Correct PIN)
    chorus::ClientSession client2("Client 2");
    REQUIRE(client2.connect("127.0.0.1", kTestTcpPort, 47861, kTestPin));

    // Connect Client 3 (Wrong PIN)
    chorus::ClientSession client3_bad("Client 3 Bad");
    REQUIRE(client3_bad.connect("127.0.0.1", kTestTcpPort, 47862, "0000"));

    // Tick host and clients for 100ms
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        host.update();
        client1.update();
        client2.update();
        client3_bad.update();
    }

    // Client 1 and 2 should be authenticated; Client 3 rejected
    REQUIRE(host.authenticated_client_count() == 2);
    REQUIRE(client3_bad.state() == chorus::ClientSessionState::Rejected);
    REQUIRE(client3_bad.rejection_reason() == "bad_pin");

    // Broadcast a 20ms audio frame from host
    std::vector<float> frame_pcm(chorus::kFloatsPerFrame, 0.2F);
    const size_t sent_count = host.broadcast_audio_frame(frame_pcm);
    REQUIRE(sent_count == 2);  // Broadcasted to both authenticated clients

    // Set client volume and mute from host
    const auto clients = host.client_list();
    REQUIRE(clients.size() == 2);
    REQUIRE(host.set_client_volume(clients[0].id, 0.75F));
    REQUIRE(host.set_client_mute(clients[1].id, true));

    // Tick client updates
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        host.update();
        client1.update();
        client2.update();
    }

    REQUIRE(client1.volume() == 0.75F);
    REQUIRE(client2.is_muted() == true);
}
// NOLINTEND
