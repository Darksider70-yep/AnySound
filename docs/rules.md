# Rules: Chorus (working title)

These rules apply to every change. "Must" means required; "should" means follow unless you write down why not.

## 1. Working agreement

1. Read `architecture.md`, `project_phases.md`, and `Ui.md` before writing code. Work only on the **current phase**.
2. For any change touching more than a few files, write a short plan (files, approach, risks) and wait for approval before coding.
3. Do not invent requirements. If the spec is ambiguous or conflicts with reality (for example, a measured limitation), stop and write `docs/decisions/NNN-title.md` (context, options, recommendation), then ask.
4. Do not add dependencies, change the wire protocol, or change the threading model without approval. Protocol changes bump `kProtocolVersion` and update `architecture.md` in the same commit.
5. No placeholder implementations presented as finished. Any stub must be marked `// TODO(phase-N):` and listed in the phase summary.
6. Keep commits small and focused. Do not refactor unrelated code.

## 2. Language and style

- C++20, CMake 3.25 or newer. Namespace `chorus::`; no `using namespace` in headers.
- Naming: types `PascalCase`, functions and variables `snake_case`, private members `trailing_`, constants `kPascalCase`, enums `enum class`.
- Formatting by `.clang-format` (4 spaces, 100 columns). `clang-tidy` clean.
- Warnings as errors: `/W4 /WX` (MSVC), `-Wall -Wextra -Wpedantic -Werror` (Clang/GCC).
- RAII everywhere. No raw `new`/`delete`, no owning raw pointers. Use `std::span`, `std::string_view`, `[[nodiscard]]`, `const` by default.
- Errors: exceptions are allowed only during setup and teardown. Hot paths and network code return `Result<T>` (`tl::expected`). Never throw across a module boundary on a hot path.
- Platform-specific code lives only in `core/platform/` and `apps/`. Everything else must compile on all targets.

## 3. Real-time audio rules (hard)

Code running in an audio callback (capture or playback) must not:

- allocate or free memory (`new`, `malloc`, `std::vector` growth, `std::string`, `std::function` construction)
- lock a mutex, wait on a condition variable, or call anything that might block
- perform I/O: sockets, files, logging, `printf`
- throw exceptions
- do unbounded loops or work whose cost depends on untrusted input

It must:

- read and write only pre-allocated buffers via lock-free SPSC structures and `std::atomic`
- use relaxed or acquire/release atomics deliberately, with a comment explaining the ordering
- finish well within its budget (under 25% of the buffer period)
- report events (underrun, late frame) by incrementing atomic counters or pushing to a lock-free queue

## 4. Threading

- Every class documents its **thread affinity** in its header comment (which thread calls what).
- Cross-thread communication uses SPSC queues or atomics. No shared mutable state behind ad hoc locks.
- The UI thread never calls into the audio threads and never blocks on the network.
- TSAN must be clean on Linux CI.

## 5. Networking and security

- Treat every incoming byte as hostile. Validate magic, version, session id, and lengths before reading further. Bounds-check everything.
- Cap all lengths (UDP payload 1200 bytes, control message 64 KB). Never allocate based on an unchecked length.
- Serialize with explicit big-endian helpers; never `memcpy` structs to or from the wire.
- All sockets have timeouts; all sessions have keepalive and cleanup.
- Rate-limit PIN attempts. Never log PINs or full packet contents.
- Bind only to the LAN interface in use; do not expose to the internet. Do not add telemetry.

## 6. Testing

- Logic in `sync/`, `proto/`, and framing **must** have unit tests (Catch2), including edge cases: reorder, duplicate, loss, wraparound, clock jumps, malformed input.
- `ClockEstimator`, `JitterBuffer`, and `DriftController` are tested against simulated network conditions (jitter, asymmetry, outliers, skew).
- Integration tests use loopback sockets. CI runs ASAN, UBSAN, and (Linux) TSAN builds.
- Parser fuzzing from Phase 3 onward.
- Bug fixes come with a regression test.

## 7. Performance budget

- Host: under 5% CPU with one client, under 10% with four (on a mid-range laptop).
- Client: under 5% CPU.
- Memory is stable after warm-up (no growth over 60 minutes).

## 8. Logging and diagnostics

- Use spdlog on non-real-time threads only. Levels: `error` (broken), `warn` (degraded), `info` (lifecycle), `debug` (verbose, off by default).
- Real-time code exposes counters; a non-RT thread reads and logs them.
- Every user-facing failure has a clear cause and a clear next step (see `Ui.md`, copy rules).

## 9. Git and CI

- Branches: `phase-N/short-name`. Conventional commits (`feat:`, `fix:`, `test:`, `docs:`, `refactor:`).
- Never force-push `main`. CI must be green before merge.
- Do not commit binaries, build output, or secrets.

## 10. UI rules

- Follow `Ui.md` exactly for tokens, copy, states, and accessibility.
- UIs talk only to `AppController` (state snapshot plus events). No audio or network logic in UI code.
- Throttle live UI updates (meters, sync readouts) to 10-30 Hz.
- All user-visible strings live in one resource file. All colors, sizes, and spacing come from design tokens; no hard-coded values.

## 11. Licensing

Allowed: MIT, BSD, Apache-2.0, zlib, ISC, public domain, and LGPL with dynamic linking (Qt). Ask before adding anything GPL, AGPL, or with unclear terms.

## 12. Definition of done (per change)

- [ ] Builds warning-free on CI
- [ ] Tests added or updated and passing (including sanitizers)
- [ ] No violations of sections 3 to 5
- [ ] Docs updated (`architecture.md`, `docs/findings.md`, or a decision record)
- [ ] Acceptance checks for the current phase re-run if affected
- [ ] Summary written: what changed, what was measured, open questions
