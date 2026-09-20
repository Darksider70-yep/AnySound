#include <chorus/diagnostic/diagnostic_logger.hpp>

#include <nlohmann/json.hpp>

#include <chrono>

namespace chorus {

namespace {

std::string event_type_to_string(DiagnosticEventType type) {
    switch (type) {
        case DiagnosticEventType::SessionStarted: return "session_started";
        case DiagnosticEventType::SessionStopped: return "session_stopped";
        case DiagnosticEventType::ClientConnected: return "client_connected";
        case DiagnosticEventType::ClientDisconnected: return "client_disconnected";
        case DiagnosticEventType::ClockSyncAcquired: return "clock_sync_acquired";
        case DiagnosticEventType::HardResyncTriggered: return "hard_resync_triggered";
        case DiagnosticEventType::PacketLossSpike: return "packet_loss_spike";
        case DiagnosticEventType::BufferUnderrun: return "buffer_underrun";
        case DiagnosticEventType::LatencyTuned: return "latency_tuned";
    }
    return "unknown";
}

}  // namespace

DiagnosticLogger::DiagnosticLogger(size_t max_events)
    : max_events_(max_events > 10 ? max_events : 10) {}

DiagnosticLogger& DiagnosticLogger::instance() {
    static DiagnosticLogger s_instance(500);
    return s_instance;
}

void DiagnosticLogger::log(DiagnosticEventType type, std::string_view summary, std::string_view details) {
    auto now_us = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );

    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.push_back(DiagnosticEvent{
        .timestamp_us = now_us,
        .type = type,
        .summary = std::string(summary),
        .details = std::string(details)
    });

    while (buffer_.size() > max_events_) {
        buffer_.pop_front();
    }
}

std::vector<DiagnosticEvent> DiagnosticLogger::events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<DiagnosticEvent>(buffer_.begin(), buffer_.end());
}

std::string DiagnosticLogger::export_json() const {
    std::lock_guard<std::mutex> lock(mutex_);

    nlohmann::json j = nlohmann::json::object();
    j["timestamp_now_us"] = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();

    auto events_arr = nlohmann::json::array();
    for (const auto& ev : buffer_) {
        events_arr.push_back({
            {"timestamp_us", ev.timestamp_us},
            {"type", event_type_to_string(ev.type)},
            {"summary", ev.summary},
            {"details", ev.details}
        });
    }
    j["events"] = events_arr;
    j["total_events"] = buffer_.size();

    return j.dump(2);
}

void DiagnosticLogger::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
}

}  // namespace chorus
