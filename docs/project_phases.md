# Project phases: Chorus (working title)

Read `architecture.md` and `rules.md` first. Work on **one phase at a time**. At the end of each phase, run every acceptance check, write a short summary of results, and **stop for review** before starting the next phase. Do not pull work forward from later phases.

Target for early phases: **Windows host + Windows/Linux clients** (WASAPI loopback is the smoothest capture path). Change only after Phase 1 findings.

## How every phase ends

- Code builds warning-free on CI; all tests pass.
- Docs updated (`architecture.md` if the design changed, `docs/findings.md` for measurements).
- A short summary: what was built, what was measured, open issues.

## Test rig (used from Phase 2 onward)

Measuring sync objectively beats listening.

1. `tools/click_track`: generates a WAV with a sharp click every 1 s. Play it on the host.
2. Place a phone or mic **equidistant** from two speakers (host and a client, or two clients) and record.
3. `tools/measure_offset.py`: finds click pairs in the recording (cross-correlation) and prints the time difference in ms. With equal distances, that difference is the sync error.
4. For network faults, use `tc netem` (Linux) or an in-process lossy socket wrapper to add jitter, loss, and delay.

---

## Phase 0: Scaffold

**Goal:** a buildable, tested, CI-checked skeleton.

- CMake project (C++20), vcpkg manifest, folder layout from `architecture.md` section 11.
- `.clang-format`, `.clang-tidy`, warnings-as-errors, sanitizer presets (ASAN/UBSAN, TSAN on Linux).
- `core` static library with one trivial function and one Catch2 test.
- GitHub Actions: build and test on Windows and Linux.
- `docs/decisions/` folder with a template.

**Accept:** clean configure and build on Windows; `ctest` passes; CI green.

## Phase 1: Audio pipe prototype (no sync)

**Goal:** prove the audio path end to end.

- `chorus_host`: capture loopback (miniaudio), accumulate 960-frame frames, Opus encode, send UDP to a given IP.
- `chorus_client`: receive, decode, write to a fixed 300 ms buffer, play.
- Experiment: does loopback level change with master volume and mute? Record in `docs/findings.md` (needed for `architecture.md` section 7a).
- Experiment: how accurate are miniaudio's output timing/timestamps on Windows? Record jitter numbers.

**Accept:** 10 minutes of continuous playback on a LAN with no audible dropouts; host CPU under 5%; findings documented.

## Phase 2: Clock sync and scheduled playback

**Goal:** frames play at the same instant on different machines.

- Implement `proto/` (header, AUDIO/PING/PONG) with round-trip and malformed-input tests.
- `ClockEstimator` with unit tests against a **simulated network** (jitter, asymmetric delay, outliers).
- `DeviceClock`, `TimelineBuffer`; client plays frames at `playAtHostUs`.

**Accept:** with the test rig, measured offset between two laptops is **at most 10 ms** immediately after joining. Over 10 minutes the offset may drift up to 20 ms (correction comes next phase).

## Phase 3: Robustness

**Goal:** stay in sync for a long time on imperfect Wi-Fi.

- `DriftController` and `Resampler` (skew estimate plus capped PI correction, hard resync above 30 ms).
- `JitterBuffer` with reorder handling, loss detection, Opus PLC.
- Fault-injection tests: 5% random loss, 50 ms jitter, a 2 s outage.
- Fuzz the packet parser.

**Accept:** 60-minute run with reported p95 sync error at most 5 ms and no audible glitches at 5% loss; recovers from a 2 s outage within 3 s without a loud artifact.

## Phase 4: Sessions, control, discovery

**Goal:** a real multi-client session.

- TCP control channel, `hello`/`welcome`/`reject`, PIN check with rate limiting.
- Host fan-out to N clients; per-client volume, mute, and offset; `stats` reporting.
- mDNS discovery and manual `ip:port` fallback; reconnect logic.
- `AppController` facade exposing a state snapshot and events (this is what UIs will use).

**Accept:** 4 simultaneous clients; a client joining or leaving does not disturb the others; wrong PIN rejected; host CPU under 10% with 4 clients; clients rejoin automatically after a brief Wi-Fi drop.

## Phase 5: Desktop UI

**Goal:** the whole flow without a terminal.

- Qt 6 QML app implementing `Ui.md`: Home, Host, Client, Calibrate, Settings.
- Live device list with sync state, per-device volume, mute, and delay.
- Windows packaging (`windeployqt`) and a portable build.

**Accept:** a new user can host, join, calibrate, and stop using only the UI; UI stays responsive while streaming; keyboard-only operation works; reduced-motion and light theme verified.

## Phase 6: Android client

**Goal:** phones as extra speakers.

- Build `core` with the NDK; JNI bridge; Kotlin + Jetpack Compose UI following `Ui.md`.
- Foreground service with wake lock and Wi-Fi lock; AAudio low-latency output with hardware timestamps for `DeviceClock`.
- Calibration screen with a per-device saved offset.

**Accept:** the phone joins by QR or code and stays in sync within 15 ms after calibration; playback continues for 30 minutes with the screen off and while switching apps; recovers after Wi-Fi reconnect.

## Phase 7: Hardening and extras

- Encrypted transport with a proper pairing handshake (see `architecture.md` section 9).
- Host capture for macOS and Linux.
- "Delayed host" mode via a virtual audio cable.
- Bitrate presets, latency auto-suggestion, stats overlay, crash and diagnostic logs, code signing and installer.

## Phase 8 (optional): iOS client

AVAudioEngine output, background audio mode, Bonjour discovery, same JNI-style bridge via Objective-C++.
