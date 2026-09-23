# Chorus (AnySound) 🎶

**Chorus** is a high-performance, low-latency, multi-device audio synchronization system. It captures system audio from a host device (e.g. Windows laptop) and streams it synchronously across other devices in the room (e.g. Android phones, secondary PCs) to create an amplified, multi-speaker sound stage.

---

## 🚀 Key Features

- **Ultra-Low Latency & High Precision Sync**: NTP-style UDP clock synchronization with linear regression drift compensation and phase-locked loop (PLL) tracking sub-5ms audio alignment ($\pm 1\text{--}3\text{ ms}$).
- **Opus Codec Compression**: High-fidelity 48 kHz stereo audio stream with Packet Loss Concealment (PLC) and adaptive jitter buffering.
- **Auto LAN Discovery**: Subnet broadcast and multicast discovery (`239.255.77.77:47803`) with zero-configuration peer pairing.
- **Cross-Platform Engine**: Pure C++20 core engine (`core/`) shared across Windows CLI, Desktop, and Android (via JNI & Jetpack Compose UI).
- **Session Authentication & Controls**: Optional 4-digit PIN authentication, client volume control, individual delay/offset calibration ($\pm 500\text{ ms}$ for Bluetooth headphone latency), and diagnostics export.

---

## 📁 Repository Structure

```text
AnySound/
├── apps/
│   ├── android/             # Android Kotlin Jetpack Compose client app
│   ├── cli/                 # C++ Host (chorus_host) & Client (chorus_client) binaries
│   ├── desktop/             # Desktop Qt UI frontend
│   └── web_ui/              # Web-based visual dashboard (index.html)
├── core/                    # Core C++20 engine
│   ├── include/chorus/      # Public headers (audio, codec, net, sync, session, proto)
│   └── src/                 # Implementations (WASAPI loopback, Opus, sockets, drift control)
├── docs/                    # Technical specifications, architecture, and guides
│   ├── architecture.md      # Core architecture and wire protocol specification
│   ├── findings.md          # Hardware latency and empirical findings
│   ├── project_phases.md    # Phased milestone delivery roadmap
│   ├── rules.md             # Code standards and concurrency rules
│   └── Ui.md                # UI design system & token definitions
├── tests/                   # Catch2 unit tests (6,400+ assertions across 40 test cases)
└── CMakeLists.txt           # Modern CMake configuration
```

---

## 🛠️ Building & Running

### 1. Prerequisites

- **C++ Compiler**: GCC/Clang with C++20 support or MSVC (Windows).
- **Build System**: CMake 3.24+ and Ninja.
- **Android SDK / NDK**: Android Studio with NDK 26+ for the Android app.

### 2. Building the Native Binaries (Windows / Linux)

```powershell
# Configure CMake
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build CLI host, client, and unit tests
cmake --build build --config Debug --target chorus_host chorus_client chorus_unit_tests

# Run unit tests (40/40 test cases)
.\build\tests\chorus_unit_tests.exe
```

### 3. Running the Host (Audio Source)

To stream your PC / Laptop's system audio (YouTube, Spotify, games, etc.):

```powershell
# Stream live system audio (WASAPI loopback)
.\build\apps\cli\chorus_host.exe

# Or test with synthetic 440 Hz test sine wave
.\build\apps\cli\chorus_host.exe --test-tone
```

Options:
- `--pin <pin>`: Require a 4-digit PIN to connect.
- `--latency <ms>`: Target latency buffer (default: 300 ms).
- `--tcp-port <port>`: Control port (default: `47800`).
- `--udp-port <port>`: Audio datagram port (default: `47801`).

### 4. Running the Client (Audio Receiver)

#### On Another PC / Laptop (CLI):
```powershell
# Auto-scan local network for hosts
.\build\apps\cli\chorus_client.exe --scan

# Connect to host
.\build\apps\cli\chorus_client.exe 192.168.1.16 --port 47800
```

#### On Android Phone:
1. Build the APK:
   ```powershell
   cd apps/android
   .\gradlew.bat assembleDebug
   ```
2. Install `apps/android/app/build/outputs/apk/debug/app-debug.apk` on your Android device.
3. Open Chorus, tap **Listen / Find Hosts**, and tap **Join** when your host appears (or enter IP manually).

---

## 📡 Network Protocol Overview

| Channel | Protocol | Port | Description |
|---|---|---|---|
| **Control** | TCP | `47800` | Handshake (`Hello`/`Welcome`), PIN auth, volume/mute/offset sync, and stats reporting. |
| **Audio** | UDP | `47801` | Opus-encoded audio frames stamped with `play_at_host_us` microseconds. |
| **Clock Sync** | UDP | `47801` | NTP-style Ping/Pong ($t_0, t_1, t_2, t_3$) for sub-millisecond clock translation. |
| **Discovery** | UDP | `47803` | Subnet broadcast and multicast announcements (`239.255.77.77`). |

---

## 📖 Documentation Index

For detailed specifications and architecture guides, explore the [`docs/`](docs/) directory:
- [Architecture & Protocol Specification](docs/architecture.md)
- [Design Decisions & Log](docs/findings.md)
- [Project Phases & Roadmap](docs/project_phases.md)
- [Engineering Rules & Concurrency](docs/rules.md)
- [UI Theme & Interaction Spec](docs/Ui.md)

---

## 📄 License

Chorus is released under the MIT License. See [LICENSE](LICENSE) for details.
