#pragma once

#include <chorus/codec/opus_codec.hpp>
#include <chorus/core.hpp>
#include <chorus/net/control_message.hpp>
#include <chorus/net/tcp_socket.hpp>
#include <chorus/net/udp_socket.hpp>
#include <chorus/playback/timeline_buffer.hpp>
#include <chorus/platform/audio_device.hpp>
#include <chorus/sync/clock_estimator.hpp>
#include <chorus/sync/drift_controller.hpp>
#include <chorus/sync/jitter_buffer.hpp>
#include <chorus/sync/resampler.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace chorus {

inline constexpr uint16_t kDefaultClientDataPort = 47802;

enum class ClientSessionState : uint8_t {
    Disconnected,
    Connecting,
    Authenticating,
    Rejected,
    Synchronizing,
    Active
};

/// @brief Orchestrates client connection, TCP control, clock sync, and scheduled audio playout.
class ClientSession {
public:
    explicit ClientSession(std::string_view client_name = "Chorus Client");
    ~ClientSession();

    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;
    ClientSession(ClientSession&&) = delete;
    ClientSession& operator=(ClientSession&&) = delete;

    /// @brief Connects to host TCP control port with PIN.
    [[nodiscard]] bool connect(std::string_view host_ip,
                               uint16_t tcp_port = kDefaultTcpControlPort,
                               uint16_t udp_listen_port = kDefaultClientDataPort,
                               std::string_view pin = "");

    /// @brief Disconnects from host and stops playback.
    void disconnect();

    /// @brief Periodic non-blocking tick: processes TCP control, clock PINGs, UDP audio packets, and stats.
    void update();

    /// @brief Returns current session state.
    [[nodiscard]] ClientSessionState state() const noexcept;

    /// @brief Returns rejection reason if state is Rejected.
    [[nodiscard]] std::string rejection_reason() const;

    /// @brief Returns current volume (0.0 to 1.0).
    [[nodiscard]] float volume() const noexcept;

    /// @brief Sets local user volume.
    void set_volume(float vol) noexcept;

    /// @brief Returns mute status.
    [[nodiscard]] bool is_muted() const noexcept;

    /// @brief Sets local user mute.
    void set_mute(bool mute) noexcept;

    /// @brief Sets user delay offset (+/- 500 ms) and notifies host.
    void set_offset_ms(int32_t offset_ms);

    /// @brief Returns current delay offset in milliseconds.
    [[nodiscard]] int32_t offset_ms() const noexcept;

    /// @brief Returns latest client stats snapshot.
    [[nodiscard]] ClientStatsMessage stats() const;

    /// @brief Returns true if session is actively playing audio.
    [[nodiscard]] bool is_active() const noexcept;

private:
    void send_clock_ping();
    void process_incoming_udp();
    void drain_jitter_and_play();

    std::string client_name_;
    std::string host_ip_{"127.0.0.1"};
    uint16_t tcp_port_{kDefaultTcpControlPort};
    uint16_t udp_listen_port_{kDefaultClientDataPort};
    uint16_t host_udp_port_{kDefaultUdpDataPort};
    std::string submitted_pin_;
    std::string rejection_reason_;

    std::atomic<ClientSessionState> state_{ClientSessionState::Disconnected};
    std::atomic<float> volume_{1.0F};
    std::atomic<bool> is_muted_{false};
    std::atomic<int32_t> offset_ms_{0};

    TcpStream tcp_stream_;
    UdpSocket udp_socket_;
    OpusDecoderWrap decoder_;
    TimelineBuffer timeline_buffer_;
    ClockEstimator clock_estimator_;
    JitterBuffer jitter_buffer_;
    DriftController drift_controller_;
    Resampler resampler_;
    AudioPlaybackDevice playback_device_;

    uint32_t next_ping_id_{1};
    uint64_t audio_packets_received_{0};
    bool playback_device_started_{false};

    std::chrono::steady_clock::time_point last_ping_time_;
    std::chrono::steady_clock::time_point last_stats_report_time_;
};

}  // namespace chorus
