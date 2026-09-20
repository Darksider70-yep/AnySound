#include <chorus/app/app_controller.hpp>
#include <chorus/core.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint64_t kDefaultTargetLatencyMs = 300;

class HostSignalTracker {
public:
    static void initialize() {
        std::signal(SIGINT, &HostSignalTracker::handle_signal);
        std::signal(SIGTERM, &HostSignalTracker::handle_signal);
    }
    [[nodiscard]] static bool stop_requested() noexcept {
        return stop_flag_.load(std::memory_order_relaxed);
    }

private:
    static void handle_signal(int) {
        stop_flag_.store(true, std::memory_order_relaxed);
    }
    static inline std::atomic<bool> stop_flag_{false};
};

struct HostConfig {
    std::string target_ip{"127.0.0.1"};
    uint16_t tcp_port{chorus::kDefaultTcpControlPort};
    uint16_t udp_port{chorus::kDefaultUdpDataPort};
    std::string pin{};
    bool test_tone{false};
    int duration_sec{0};
    uint64_t target_latency_ms{kDefaultTargetLatencyMs};
    bool show_help{false};
};

HostConfig parse_host_args(std::span<char*> args) {
    HostConfig config;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string arg = args[i];
        if (arg == "--test-tone") {
            config.test_tone = true;
        } else if (arg == "--duration" && (i + 1 < args.size())) {
            config.duration_sec = std::stoi(args[++i]);
        } else if (arg == "--latency" && (i + 1 < args.size())) {
            config.target_latency_ms = static_cast<uint64_t>(std::stoul(args[++i]));
        } else if (arg == "--pin" && (i + 1 < args.size())) {
            config.pin = args[++i];
        } else if (arg == "--tcp-port" && (i + 1 < args.size())) {
            config.tcp_port = static_cast<uint16_t>(std::stoi(args[++i]));
        } else if (arg == "--udp-port" && (i + 1 < args.size())) {
            config.udp_port = static_cast<uint16_t>(std::stoi(args[++i]));
        } else if (arg == "--help" || arg == "-h") {
            config.show_help = true;
            return config;
        } else if (i == 1 && arg[0] != '-') {
            config.target_ip = arg;
        }
    }
    return config;
}

}  // namespace

int main(int argc, char* argv[]) {
    HostSignalTracker::initialize();

    const HostConfig config = parse_host_args(std::span<char*>(argv, static_cast<size_t>(argc)));
    if (config.show_help) {
        std::cout << "Usage: chorus_host [--pin <pin>] [--tcp-port <port>] [--udp-port <port>] [--test-tone] [--duration <sec>] [--latency <ms>]\n";
        return 0;
    }

    std::cout << "========================================\n";
    std::cout << "Chorus Host Audio Streamer (Phase 4 Session & Fan-Out)\n";
    std::cout << "Target Latency: " << config.target_latency_ms << " ms\n";
    std::cout << "Mode: " << (config.test_tone ? "440Hz Test Sine Generator" : "WASAPI System Loopback Capture") << "\n";
    if (config.duration_sec > 0) {
        std::cout << "Duration: " << config.duration_sec << " seconds\n";
    }
    std::cout << "========================================\n" << std::flush;

    chorus::AppController app;
    if (!app.start_host(config.pin, config.target_latency_ms, config.test_tone)) {
        std::cerr << "Failed to start Host Session!\n" << std::flush;
        return 1;
    }

    const auto snap = app.snapshot();
    std::cout << ">> Host Running! PIN: [" << snap.session_pin << "]\n";
    std::cout << ">> TCP Control Port: " << config.tcp_port << " | UDP Audio Port: " << config.udp_port << "\n";
    std::cout << ">> Broadcasting discovery over LAN. Waiting for clients...\n" << std::flush;

    const auto start_time = std::chrono::steady_clock::now();
    auto last_stats_time = start_time;

    while (!HostSignalTracker::stop_requested()) {
        const auto now = std::chrono::steady_clock::now();
        if (config.duration_sec > 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            if (elapsed >= config.duration_sec) {
                break;
            }
        }

        app.update();

        if (now - last_stats_time >= std::chrono::seconds(2)) {
            const auto current_snap = app.snapshot();
            std::cout << "[Host] PIN: " << current_snap.session_pin
                      << " | Connected Clients: " << current_snap.connected_clients.size() << "\n";
            for (const auto& client : current_snap.connected_clients) {
                std::cout << "  - Client #" << client.id << " [" << client.name << "] @ "
                          << client.address << " | Auth: " << (client.authenticated ? "YES" : "NO")
                          << " | Vol: " << static_cast<int>(client.volume * 100) << "%"
                          << (client.is_muted ? " (MUTED)" : "")
                          << " | Offset: " << client.offset_ms << " ms"
                          << " | Sync Err: " << (static_cast<double>(client.last_stats.sync_error_us) / 1000.0) << " ms\n";
            }
            std::cout << std::flush;
            last_stats_time = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "\nStopping Chorus Host...\n" << std::flush;
    app.stop_host();
    return 0;
}
