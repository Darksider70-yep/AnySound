#include <chorus/session/pin_authenticator.hpp>

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

namespace chorus {

namespace {

constexpr int kMaxPinValue = 9999;

uint64_t get_current_epoch_sec() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(now).count());
}

}  // namespace

PinAuthenticator::PinAuthenticator(std::string_view fixed_pin,
                                   uint32_t max_attempts_per_minute,
                                   std::chrono::seconds window_duration)
    : max_attempts_per_minute_(max_attempts_per_minute),
      window_duration_sec_(static_cast<uint64_t>(window_duration.count())) {
    if (!fixed_pin.empty()) {
        pin_ = std::string(fixed_pin);
    } else {
        (void)generate_random_pin();
    }
}

std::string PinAuthenticator::generate_random_pin() {
    std::random_device random_device_inst;
    std::mt19937 gen(random_device_inst());
    std::uniform_int_distribution<int> dist(0, kMaxPinValue);

    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << dist(gen);
    pin_ = oss.str();
    return pin_;
}

void PinAuthenticator::set_pin(std::string_view pin) {
    pin_ = std::string(pin);
}

std::string PinAuthenticator::pin() const {
    return pin_;
}

void PinAuthenticator::prune_expired_attempts(std::string_view client_ip, uint64_t now_sec) noexcept {
    const std::string ip_key(client_ip);
    const auto found_it = failed_attempts_.find(ip_key);
    if (found_it == failed_attempts_.end()) {
        return;
    }

    auto& deq = found_it->second;
    while (!deq.empty() && (now_sec >= deq.front()) && ((now_sec - deq.front()) >= window_duration_sec_)) {
        deq.pop_front();
    }

    if (deq.empty()) {
        failed_attempts_.erase(found_it);
    }
}

PinAuthResult PinAuthenticator::verify(std::string_view client_ip,
                                       std::string_view submitted_pin,
                                       uint64_t current_time_sec) noexcept {
    const uint64_t now_sec = (current_time_sec > 0) ? current_time_sec : get_current_epoch_sec();
    prune_expired_attempts(client_ip, now_sec);

    const std::string ip_key(client_ip);
    const auto found_it = failed_attempts_.find(ip_key);
    if (found_it != failed_attempts_.end() && found_it->second.size() >= max_attempts_per_minute_) {
        return PinAuthResult::RateLimited;
    }

    // Check PIN matching
    if (submitted_pin == pin_) {
        failed_attempts_.erase(ip_key);
        return PinAuthResult::Success;
    }

    failed_attempts_[ip_key].push_back(now_sec);
    if (failed_attempts_[ip_key].size() >= max_attempts_per_minute_) {
        return PinAuthResult::RateLimited;
    }

    return PinAuthResult::BadPin;
}

void PinAuthenticator::reset() noexcept {
    failed_attempts_.clear();
}

}  // namespace chorus
