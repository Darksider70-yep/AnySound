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
