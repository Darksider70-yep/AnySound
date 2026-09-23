# Chorus Documentation

This directory contains technical specifications, architecture designs, empirical test findings, and engineering guidelines for the **Chorus** multi-device synchronized audio engine.

---

## 📚 Document Directory

| Document | Description |
|---|---|
| [**Architecture & Protocol**](architecture.md) | Source of truth for system modules, threading model, clock synchronization, and the binary wire protocol. |
| [**Project Phases & Milestones**](project_phases.md) | Phased roadmap covering loopback capture, Opus streaming, clock sync, session orchestration, Android JNI, and UI. |
| [**Engineering Rules & Standards**](rules.md) | Code quality standards, real-time audio thread constraints (lock-free, zero-allocation), C++20 conventions, and safety guidelines. |
| [**Empirical Findings & Testing**](findings.md) | Hardware measurement notes on WASAPI loopback behavior, Opus packet sizes, Wi-Fi jitter benchmarks, and time sync calibration. |
| [**UI Design System & Tokens**](Ui.md) | Visual design language, dark theme palette (Harbor, Deck, Sonar, Fog), typography, micro-animations, and component hierarchy. |
| [**Decisions**](decisions/) | Architecture Decision Records (ADRs) detailing protocol choices and platform integrations. |

---

## 🏗️ Architectural Core Principles

1. **Deterministic Playout Scheduling**:
   - Host timestamps each audio frame with a future playout deadline: $\text{playAtHostUs} = \text{captureTime} + \text{targetLatencyMs} \times 1000$.
   - Clients translate host deadlines to local hardware clocks using continuous NTP-style UDP clock estimation with linear regression skew correction.

2. **Real-Time Thread Isolation**:
   - Audio callbacks never perform memory allocations, blocking I/O, or acquire mutexes.
   - Producer-consumer boundaries use lock-free SPSC circular ring buffers (`SpscRing`).

3. **Resilient Network Adaptation**:
   - In-band Forward Error Correction (FEC) and Opus Packet Loss Concealment (PLC) ensure smooth playback across lossy wireless links.
   - Phase error and clock drift are corrected via fractional sample resampling capped at $\pm 200\text{ ppm}$ to eliminate pitch distortion.
