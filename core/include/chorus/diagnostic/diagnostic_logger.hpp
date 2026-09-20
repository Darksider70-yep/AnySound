#pragma once

#include <chorus/core.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace chorus {

enum class DiagnosticEventType : uint8_t {
    SessionStarted,
    SessionStopped,
    ClientConnected,
    ClientDisconnected,
    ClockSyncAcquired,
    HardResyncTriggered,
    PacketLossSpike,
    BufferUnderrun,
    LatencyTuned
};

struct DiagnosticEvent {
    uint64_t timestamp_us{0};
    DiagnosticEventType type{DiagnosticEventType::SessionStarted};
    std::string summary;
    std::string details;
};

/// @brief Thread-safe ring buffer diagnostic event logger for troubleshooting, telemetry, and export.
class DiagnosticLogger {
public:
    explicit DiagnosticLogger(size_t max_events = 200);
    ~DiagnosticLogger() = default;

    DiagnosticLogger(const DiagnosticLogger&) = delete;
    DiagnosticLogger& operator=(const DiagnosticLogger&) = delete;
    DiagnosticLogger(DiagnosticLogger&&) = delete;
    DiagnosticLogger& operator=(DiagnosticLogger&&) = delete;

    /// @brief Records a new diagnostic event.
    void log(DiagnosticEventType type, std::string_view summary, std::string_view details = "");

    /// @brief Returns all buffered events.
    [[nodiscard]] std::vector<DiagnosticEvent> events() const;

    /// @brief Exports buffered diagnostics as a structured JSON string.
    [[nodiscard]] std::string export_json() const;

    /// @brief Clears all logged events.
    void clear();

    /// @brief Global singleton accessor.
    static DiagnosticLogger& instance();

private:
    size_t max_events_{200};
    mutable std::mutex mutex_;
    std::deque<DiagnosticEvent> buffer_;
};

}  // namespace chorus
