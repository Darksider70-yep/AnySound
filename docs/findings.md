# Empirical Findings and Measurements: Chorus

This document tracks empirical measurements, hardware behavior observations, and test rig data across project phases.

---

## Phase 0: Scaffolding Baseline
- **Build toolchain:** CMake 3.25+, C++20 standard, Ninja generator.
- **Compiler support verified:** GCC 15.2.0 (C++20), MSVC / Clang CI targets.
- **Catch2 test harness:** Verified clean unit test execution.

---

## Phase 1: Audio Pipe Measurements & Findings
- **Opus Codec Integration:**
  - 48 kHz stereo 20 ms frames (960 samples/channel = 1920 floats).
  - Average bitrate: ~65-68 kbps (well within the ~96 kbps budget).
  - Packet size: ~150-200 bytes per 20 ms frame (well below the 1200 byte UDP datagram cap).
- **Lock-Free SPSC Playout Buffer:**
  - Stress-tested across threads with 100,000 items with zero data corruption.
  - Zero allocations, locks, or I/O in the audio callback paths.
- **End-to-End Loopback Stream:**
  - Successfully verified end-to-end audio streaming from `chorus_host` to `chorus_client` on localhost UDP port 47801.
  - Pre-buffering target (300 ms) engaged smoothly with 0 underruns recorded.
  - Host CPU load: < 2% during continuous audio encoding and transmission.
- **WASAPI Loopback Capture Behavior (Windows):**
  - Loopback capture is supported directly via miniaudio `ma_device_type_loopback` capturing the default render endpoint.
  - Note: In Windows WASAPI loopback, muting master volume or silence on the render device halts frame generation unless audio is playing or a background dummy stream keeps the endpoint active. A fallback to test generation or feeding silence keeps timing synchronized.

---

## Phase 2: Clock Sync & Scheduled Playout Findings
- **NTP-Style UDP Clock Estimation:**
  - Round-trip ping/pong timestamps `(t0, t1, t2, t3)` tested against simulated networks (10 ms one-way delay, 0-15 ms jitter, 150 ms outlier spikes, 60 ppm clock drift).
  - Pure `ClockEstimator` achieves sync offset precision within < 50 µs on ideal links and < 8 ms under heavy synthetic jitter.
  - Sliding lowest-RTT window reliably rejects asymmetric route spikes and transient delay surges.
- **Scheduled Timeline Buffer Playout:**
  - `TimelineBuffer` maps microsecond `play_at_host_us` deadlines converted to local device time via `host_to_local_us()`.
  - Hard real-time lock-free read callback in `AudioPlaybackDevice` drains samples directly at hardware DAC rate (48 kHz stereo), preventing timeline buffer over-consumption and late-frame drops.
  - Late frame rejection and timeline gap detection tested and validated.
- **End-to-End Loopback Measurements:**
  - `chorus_host` frame pacing at steady 50.2 fps (103-105 kbps bandwidth).
  - Client clock sync lock acquired with 0.5 ms estimated offset and 1.3 ms RTT on loopback.
  - 0 late frames dropped during steady-state streaming; 130k+ audio samples rendered on schedule.
- **Calibration Tooling:**
  - `tools/click_track.py`: generates 48 kHz 16-bit stereo pulse click tracks with Hanning window envelopes.
  - `tools/measure_offset.py`: performs cross-correlation peak alignment on stereo reference recordings and reports average offset, min/max bounds, and peak-to-peak jitter. Measured offset on synthetic test track: 0.00 ms (PASS <= 10 ms).

---

## Phase 3: Robustness & Fault Tolerance Findings
- **Jitter Buffer & Packet Loss Concealment (PLC):**
  - `JitterBuffer` reorders sequence numbers out-of-order, suppresses duplicate frames, and detects sequence gaps.
  - Opus PLC synthesis verified under 5% packet loss fault injection with seamless audio continuity and zero hard dropouts.
- **Continuous Drift Control & Stereo Resampling:**
  - `Resampler` provides $C^1$ cubic spline fractional interpolation with continuous sub-sample phase tracking across frame boundaries (zero boundary clicks).
  - `DriftController` applies PI feedback on measured playout phase error, clamped to $\pm 200\text{ ppm}$ ($\pm 0.02\%$) to prevent audible pitch modulation.
  - Excursions $>30\text{ ms}$ trigger an automated hard resync, successfully recovering from 2-second connection blackouts within $<3$ seconds.
- **Protocol Fuzzing:**
  - 10,000 randomized and mutated bitstreams tested against `PacketHeader`, `AudioPacket`, `PingPacket`, and `PongPacket` deserializers with zero crashes, buffer overruns, or undefined behavior.

---

## Phase 4: Sessions, Control, and Discovery Findings
- **TCP Control Protocol & Framing:**
  - Length-prefixed 4-byte big-endian framing with UTF-8 JSON payloads reliably parses and serializes control messages (`hello`, `welcome`, `reject`, `set_volume`, `set_mute`, `set_offset_ms`, `set_target_latency_ms`, `stats`, `bye`).
  - Strict payload size bounds (64 KB cap) prevent memory exhaust attacks or malformed payload overflows.
- **PIN Authentication & Sliding-Window Rate Limiting:**
  - Enforces 4-digit numeric PIN with rolling 60-second sliding-window tracker.
  - Automatically rate-limits client IP addresses exceeding 5 failed attempts within 60 seconds with `"rate_limited"` rejection.
- **UDP Audio Fan-Out & Host Orchestration:**
  - `HostSession` unicasts encoded Opus audio frames concurrently to up to 8 authenticated clients with individual volume, mute, and timeline delay offsets.
  - Multi-client automated unit and loopback tests confirmed clean fan-out distribution and per-client control dispatch.
- **LAN Discovery Broadcasting & Scanning:**
  - `DiscoveryBroadcaster` issues periodic UDP beacons on port 47803 announcing session name, control port, and audio data port.
  - `DiscoveryScanner` tracks live local hosts with automatic 5-second stale host pruning.
- **Unified AppController Facade:**
  - Integrates audio capture, sessions, discovery, and volume/offset controls into an event-loop-friendly interface for desktop GUI and CLI apps.

---

## Phase 5: Desktop UI Findings & Design System
- **Qt 6 QML Design System & Component Library (`apps/desktop/`):**
  - Implemented `Theme.qml` singleton with exact token color palettes (`harbor`, `deck`, `line`, `fog`, `mist`, `sonar`, `drift`, `lost`), font scales (`Instrument Serif`, `Manrope`), and focus state rings.
  - Built custom component library: `SyncBadge.qml`, `RoomView.qml`, `VolumeSlider.qml`, `DelayStepper.qml`, `DeviceRow.qml`, `PrimaryButton.qml`, and `SecondaryButton.qml`.
  - Built full screen suite: `HomeView.qml`, `HostView.qml` (with 2-column Room view and device list), `ClientFindView.qml`, `ClientListeningView.qml` (hero sync readout), `CalibrateView.qml` (fine-tune delay), and `SettingsView.qml`.
- **Sonar Ring Visual Metaphor:**
  - Visual phase offset `phase = clamp(syncErrorMs / 20, -1, 1) * 0.5` visibly encodes synchronization state without relying on color alone.
  - Implemented 30 fps throttled redraw and reduced-motion fallback modes.
- **Interactive Companion UI (`apps/web_ui/index.html`):**
  - Standalone browser-renderable interface mirroring `Ui.md` with active sonar canvas animations, dynamic device rows, and live theme switching.
