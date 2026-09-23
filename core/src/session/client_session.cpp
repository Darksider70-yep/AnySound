#include <chorus/session/client_session.hpp>
#include <chorus/proto/packet.hpp>

#include <algorithm>
#include <array>

namespace chorus {

namespace {

constexpr size_t kDefaultClockPingSamples = 30;
constexpr size_t kDefaultClockTrimSamples = 5;
constexpr size_t kDefaultJitterCapacity = 30;
constexpr size_t kDefaultJitterPrebuffer = 3;
constexpr int kConnectTimeoutMs = 2000;
constexpr int32_t kMinOffsetMs = -500;
constexpr int32_t kMaxOffsetMs = 500;
constexpr double kFrameDurationSec = 0.02;

uint64_t current_steady_us() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

}  // namespace

ClientSession::ClientSession(std::string_view client_name)
    : client_name_(client_name),
      timeline_buffer_(static_cast<size_t>(kSampleRate * 2)),
      clock_estimator_(kDefaultClockPingSamples, kDefaultClockTrimSamples),
      jitter_buffer_(kDefaultJitterCapacity, kDefaultJitterPrebuffer),
      drift_controller_(kDefaultMaxCorrectionPpm, kDefaultHardResyncThresholdUs) {}

ClientSession::~ClientSession() {
    disconnect();
}

bool ClientSession::connect(std::string_view host_ip,
                            uint16_t tcp_port,
                            uint16_t udp_listen_port,
                            std::string_view pin) {
    disconnect();

    host_ip_ = std::string(host_ip);
    tcp_port_ = tcp_port;
    udp_listen_port_ = udp_listen_port;
    submitted_pin_ = std::string(pin);
    rejection_reason_.clear();

    if (!decoder_.init()) {
        return false;
    }

    if (!udp_socket_.bind(udp_listen_port_)) {
        return false;
    }
    (void)udp_socket_.set_recv_timeout_ms(1);

    state_.store(ClientSessionState::Connecting);

    const Endpoint host_ep{.address = host_ip_, .port = tcp_port_};
    if (!tcp_stream_.connect(host_ep, kConnectTimeoutMs)) {
        state_.store(ClientSessionState::Disconnected);
        udp_socket_.close();
        return false;
    }

    // Send Hello
    const HelloMessage hello{
        .name = client_name_,
        .platform = "Windows",
        .protocol = kProtocolVersion,
        .pin = submitted_pin_
    };

    if (!tcp_stream_.send_message(hello)) {
        state_.store(ClientSessionState::Disconnected);
        tcp_stream_.close();
        udp_socket_.close();
        return false;
    }

    state_.store(ClientSessionState::Authenticating);
    last_ping_time_ = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    last_stats_report_time_ = std::chrono::steady_clock::now();
    return true;
}

void ClientSession::disconnect() {
    const auto current = state_.load();
    if (current != ClientSessionState::Disconnected) {
        if (current != ClientSessionState::Rejected) {
            state_.store(ClientSessionState::Disconnected);
        }
        if (tcp_stream_.is_connected()) {
            (void)tcp_stream_.send_message(ByeMessage{});
            tcp_stream_.close();
        }
        udp_socket_.close();
        playback_device_.stop();
        playback_device_started_ = false;
        timeline_buffer_.reset();
        jitter_buffer_.reset();
        clock_estimator_.reset();
        drift_controller_.reset();
        resampler_.reset();
    }
}

void ClientSession::update() {
    if (state_.load() == ClientSessionState::Disconnected || state_.load() == ClientSessionState::Rejected) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();

    // 1. Process TCP control messages
    if (tcp_stream_.is_connected()) {
        const auto messages = tcp_stream_.read_messages();
        for (const auto& msg : messages) {
            std::visit([&](const auto& msg_data) {
                using MsgType = std::decay_t<decltype(msg_data)>;
                if constexpr (std::is_same_v<MsgType, WelcomeMessage>) {
                    host_udp_port_ = msg_data.udp_port;
                    state_.store(ClientSessionState::Synchronizing);
                } else if constexpr (std::is_same_v<MsgType, RejectMessage>) {
                    rejection_reason_ = msg_data.reason;
                    state_.store(ClientSessionState::Rejected);
                    disconnect();
                } else if constexpr (std::is_same_v<MsgType, SetVolumeMessage>) {
                    volume_.store(std::clamp(msg_data.value, 0.0F, 1.0F));
                } else if constexpr (std::is_same_v<MsgType, SetMuteMessage>) {
                    is_muted_.store(msg_data.value);
                } else if constexpr (std::is_same_v<MsgType, SetOffsetMsMessage>) {
                    offset_ms_.store(std::clamp(msg_data.value, kMinOffsetMs, kMaxOffsetMs));
                } else if constexpr (std::is_same_v<MsgType, ByeMessage>) {
                    disconnect();
                }
            }, msg);
        }
    } else if (state_.load() != ClientSessionState::Rejected) {
        // Lost TCP connection
        disconnect();
        return;
    }

    // 2. Clock sync PING scheduling
    if (state_.load() == ClientSessionState::Synchronizing || state_.load() == ClientSessionState::Active) {
        const bool is_synced = clock_estimator_.is_synchronized();
        const auto ping_interval = is_synced ? std::chrono::seconds(2) : std::chrono::milliseconds(25);
        if (now - last_ping_time_ >= ping_interval) {
            send_clock_ping();
            last_ping_time_ = now;
        }

        if (is_synced && state_.load() == ClientSessionState::Synchronizing) {
            state_.store(ClientSessionState::Active);
        }
    }

    // 3. Process UDP datagrams
    process_incoming_udp();

    // 4. Drain jitter buffer and feed timeline
    drain_jitter_and_play();

    // 5. Periodic 1-second stats report to host
    if (tcp_stream_.is_connected() && (now - last_stats_report_time_ >= std::chrono::seconds(1))) {
        (void)tcp_stream_.send_message(stats());
        last_stats_report_time_ = now;
    }
}

void ClientSession::send_clock_ping() {
    const uint64_t time_t0 = current_steady_us();
    const PingPacket ping{
        .header = {
            .magic = kPacketMagic,
            .version = kProtocolVersion,
            .type = PacketType::Ping,
            .session_id = 1
        },
        .ping_id = next_ping_id_++,
        .t0 = time_t0
    };

    std::array<uint8_t, kPingPacketSize> ping_buf{};
    const size_t bytes = serialize_ping_packet(ping, ping_buf);
    if (bytes > 0) {
        const Endpoint host_udp_ep{.address = host_ip_, .port = host_udp_port_};
        (void)udp_socket_.send_to(std::span<const uint8_t>(ping_buf.data(), bytes), host_udp_ep);
    }
}

void ClientSession::process_incoming_udp() {
    std::vector<uint8_t> packet_buf(kMaxUdpPayloadSize);
    Endpoint sender;

    while (true) {
        const int bytes = udp_socket_.receive_from(packet_buf, sender);
        if (bytes <= 0) {
            break;
        }

        const uint64_t time_t3 = current_steady_us();
        const std::span<const uint8_t> pkt_span(packet_buf.data(), static_cast<size_t>(bytes));
        const auto header_opt = parse_header(pkt_span);

        if (header_opt.has_value()) {
            if (header_opt->type == PacketType::Pong) {
                const auto pong_opt = parse_pong_packet(pkt_span);
                if (pong_opt.has_value()) {
                    clock_estimator_.record_sample(pong_opt->ping_id, pong_opt->t0, pong_opt->t1, pong_opt->t2, time_t3);
                }
            } else if (header_opt->type == PacketType::Audio) {
                const auto audio_opt = parse_audio_packet(pkt_span);
                if (audio_opt.has_value()) {
                    jitter_buffer_.push(*audio_opt);
                    audio_packets_received_++;
                }
            }
        }
    }
}

void ClientSession::drain_jitter_and_play() {
    std::vector<float> pcm_decoded(static_cast<size_t>(kFloatsPerFrame));
    std::vector<float> pcm_resampled(static_cast<size_t>(kFloatsPerFrame * 2));

    JitterFrame jframe;
    while (true) {
        const auto pop_res = jitter_buffer_.pop(jframe);
        if (pop_res == JitterPopResult::Empty) {
            break;
        }

        if (pop_res == JitterPopResult::LossPlc) {
            const int samples = decoder_.decode_plc(pcm_decoded);
            if (samples == kSamplesPerFramePerChannel) {
                resampler_.set_ratio(drift_controller_.resampler_ratio());
                const size_t out_frames = resampler_.process(pcm_decoded, pcm_resampled);
                if (out_frames > 0) {
                    const uint64_t current_us = current_steady_us();
                    (void)timeline_buffer_.insert_frame(current_us, std::span<const float>(pcm_resampled.data(), out_frames * 2));
                }
            }
        } else if (pop_res == JitterPopResult::Ready) {
            const int samples = decoder_.decode(jframe.payload, pcm_decoded);
            if (samples == kSamplesPerFramePerChannel) {
                // Apply local user volume and mute
                const float vol = is_muted_.load() ? 0.0F : volume_.load();
                if (vol != 1.0F) {
                    for (float& pcm_sample : pcm_decoded) {
                        pcm_sample *= vol;
                    }
                }

                const int64_t user_offset_us = static_cast<int64_t>(offset_ms_.load()) * 1000;
                const auto local_play_us = static_cast<uint64_t>(
                    static_cast<int64_t>(clock_estimator_.host_to_local_us(jframe.play_at_host_us)) + user_offset_us
                );
                const uint64_t current_local_us = current_steady_us();
                const int64_t phase_error_us = static_cast<int64_t>(local_play_us) - static_cast<int64_t>(current_local_us);

                drift_controller_.update(phase_error_us, clock_estimator_.skew_ppm(), kFrameDurationSec);
                if (drift_controller_.needs_hard_resync()) {
                    timeline_buffer_.reset();
                    drift_controller_.clear_resync();
                }

                resampler_.set_ratio(drift_controller_.resampler_ratio());
                const size_t out_frames = resampler_.process(pcm_decoded, pcm_resampled);

                if (out_frames == static_cast<size_t>(kSamplesPerFramePerChannel)) {
                    (void)timeline_buffer_.insert_frame(local_play_us, std::span<const float>(pcm_resampled.data(), out_frames * 2));
                } else {
                    (void)timeline_buffer_.insert_frame(local_play_us, pcm_decoded);
                }

                if (!playback_device_started_ && timeline_buffer_.is_active()) {
                    if (playback_device_.start_playback(&timeline_buffer_)) {
                        playback_device_started_ = true;
                    }
                }
            }
        }
    }
}

ClientSessionState ClientSession::state() const noexcept {
    return state_.load();
}

std::string ClientSession::rejection_reason() const {
    return rejection_reason_;
}

float ClientSession::volume() const noexcept {
    return volume_.load();
}

void ClientSession::set_volume(float vol) noexcept {
    volume_.store(std::clamp(vol, 0.0F, 1.0F));
}

bool ClientSession::is_muted() const noexcept {
    return is_muted_.load();
}

void ClientSession::set_mute(bool mute) noexcept {
    is_muted_.store(mute);
}

void ClientSession::set_offset_ms(int32_t offset_ms) {
    offset_ms_.store(std::clamp(offset_ms, kMinOffsetMs, kMaxOffsetMs));
    if (tcp_stream_.is_connected()) {
        (void)tcp_stream_.send_message(SetOffsetMsMessage{.value = offset_ms_.load()});
    }
}

int32_t ClientSession::offset_ms() const noexcept {
    return offset_ms_.load();
}

ClientStatsMessage ClientSession::stats() const {
    const int64_t sync_err = clock_estimator_.is_synchronized()
        ? (clock_estimator_.min_rtt_us() / 2)
        : 0;
    return ClientStatsMessage{
        .sync_error_us = sync_err,
        .skew_ppm = clock_estimator_.skew_ppm(),
        .underruns = playback_device_.underruns(),
        .late = timeline_buffer_.late_frames_dropped(),
        .loss_pct = (jitter_buffer_.total_received() > 0)
            ? (static_cast<double>(jitter_buffer_.lost_plc_count()) * 100.0 / static_cast<double>(jitter_buffer_.total_received() + jitter_buffer_.lost_plc_count()))
            : 0.0,
        .buffer_ms = static_cast<uint32_t>(jitter_buffer_.size() * static_cast<size_t>(kFrameDurationMs))
    };
}

bool ClientSession::is_active() const noexcept {
    return state_.load() == ClientSessionState::Active;
}

}  // namespace chorus
