#include <chorus/app/app_controller.hpp>
#include <chorus/core.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

class ClientSignalTracker {
public:
    static void initialize() {
        std::signal(SIGINT, &ClientSignalTracker::handle_signal);
        std::signal(SIGTERM, &ClientSignalTracker::handle_signal);
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

struct ClientConfig {
    std::string host_ip{"127.0.0.1"};
    uint16_t tcp_port{chorus::kDefaultTcpControlPort};
    std::string pin{"0000"};
    bool scan_only{false};
    int duration_sec{0};
    bool show_help{false};
};

ClientConfig parse_client_args(std::span<char*> args) {
    ClientConfig config;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string arg = args[i];
        if (arg == "--duration" && (i + 1 < args.size())) {
            config.duration_sec = std::stoi(args[++i]);
        } else if (arg == "--pin" && (i + 1 < args.size())) {
            config.pin = args[++i];
        } else if (arg == "--tcp-port" && (i + 1 < args.size())) {
            config.tcp_port = static_cast<uint16_t>(std::stoi(args[++i]));
        } else if (arg == "--scan") {
            config.scan_only = true;
        } else if (arg == "--help" || arg == "-h") {
            config.show_help = true;
            return config;
        } else if (arg[0] != '-') {
            config.host_ip = arg;
        }
    }
    return config;
}

}  // namespace

int main(int argc, char* argv[]) {
    ClientSignalTracker::initialize();

    const ClientConfig config = parse_client_args(std::span<char*>(argv, static_cast<size_t>(argc)));
    if (config.show_help) {
        std::cout << "Usage: chorus_client [host-ip] [--pin <pin>] [--tcp-port <port>] [--scan] [--duration <sec>]\n";
        return 0;
    }

    std::cout << "========================================\n";
    std::cout << "Chorus Client Audio Receiver (Phase 4 Session & Auth)\n";
    if (config.scan_only) {
        std::cout << "Mode: LAN Discovery Scanner\n";
    } else {
        std::cout << "Target Host: " << config.host_ip << ":" << config.tcp_port << "\n";
        std::cout << "PIN: [" << config.pin << "]\n";
    }
    if (config.duration_sec > 0) {
        std::cout << "Duration: " << config.duration_sec << " seconds\n";
    }
    std::cout << "========================================\n" << std::flush;

    chorus::AppController app;

    if (config.scan_only) {
        std::cout << "Scanning for Chorus hosts on LAN for 5 seconds...\n" << std::flush;
        for (int i = 0; i < 50 && !ClientSignalTracker::stop_requested(); ++i) {
            app.update();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        const auto snap = app.snapshot();
        std::cout << "Found " << snap.discovered_hosts.size() << " host(s):\n";
        for (const auto& h : snap.discovered_hosts) {
            std::cout << " - Host: " << h.name << " @ " << h.address << ":" << h.tcp_port << "\n";
        }
        return 0;
    }

    std::cout << "Connecting to host " << config.host_ip << ":" << config.tcp_port << "...\n" << std::flush;
    if (!app.join_host(config.host_ip, config.tcp_port, config.pin)) {
        std::cerr << "Failed to initiate connection to host!\n" << std::flush;
        return 1;
    }

    const auto start_time = std::chrono::steady_clock::now();
    auto last_stats_time = start_time;

    while (!ClientSignalTracker::stop_requested()) {
        const auto now = std::chrono::steady_clock::now();
        if (config.duration_sec > 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            if (elapsed >= config.duration_sec) {
                break;
            }
        }

        app.update();

        const auto snap = app.snapshot();
        if (snap.role == chorus::AppRole::Idle && !snap.rejection_reason.empty()) {
            std::cerr << "Rejected by host! Reason: " << snap.rejection_reason << "\n" << std::flush;
            break;
        }

        if (now - last_stats_time >= std::chrono::seconds(2)) {
            const auto& st = snap.client_stats;
            std::cout << "[Client] Active: " << (snap.is_active ? "YES" : "NO")
                      << " | Sync Err: " << (static_cast<double>(st.sync_error_us) / 1000.0) << " ms"
                      << " | Skew: " << std::fixed << std::setprecision(1) << st.skew_ppm << " ppm"
                      << " | Buffer: " << st.buffer_ms << " ms"
                      << " | Underruns: " << st.underruns
                      << " | Late: " << st.late
                      << " | Loss: " << std::setprecision(2) << st.loss_pct << "%"
                      << " | Vol: " << static_cast<int>(snap.volume * 100) << "%"
                      << (snap.is_muted ? " (MUTED)" : "")
                      << " | Offset: " << snap.offset_ms << " ms\n" << std::flush;
            last_stats_time = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "\nStopping Chorus Client...\n" << std::flush;
    app.leave_host();
    return 0;
}
