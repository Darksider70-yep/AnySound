# Empirical Findings and Measurements: Chorus

This document tracks empirical measurements, hardware behavior observations, and test rig data across project phases.

---

## Phase 0: Scaffolding Baseline
- **Build toolchain:** CMake 3.25+, C++20 standard, Ninja generator.
- **Compiler support verified:** GCC 15.2.0 (C++20), MSVC / Clang CI targets.
- **Catch2 test harness:** Verified clean unit test execution.

---

## Phase 1: Audio Pipe Measurements (Pending)
- [ ] WASAPI Loopback master volume and mute test results.
- [ ] miniaudio Windows timestamp accuracy and jitter.
- [ ] Base latency measurement over LAN.
