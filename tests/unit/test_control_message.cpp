// NOLINTBEGIN
#include <catch2/catch_test_macros.hpp>
#include <chorus/net/control_message.hpp>

TEST_CASE("ControlMessage Hello and Welcome serialization/deserialization", "[control][proto]") {
    const chorus::HelloMessage hello{
        .name = "Test Laptop",
        .platform = "Linux",
        .protocol = 1,
        .pin = "4242"
    };

    const auto vec = chorus::serialize_control_message_vec(hello);
    REQUIRE(vec.size() > 4);

    const std::string_view json_view(reinterpret_cast<const char*>(vec.data() + 4), vec.size() - 4);
    const auto parsed = chorus::parse_control_message(json_view);
    REQUIRE(parsed.has_value());
    REQUIRE(std::holds_alternative<chorus::HelloMessage>(*parsed));

    const auto& res_hello = std::get<chorus::HelloMessage>(*parsed);
    REQUIRE(res_hello.name == "Test Laptop");
    REQUIRE(res_hello.platform == "Linux");
    REQUIRE(res_hello.protocol == 1);
    REQUIRE(res_hello.pin == "4242");
}

TEST_CASE("ControlMessage Reject and Stats roundtrip", "[control][proto]") {
    const chorus::RejectMessage reject{.reason = "bad_pin"};
    const auto rej_vec = chorus::serialize_control_message_vec(reject);
    const std::string_view rej_view(reinterpret_cast<const char*>(rej_vec.data() + 4), rej_vec.size() - 4);
    const auto parsed_rej = chorus::parse_control_message(rej_view);
    REQUIRE(parsed_rej.has_value());
    REQUIRE(std::holds_alternative<chorus::RejectMessage>(*parsed_rej));
    REQUIRE(std::get<chorus::RejectMessage>(*parsed_rej).reason == "bad_pin");

    const chorus::ClientStatsMessage stats{
        .sync_error_us = -150,
        .skew_ppm = 12.5,
        .underruns = 0,
        .late = 0,
        .loss_pct = 0.5,
        .buffer_ms = 300
    };
    const auto stats_vec = chorus::serialize_control_message_vec(stats);
    const std::string_view stats_view(reinterpret_cast<const char*>(stats_vec.data() + 4), stats_vec.size() - 4);
    const auto parsed_stats = chorus::parse_control_message(stats_view);
    REQUIRE(parsed_stats.has_value());
    REQUIRE(std::holds_alternative<chorus::ClientStatsMessage>(*parsed_stats));
    const auto& s = std::get<chorus::ClientStatsMessage>(*parsed_stats);
    REQUIRE(s.sync_error_us == -150);
    REQUIRE(s.skew_ppm == 12.5);
    REQUIRE(s.buffer_ms == 300);
}
// NOLINTEND
