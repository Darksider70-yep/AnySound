#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <string_view>

namespace chorus {

inline constexpr uint32_t kDefaultMaxAttemptsPerMinute = 5;
inline constexpr uint64_t kDefaultWindowDurationSec = 60;

enum class PinAuthResult : uint8_t {
    Success,
    BadPin,
    RateLimited
};

/// @brief 4-digit PIN generation, verification, and per-IP rate limiting.
/// Pure logic (zero OS dependencies, fully testable).
class PinAuthenticator {
public:
    explicit PinAuthenticator(std::string_view fixed_pin = "",
                              uint32_t max_attempts_per_minute = kDefaultMaxAttemptsPerMinute,
                              std::chrono::seconds window_duration = std::chrono::seconds(kDefaultWindowDurationSec));
    ~PinAuthenticator() = default;

    PinAuthenticator(const PinAuthenticator&) = delete;
    PinAuthenticator& operator=(const PinAuthenticator&) = delete;
    PinAuthenticator(PinAuthenticator&&) noexcept = default;
    PinAuthenticator& operator=(PinAuthenticator&&) noexcept = default;

    /// @brief Generates and sets a new random 4-digit PIN.
    /// @return The newly generated 4-digit PIN string.
    std::string generate_random_pin();

    /// @brief Sets the session PIN explicitly.
    void set_pin(std::string_view pin);

    /// @brief Returns the current session PIN.
    [[nodiscard]] std::string pin() const;

    /// @brief Verifies a submitted PIN against the session PIN for a client IP.
    /// @param client_ip The IPv4 address of the connecting client.
    /// @param submitted_pin The PIN provided by the client.
    /// @param current_time_sec Current timestamp in seconds since epoch.
    [[nodiscard]] PinAuthResult verify(std::string_view client_ip,
                                       std::string_view submitted_pin,
                                       uint64_t current_time_sec = 0) noexcept;

    /// @brief Resets failed attempt history and state.
    void reset() noexcept;

private:
    void prune_expired_attempts(std::string_view client_ip, uint64_t now_sec) noexcept;

    std::string pin_{"0000"};
    uint32_t max_attempts_per_minute_{kDefaultMaxAttemptsPerMinute};
    uint64_t window_duration_sec_{kDefaultWindowDurationSec};

    // Maps client IP string -> history of failed attempt timestamps (seconds)
    std::map<std::string, std::deque<uint64_t>> failed_attempts_;
};

}  // namespace chorus
