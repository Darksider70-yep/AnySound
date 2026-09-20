#include <catch2/catch_test_macros.hpp>
#include <chorus/core.hpp>

TEST_CASE("Core version returns valid string", "[core]") {
    REQUIRE_FALSE(chorus::version().empty());
    REQUIRE(chorus::version() == "0.1.0");
}

TEST_CASE("Protocol constants adhere to architecture spec", "[core][proto]") {
    REQUIRE(chorus::kProtocolVersion == 1);
    REQUIRE(chorus::kDefaultTcpControlPort == 47800);
    REQUIRE(chorus::kDefaultUdpDataPort == 47801);
}
