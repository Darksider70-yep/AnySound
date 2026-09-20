#pragma once

#include <chorus/diagnostic/diagnostic_logger.hpp>
#include <chorus/net/crypto_channel.hpp>
#include <chorus/net/discovery.hpp>
#include <chorus/playback/delayed_host_renderer.hpp>
#include <chorus/playback/spsc_ring.hpp>
#include <chorus/platform/audio_device.hpp>
#include <chorus/session/client_session.hpp>
#include <chorus/session/host_session.hpp>
#include <chorus/sync/latency_tuner.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace chorus {

enum class AppRole : uint8_t {
    Idle,
    Hosting,
    Client
};

struct AppSnapshot {
    AppRole role{AppRole::Idle};
    bool is_active{false};
    bool is_encrypted{false};
    bool delayed_host_enabled{false};
    std::string session_pin;
    std::string rejection_reason;
    float volume{1.0F};
    bool is_muted{false};
    int32_t offset_ms{0};
    std::vector<ClientInfo> connected_clients;
    std::vector<DiscoveredHost> discovered_hosts;
    ClientStatsMessage client_stats{};
    NetworkQualityAssessment network_quality{};
};

/// @brief Unified application controller facade exposing state and commands for CLI, Desktop GUI, and Android.
class AppController {
public:
    AppController();
    ~AppController();

    AppController(const AppController&) = delete;
    AppController& operator=(const AppController&) = delete;
    AppController(AppController&&) = delete;
    AppController& operator=(AppController&&) = delete;

    /// @brief Starts hosting audio stream and control server.
    [[nodiscard]] bool start_host(std::string_view pin = "",
                                  uint64_t target_latency_ms = kDefaultTargetLatencyMs,
                                  bool use_test_tone = false);

    /// @brief Stops hosting session.
    void stop_host();

    /// @brief Joins an active host.
    [[nodiscard]] bool join_host(std::string_view host_ip,
                                 uint16_t tcp_port = kDefaultTcpControlPort,
                                 std::string_view pin = "");

    /// @brief Leaves active host session.
    void leave_host();

    /// @brief Non-blocking tick called by application main loop (e.g. 50-60 Hz).
    void update();

    /// @brief Returns instantaneous state snapshot.
    [[nodiscard]] AppSnapshot snapshot() const;

    /// @brief Local volume control (0.0 to 1.0).
    void set_volume(float volume);

    /// @brief Local mute toggle.
    void set_mute(bool mute);

    /// @brief Local delay offset (+/- 500 ms).
    void set_offset_ms(int32_t offset_ms);

    /// @brief Toggles delayed local playback for the host speakers.
    void set_delayed_host(bool enable);

    /// @brief Toggles encrypted transport mode.
    void set_encrypted(bool enable);

    /// @brief Host remote volume adjustment for a client.
    bool set_client_volume(uint32_t client_id, float volume);

    /// @brief Host remote mute adjustment for a client.
    bool set_client_mute(uint32_t client_id, bool mute);

    /// @brief Host remote offset adjustment for a client.
    bool set_client_offset_ms(uint32_t client_id, int32_t offset_ms);

    /// @brief Exports full diagnostic logs in structured JSON format.
    [[nodiscard]] std::string export_diagnostics() const;

private:
    void generate_test_sine(std::span<float> out_pcm);

    AppRole role_{AppRole::Idle};
    bool use_test_tone_{false};
    bool is_encrypted_{false};
    bool delayed_host_enabled_{false};
    double tone_phase_{0.0};
    uint64_t target_latency_ms_{kDefaultTargetLatencyMs};

    HostSession host_session_;
    ClientSession client_session_;
    DiscoveryScanner discovery_scanner_;
    DiscoveryBroadcaster discovery_broadcaster_;
    DelayedHostRenderer delayed_host_renderer_;
    LatencyTuner latency_tuner_;
    CryptoChannel crypto_channel_;

    AudioCaptureDevice capture_device_;
    SpscRing<float> capture_ring_;
};

}  // namespace chorus
