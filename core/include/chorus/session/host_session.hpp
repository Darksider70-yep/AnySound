#pragma once

#include <chorus/codec/opus_codec.hpp>
#include <chorus/core.hpp>
#include <chorus/net/control_message.hpp>
#include <chorus/net/tcp_socket.hpp>
#include <chorus/net/udp_socket.hpp>
#include <chorus/session/pin_authenticator.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace chorus {

struct ClientInfo {
    uint32_t id{0};
    std::string name{"Chorus Client"};
    std::string platform{"Unknown"};
    std::string address{"127.0.0.1"};
    uint16_t control_port{0};
    uint16_t udp_data_port{0};
    bool authenticated{false};
    float volume{1.0F};
    bool is_muted{false};
    int32_t offset_ms{0};
    ClientStatsMessage last_stats{};
};

/// @brief Host session orchestrator managing TCP control clients, PIN verification, and UDP audio fan-out.
class HostSession {
public:
    explicit HostSession(uint64_t target_latency_ms = kDefaultTargetLatencyMs);
    ~HostSession();

    HostSession(const HostSession&) = delete;
    HostSession& operator=(const HostSession&) = delete;
    HostSession(HostSession&&) = delete;
    HostSession& operator=(HostSession&&) = delete;

    /// @brief Starts host session on specified control & data ports with 4-digit PIN.
    [[nodiscard]] bool start(uint16_t tcp_port = kDefaultTcpControlPort,
                             uint16_t udp_port = kDefaultUdpDataPort,
                             std::string_view pin = "");

    /// @brief Stops host session and disconnects all clients.
    void stop();

    /// @brief Non-blocking tick to accept incoming clients, process control messages, and serve clock pings.
    void update();

    /// @brief Encodes and unicasts an audio frame to all connected, authenticated clients.
    /// @param frame_pcm 1920 interleaved stereo float samples (20 ms @ 48kHz).
    /// @return Number of clients audio was successfully transmitted to.
    size_t broadcast_audio_frame(std::span<const float> frame_pcm);

    /// @brief Returns the 4-digit PIN for this session.
    [[nodiscard]] std::string pin() const;

    /// @brief Sets volume (0.0 - 1.0) for a specific client.
    bool set_client_volume(uint32_t client_id, float volume);

    /// @brief Sets mute for a specific client.
    bool set_client_mute(uint32_t client_id, bool mute);

    /// @brief Sets latency offset (+/- 500 ms) for a specific client.
    bool set_client_offset_ms(uint32_t client_id, int32_t offset_ms);

    /// @brief Returns list of currently connected clients.
    [[nodiscard]] std::vector<ClientInfo> client_list() const;

    /// @brief Returns number of authenticated clients.
    [[nodiscard]] size_t authenticated_client_count() const;

    /// @brief Returns true if host session is running.
    [[nodiscard]] bool is_running() const noexcept;

private:
    struct ConnectedClient {
        ClientInfo info{};
        std::unique_ptr<TcpStream> stream;
    };

    void handle_client_message(ConnectedClient& client, const ControlMessage& message);
    void handle_incoming_clock_pings();

    const uint64_t target_latency_ms_;
    uint16_t tcp_port_{kDefaultTcpControlPort};
    uint16_t udp_port_{kDefaultUdpDataPort};

    TcpListener tcp_listener_;
    UdpSocket udp_socket_;
    PinAuthenticator pin_auth_;
    OpusEncoderWrap encoder_;

    std::vector<ConnectedClient> clients_;
    mutable std::mutex clients_mutex_;

    std::atomic<bool> is_running_{false};
    uint32_t next_client_id_{1};
    uint32_t seq_{0};
    uint64_t total_samples_streamed_{0};
    uint64_t stream_anchor_host_us_{0};
};

}  // namespace chorus
