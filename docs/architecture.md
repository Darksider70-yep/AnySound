# Architecture: Chorus (working title)

Source of truth for system design and the wire protocol. If code and this file disagree, stop and resolve it (see `rules.md`, section 1) rather than silently diverging.

## 1. Goal and scope

Several devices (laptops, later phones) play the **same audio at the same moment**, so a quiet laptop's sound is reinforced by the others in the room. One device is the **Host** (the source), the rest are **Clients** (extra speakers).

**Non-goals for v1:** internet streaming (LAN only), spatial or stereo-split audio, per-app capture, video sync, DRM content, more than about 8 clients.

## 2. Roles

- **Host**: captures system audio, encodes it, timestamps it, and sends it to every client.
- **Client**: receives, decodes, and plays each frame at its scheduled time.
- A device is one role at a time in v1. The core supports both roles in one binary.

## 3. System overview

```
HOST                                                      CLIENT (xN)
system audio
   |  WASAPI loopback (miniaudio)                         UDP recv -> JitterBuffer
   v                                                          |  Opus decode (+PLC)
FrameAccumulator (960 frames/ch @ 48 kHz)                     v
   |  stamp playAtHostUs                                  TimelineBuffer (indexed by device frame)
   v                                                          |  Resampler (drift ppm) + volume
Opus encode -> UDP send (unicast to each client) ----->      v
                                                          Output device callback (real-time)
TCP control  <------------------------------------------>  TCP control (join, volume, stats)
UDP clock ping/pong  <---------------------------------->  ClockEstimator
```

## 4. Modules (`core/`)

| Module | Responsibility | Key types |
|---|---|---|
| `platform/` | OS-specific capture and output behind interfaces | `IAudioSource`, `IOutputDevice`, `WasapiLoopbackSource` |
| `codec/` | Opus wrappers | `OpusEncoderWrap`, `OpusDecoderWrap` |
| `net/` | Sockets, framing, discovery | `UdpChannel`, `ControlServer`, `ControlClient`, `Discovery` |
| `proto/` | Packet and message (de)serialization | `PacketHeader`, `ControlMessage` |
| `sync/` | Timing logic (pure, unit-testable) | `ClockEstimator`, `JitterBuffer`, `DriftController`, `Resampler` |
| `playback/` | Real-time playout | `SpscRing`, `TimelineBuffer`, `DeviceClock` |
| `session/` | Orchestration | `HostSession`, `ClientSession` |
| `app/` | Facade for UIs | `AppController` (state snapshot + events) |

`sync/` and `proto/` must have **no OS or socket dependencies** so they can be tested with simulated inputs.

## 5. Audio format

- 48,000 Hz, 2 channels, float32 internally; convert at device boundaries.
- Opus, 20 ms frames (960 samples per channel), `OPUS_APPLICATION_AUDIO`, VBR, default 96 kbps, in-band FEC on, expected loss 5%.
- Bandwidth per client is roughly 100 kbps, so unicast fan-out is fine for v1.

## 6. Threads

**Host**
1. Capture callback (real-time): copies samples into an `SpscRing`. Nothing else.
2. Network thread (asio `io_context`): pulls frames, encodes, stamps, sends. Also serves clock pings and control.

**Client**
1. Network thread (asio): receives UDP, feeds `JitterBuffer`, decodes, writes into `TimelineBuffer`; runs `ClockEstimator` and `DriftController`.
2. Audio callback (real-time): reads `TimelineBuffer`, applies resampler ratio and volume. Follows the rules in `rules.md`, section 3.
3. Control thread or asio strand: TCP session, stats reporting.

The UI thread only talks to `AppController`; it never touches the audio threads.

## 7. Timing model

- **Host clock**: `steady_clock` microseconds since session start (`hostUs`).
- **Frame stamping**: `playAtHostUs = frameCaptureHostUs + targetLatencyMs * 1000`. Derive `frameCaptureHostUs` from a **sample counter anchored at stream start**, not from per-callback wall-clock reads (callback timing is jittery). Re-anchor slowly if the capture clock drifts.
- **Default `targetLatencyMs`**: 300 (range 100-1500), chosen by the host and sent to clients in `welcome`.
- **Clock sync** (NTP-style, over UDP): the client sends `PING{t0}`, the host answers `PONG{t0, t1, t2}` (t1 = host receive, t2 = host send), the client records t3.
  - `offset = ((t1 - t0) + (t2 - t3)) / 2`, `rtt = (t3 - t0) - (t2 - t1)`
  - Send 10-20 pings at join (about 50 ms apart), then one every 2 s. Keep the lowest-RTT samples in a sliding window and derive `offset` from them.
  - Also estimate **skew** (clock rate difference, in ppm) by linear regression of offset over the last ~30 s.
- **Device clock**: `DeviceClock` maps output device frame index to local `steady_clock`. Prefer hardware timestamps (AAudio `getTimestamp`, WASAPI `IAudioClock`) over callback times. If miniaudio cannot provide timestamps accurate to about 2 ms, implement direct backends behind `IOutputDevice`.
- **Timeline buffer**: each decoded frame is written at `deviceFrameIndex(hostToLocal(playAtHostUs))`. A frame whose target index is already in the past is dropped and counted as `late`. Gaps are filled by Opus PLC, then silence.
- **Drift control**: resampler ratio = `1 + skewPpm/1e6`, plus a small PI correction on residual phase error, capped at +/-200 ppm. If error exceeds 30 ms, hard resync (flush, re-anchor, 20 ms fade in).
- **Per-device offset**: user-set `offsetMs` (range +/-500) added to the playout time, to compensate for output latency differences (Bluetooth, phones).

## 7a. Host local playback (open problem)

WASAPI loopback captures what the host plays, so the host's own speakers would play ~`targetLatencyMs` **earlier** than clients. Replaying the capture to the same device would create a feedback loop.

- **v1 mode "Silent host"**: the host's output is turned down or muted by the user. Phase 1 must **verify empirically** whether loopback level is affected by master volume and mute, and record the result in `docs/findings.md`.
- **v2 mode "Delayed host"**: route system audio to a virtual audio cable, capture from it, and render a delayed copy to the real speakers. Requires a third-party virtual driver; design the capture interface so the source device is selectable.

## 8. Wire protocol (version 1)

All multi-byte integers are **big-endian**. Never `memcpy` a struct onto the wire; use explicit serialize/deserialize helpers. `kProtocolVersion = 1`. Default ports (configurable): TCP control 47800, UDP data 47801.

**UDP common header (8 bytes):** `magic u16 = 0x4348` | `version u8` | `type u8` | `sessionId u32`

| Type | Value | Body |
|---|---|---|
| `AUDIO` | 1 | `seq u32`, `playAtHostUs u64`, `flags u8`, `payloadLen u16`, `payload[]` (max 1200 bytes) |
| `PING` | 2 | `pingId u32`, `t0 u64` |
| `PONG` | 3 | `pingId u32`, `t0 u64`, `t1 u64`, `t2 u64` |

Drop any packet with a bad magic, version, session id, or a length that exceeds the datagram.

**TCP control:** each message is `u32 length` + UTF-8 JSON (nlohmann/json), max 64 KB.

| Message | Direction | Fields |
|---|---|---|
| `hello` | C -> H | `name`, `platform`, `protocol`, `pin` |
| `welcome` | H -> C | `sessionId`, `udpPort`, `sampleRate`, `channels`, `frameMs`, `targetLatencyMs`, `hostUs` |
| `reject` | H -> C | `reason` (`bad_pin`, `version`, `full`) |
| `set_volume`, `set_mute` | H -> C | `value` |
| `set_offset_ms` | H -> C or C -> H | `value` |
| `set_target_latency_ms` | H -> C | `value` |
| `stats` | C -> H | `syncErrorUs`, `skewPpm`, `underruns`, `late`, `lossPct`, `bufferMs` (every 1 s) |
| `bye` | both | none |

## 9. Discovery and pairing

- mDNS service `_chorus._tcp.local.` with TXT `name`, `ver`. Fallback: manual `ip:port`.
- The host shows a random **4-digit PIN** and a QR code encoding `chorus://<ip>:<port>?pin=<pin>`.
- Reject after 5 bad PIN attempts per IP per minute. Never log PINs.
- v1 is unencrypted and LAN-only; warn in the UI. v2 adds encryption (libsodium) using a proper pairing handshake (evaluate SPAKE2). **Do not derive keys directly from a 4-digit PIN.**

## 10. Platform matrix

| | Host capture | Client output | Background play | Discovery |
|---|---|---|---|---|
| Windows | WASAPI loopback (v1 target) | WASAPI | n/a | mdns lib |
| macOS | ScreenCaptureKit (13+) or BlackHole | CoreAudio | n/a | mdns lib |
| Linux | PipeWire/Pulse monitor | PipeWire/ALSA | n/a | Avahi or mdns lib |
| Android | not supported in v1 | AAudio via Oboe/miniaudio | Foreground service + wake lock + Wi-Fi lock | `NsdManager` |
| iOS | not supported | AVAudioEngine | Background audio mode | Bonjour |

## 11. Repository layout

```
chorus/
  core/            static library (no UI)
    platform/ codec/ net/ proto/ sync/ playback/ session/ app/
  apps/
    cli/           chorus_host, chorus_client (Phases 1-4)
    desktop/       Qt 6 QML app (Phase 5)
    android/       Kotlin + Compose shell, JNI into core (Phase 6)
  tests/           unit + integration (Catch2)
  tools/           measure_offset.py, click_track generator, netem scripts
  docs/            findings.md, decisions/NNN-*.md
  architecture.md  project_phases.md  rules.md  Ui.md
```

## 12. Dependencies

miniaudio, libopus, standalone Asio, nlohmann/json, speexdsp (resampler), a small mDNS library, nayuki qrcodegen, spdlog, Catch2, Qt 6 (LGPL, dynamic link). Desktop deps via vcpkg manifest; pin versions. Adding any other dependency needs approval.

## 13. Observability

Every client exposes `Stats` (sync error, skew, underruns, late frames, loss, buffer fill). Real-time code only increments atomics; a non-RT thread logs and reports them. The UI and CLI show live stats.

## 14. Known risks

1. Host local playback and loopback behavior (section 7a).
2. Device timestamp accuracy on Windows and Android output latency variance.
3. Phone Wi-Fi power saving adding jitter; needs the Wi-Fi lock.
4. Client isolation on public/college Wi-Fi blocks device-to-device traffic; test on a hotspot.
5. Bluetooth outputs add large, variable latency; warn users and rely on manual offset.
6. Cheap laptops may have clock skew beyond the +/-200 ppm correction cap; log and surface it.
