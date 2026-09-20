#include <chorus/diagnostic/diagnostic_logger.hpp>
#include <chorus/playback/delayed_host_renderer.hpp>

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace chorus;

TEST_CASE("DelayedHostRenderer configuration and frame submission", "[delayed_host]") {
    DelayedHostRenderer renderer(300);

    renderer.set_volume(0.8F);
    renderer.set_mute(false);
    renderer.set_target_latency_ms(250);

    std::vector<float> pcm(static_cast<size_t>(kFloatsPerFrame), 0.5F);
    // Submitting frames while not running does not crash
    renderer.submit_frame(pcm, 1000000ULL);
    REQUIRE_FALSE(renderer.is_running());
}

TEST_CASE("DiagnosticLogger event recording and JSON export", "[diagnostic]") {
    auto& logger = DiagnosticLogger::instance();
    logger.clear();

    logger.log(DiagnosticEventType::SessionStarted, "Test session started", "PIN=1234");
    logger.log(DiagnosticEventType::ClientConnected, "Client 1 connected", "192.168.1.50");
    logger.log(DiagnosticEventType::HardResyncTriggered, "Hard resync triggered", "skew=45ppm");

    auto events = logger.events();
    REQUIRE(events.size() == 3);
    REQUIRE(events[0].summary == "Test session started");
    REQUIRE(events[1].summary == "Client 1 connected");
    REQUIRE((events[2].summary == "Hard Resync Triggered" || events[2].summary == "Hard resync triggered"));

    std::string json_str = logger.export_json();
    REQUIRE_FALSE(json_str.empty());
    REQUIRE(json_str.find("session_started") != std::string::npos);
    REQUIRE(json_str.find("PIN=1234") != std::string::npos);
}
