// NOLINTBEGIN
#include <catch2/catch_test_macros.hpp>
#include <chorus/session/pin_authenticator.hpp>

TEST_CASE("PinAuthenticator verification and rate-limiting", "[session][pin]") {
    chorus::PinAuthenticator auth("1234", 5, std::chrono::seconds(60));
    REQUIRE(auth.pin() == "1234");

    const std::string ip = "192.168.1.50";

    // 1. Correct PIN succeeds immediately
    REQUIRE(auth.verify(ip, "1234", 1000) == chorus::PinAuthResult::Success);

    // 2. Failed attempts increment counter
    REQUIRE(auth.verify(ip, "0000", 1001) == chorus::PinAuthResult::BadPin);
    REQUIRE(auth.verify(ip, "1111", 1002) == chorus::PinAuthResult::BadPin);
    REQUIRE(auth.verify(ip, "2222", 1003) == chorus::PinAuthResult::BadPin);
    REQUIRE(auth.verify(ip, "3333", 1004) == chorus::PinAuthResult::BadPin);

    // 5th failed attempt triggers RateLimited
    REQUIRE(auth.verify(ip, "4444", 1005) == chorus::PinAuthResult::RateLimited);

    // Even if user now provides the correct PIN within the 60s window, rate-limiting blocks them
    REQUIRE(auth.verify(ip, "1234", 1010) == chorus::PinAuthResult::RateLimited);

    // After 60 seconds (1001 + 61 = 1062), expired attempts fall out of the sliding window
    REQUIRE(auth.verify(ip, "1234", 1070) == chorus::PinAuthResult::Success);
}
// NOLINTEND
