#include <chorus/session/host_session.hpp>
#include <chorus/proto/packet.hpp>

#include <algorithm>
#include <array>
#include <chrono>

namespace chorus {

namespace {

constexpr size_t kMaxClients = 8;
constexpr uint16_t kDefaultClientDataPort = 47802;
constexpr size_t kPingBufferSize = 128;
constexpr int32_t kMinOffsetMs = -500;
constexpr int32_t kMaxOffsetMs = 500;

uint64_t current_steady_us() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

}  // namespace

HostSession::HostSession(uint64_t target_latency_ms)
    : target_latency_ms_(target_latency_ms) {}

HostSession::~HostSession() {
    stop();
}

bool HostSession::start(uint16_t tcp_port, uint16_t udp_port, std::string_view pin) {
    stop();

    tcp_port_ = tcp_port;
    udp_port_ = udp_port;

    if (pin == "random" || pin == "auto") {
        (void)pin_auth_.generate_random_pin();
    } else {
        pin_auth_.set_pin(pin);
    }

    if (!tcp_listener_.listen(tcp_port_)) {
        return false;
    }

    if (!udp_socket_.bind(udp_port_)) {
        tcp_listener_.close();
        return false;
    }
    (void)udp_socket_.set_recv_timeout_ms(1);

    if (!encoder_.init()) {
        tcp_listener_.close();
        udp_socket_.close();
        return false;
    }

    stream_anchor_host_us_ = current_steady_us();
    total_samples_streamed_ = 0;
    seq_ = 0;
    is_running_.store(true);
    return true;
}

void HostSession::stop() {
    if (is_running_.load()) {
        is_running_.store(false);

        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& client : clients_) {
            if (client.stream && client.stream->is_connected()) {
                (void)client.stream->send_message(ByeMessage{});
                client.stream->close();
            }
        }
        clients_.clear();

        tcp_listener_.close();
        udp_socket_.close();
    }
}

void HostSession::update() {
    if (!is_running_.load()) {
        return;
    }

    // 1. Accept new TCP clients
    while (true) {
        auto new_client_stream = tcp_listener_.accept_client();
        if (!new_client_stream) {
            break;
        }

        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (clients_.size() >= kMaxClients) {
            // Server full
            (void)new_client_stream->send_message(RejectMessage{.reason = "full"});
            new_client_stream->close();
            continue;
        }

        const Endpoint peer = new_client_stream->peer();
        ConnectedClient client{
            .info = {
                .id = next_client_id_++,
                .name = "Unknown",
                .platform = "Unknown",
                .address = peer.address,
                .control_port = peer.port,
                .udp_data_port = kDefaultClientDataPort,
                .authenticated = false,
                .volume = 1.0F,
                .is_muted = false,
                .offset_ms = 0,
                .last_stats = {}
            },
            .stream = std::move(new_client_stream)
        };
        clients_.push_back(std::move(client));
    }

    // 2. Read messages from connected clients
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& client : clients_) {
            if (client.stream && client.stream->is_connected()) {
                const auto messages = client.stream->read_messages();
                for (const auto& msg : messages) {
                    handle_client_message(client, msg);
                }
            }
        }

        // Clean up disconnected clients
        std::erase_if(clients_, [](const ConnectedClient& client_item) {
            return !client_item.stream || !client_item.stream->is_connected();
        });
    }

    // 3. Handle clock sync PINGs
    handle_incoming_clock_pings();
}

void HostSession::handle_client_message(ConnectedClient& client, const ControlMessage& message) {
    std::visit([&](const auto& msg) {
        using T = std::decay_t<decltype(msg)>;
        if constexpr (std::is_same_v<T, HelloMessage>) {
            client.info.name = msg.name;
            client.info.platform = msg.platform;

            if (msg.protocol != kProtocolVersion) {
                (void)client.stream->send_message(RejectMessage{.reason = "version"});
                client.stream->close();
                return;
            }

            const auto auth_res = pin_auth_.verify(client.info.address, msg.pin);
            if (auth_res == PinAuthResult::Success) {
                client.info.authenticated = true;
                const WelcomeMessage welcome{
                    .session_id = 1,
                    .udp_port = udp_port_,
                    .sample_rate = static_cast<uint32_t>(kSampleRate),
                    .channels = static_cast<uint32_t>(kChannels),
                    .frame_ms = static_cast<uint32_t>(kFrameDurationMs),
                    .target_latency_ms = target_latency_ms_,
                    .host_us = current_steady_us()
                };
                (void)client.stream->send_message(welcome);
            } else if (auth_res == PinAuthResult::RateLimited) {
                (void)client.stream->send_message(RejectMessage{.reason = "rate_limited"});
                client.stream->close();
            } else {
                (void)client.stream->send_message(RejectMessage{.reason = "bad_pin"});
                client.stream->close();
            }
        } else if constexpr (std::is_same_v<T, ClientStatsMessage>) {
            client.info.last_stats = msg;
        } else if constexpr (std::is_same_v<T, SetOffsetMsMessage>) {
            client.info.offset_ms = msg.value;
        } else if constexpr (std::is_same_v<T, ByeMessage>) {
            client.stream->close();
        }
    }, message);
}

void HostSession::handle_incoming_clock_pings() {
    std::array<uint8_t, kPingBufferSize> recv_buf{};
    Endpoint sender;

    while (true) {
        const int bytes = udp_socket_.receive_from(recv_buf, sender);
        if (bytes <= 0) {
            break;
        }

        const uint64_t timestamp_t1 = current_steady_us();
        const auto ping_opt = parse_ping_packet(std::span<const uint8_t>(recv_buf.data(), static_cast<size_t>(bytes)));
        if (ping_opt.has_value()) {
            // Update sender's UDP data port if matched
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                for (auto& client_entry : clients_) {
                    if (client_entry.info.address == sender.address) {
                        client_entry.info.udp_data_port = sender.port;
                    }
                }
            }

            const uint64_t timestamp_t2 = current_steady_us();
            const PongPacket pong{
                .header = {
                    .magic = kPacketMagic,
                    .version = kProtocolVersion,
                    .type = PacketType::Pong,
                    .session_id = ping_opt->header.session_id
                },
                .ping_id = ping_opt->ping_id,
                .t0 = ping_opt->t0,
                .t1 = timestamp_t1,
                .t2 = timestamp_t2
            };

            std::array<uint8_t, kPongPacketSize> pong_buf{};
            const size_t pong_bytes = serialize_pong_packet(pong, pong_buf);
            if (pong_bytes > 0) {
                (void)udp_socket_.send_to(std::span<const uint8_t>(pong_buf.data(), pong_bytes), sender);
            }
        }
    }
}

size_t HostSession::broadcast_audio_frame(std::span<const float> frame_pcm) {
    if (!is_running_.load() || frame_pcm.empty()) {
        return 0;
    }

    std::vector<uint8_t> opus_payload(static_cast<size_t>(kMaxOpusPayloadBytes));
    const int encoded_bytes = encoder_.encode(frame_pcm, opus_payload);
    if (encoded_bytes <= 0) {
        return 0;
    }

    const uint64_t frame_capture_us = current_steady_us();
    const uint64_t play_at_host_us = frame_capture_us + (target_latency_ms_ * 1000ULL);

    const AudioPacket pkt{
        .header = {
            .magic = kPacketMagic,
            .version = kProtocolVersion,
            .type = PacketType::Audio,
            .session_id = 1
        },
        .seq = seq_++,
        .play_at_host_us = play_at_host_us,
        .flags = 0,
        .payload = std::span<const uint8_t>(opus_payload.data(), static_cast<size_t>(encoded_bytes))
    };

    std::vector<uint8_t> pkt_buf(kMaxUdpPayloadSize);
    const size_t pkt_len = serialize_audio_packet(pkt, pkt_buf);
    if (pkt_len == 0) {
        return 0;
    }

    size_t sent_count = 0;
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& client : clients_) {
        if (client.info.authenticated) {
            const Endpoint dest{
                .address = client.info.address,
                .port = client.info.udp_data_port
            };
            if (udp_socket_.send_to(std::span<const uint8_t>(pkt_buf.data(), pkt_len), dest)) {
                sent_count++;
            }
        }
    }

    total_samples_streamed_ += static_cast<uint64_t>(kSamplesPerFramePerChannel);
    return sent_count;
}

std::string HostSession::pin() const {
    return pin_auth_.pin();
}

bool HostSession::set_client_volume(uint32_t client_id, float volume) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& client_entry : clients_) {
        if (client_entry.info.id == client_id) {
            client_entry.info.volume = std::clamp(volume, 0.0F, 1.0F);
            if (client_entry.stream && client_entry.stream->is_connected()) {
                (void)client_entry.stream->send_message(SetVolumeMessage{.value = client_entry.info.volume});
            }
            return true;
        }
    }
    return false;
}

bool HostSession::set_client_mute(uint32_t client_id, bool mute) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& client_entry : clients_) {
        if (client_entry.info.id == client_id) {
            client_entry.info.is_muted = mute;
            if (client_entry.stream && client_entry.stream->is_connected()) {
                (void)client_entry.stream->send_message(SetMuteMessage{.value = mute});
            }
            return true;
        }
    }
    return false;
}

bool HostSession::set_client_offset_ms(uint32_t client_id, int32_t offset_ms) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& client_entry : clients_) {
        if (client_entry.info.id == client_id) {
            client_entry.info.offset_ms = std::clamp(offset_ms, kMinOffsetMs, kMaxOffsetMs);
            if (client_entry.stream && client_entry.stream->is_connected()) {
                (void)client_entry.stream->send_message(SetOffsetMsMessage{.value = client_entry.info.offset_ms});
            }
            return true;
        }
    }
    return false;
}

std::vector<ClientInfo> HostSession::client_list() const {
    std::vector<ClientInfo> list;
    std::lock_guard<std::mutex> lock(clients_mutex_);
    list.reserve(clients_.size());
    for (const auto& client_entry : clients_) {
        list.push_back(client_entry.info);
    }
    return list;
}

size_t HostSession::authenticated_client_count() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return static_cast<size_t>(std::count_if(clients_.begin(), clients_.end(), [](const ConnectedClient& client_item) {
        return client_item.info.authenticated;
    }));
}

bool HostSession::is_running() const noexcept {
    return is_running_.load();
}

}  // namespace chorus
