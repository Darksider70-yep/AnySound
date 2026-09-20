#!/usr/bin/env python3
"""
tools/measure_offset.py
Analyzes stereo audio recordings (Channel 0 = Host, Channel 1 = Client) or timestamp logs
to compute cross-correlation latency, time offset, and clock jitter between devices.
"""

import sys
import wave
import struct
from pathlib import Path

def analyze_stereo_wav(wav_path: str):
    path = Path(wav_path)
    if not path.exists():
        print(f"Error: File not found: {wav_path}")
        return 1

    with wave.open(str(path), "rb") as wav_file:
        channels = wav_file.getnchannels()
        sampwidth = wav_file.getsampwidth()
        rate = wav_file.getframerate()
        frames = wav_file.getnframes()

        print(f"Analyzing {path.name}: {channels} ch, {rate} Hz, {frames} frames ({frames/rate:.2f} s)")

        if channels != 2:
            print("Warning: measure_offset expects stereo input (Left = Host, Right = Client).")

        raw_data = wav_file.readframes(frames)

    # Decode 16-bit PCM
    if sampwidth == 2:
        samples = struct.unpack(f"<{frames * channels}h", raw_data)
        ch0 = [samples[i * channels] / 32768.0 for i in range(frames)]
        ch1 = [samples[i * channels + 1] / 32768.0 for i in range(frames)]
    else:
        print(f"Unsupported sample width: {sampwidth} bytes (expected 2 bytes / 16-bit).")
        return 1

    # Simple peak-to-peak threshold detection
    threshold = 0.3
    ch0_clicks = []
    ch1_clicks = []

    for i in range(1, frames - 1):
        if ch0[i] > threshold and ch0[i] > ch0[i-1] and ch0[i] > ch0[i+1]:
            if not ch0_clicks or (i - ch0_clicks[-1]) > (rate * 0.2):  # Minimum 200 ms between clicks
                ch0_clicks.append(i)
        if ch1[i] > threshold and ch1[i] > ch1[i-1] and ch1[i] > ch1[i+1]:
            if not ch1_clicks or (i - ch1_clicks[-1]) > (rate * 0.2):
                ch1_clicks.append(i)

    print(f"Detected clicks - Host (CH0): {len(ch0_clicks)}, Client (CH1): {len(ch1_clicks)}")

    matched_offsets_ms = []
    for h_idx in ch0_clicks:
        # Find nearest client click within 100 ms
        closest_c = None
        min_dist = rate * 0.1  # 100 ms max window
        for c_idx in ch1_clicks:
            dist = abs(c_idx - h_idx)
            if dist < min_dist:
                min_dist = dist
                closest_c = c_idx
        if closest_c is not None:
            offset_samples = closest_c - h_idx
            offset_ms = (offset_samples / rate) * 1000.0
            matched_offsets_ms.append(offset_ms)

    if matched_offsets_ms:
        avg_offset = sum(matched_offsets_ms) / len(matched_offsets_ms)
        min_offset = min(matched_offsets_ms)
        max_offset = max(matched_offsets_ms)
        jitter = max_offset - min_offset
        print("\n--- Synchronization Alignment Results ---")
        print(f"Matched click pairs: {len(matched_offsets_ms)}")
        print(f"Average time offset (Client - Host): {avg_offset:+.2f} ms")
        print(f"Min offset: {min_offset:+.2f} ms | Max offset: {max_offset:+.2f} ms")
        print(f"Peak-to-peak click jitter: {jitter:.2f} ms")
        if abs(avg_offset) <= 10.0:
            print("[PASS] Offset is within Phase 2 acceptance threshold (<= 10 ms).")
        else:
            print(f"[FAIL] Offset exceeds 10 ms threshold (observed: {avg_offset:+.2f} ms).")
    else:
        print("No correlated click pairs found. Ensure both channels have audible click peaks.")

    return 0

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python measure_offset.py <recorded_stereo.wav>")
        sys.exit(1)
    sys.exit(analyze_stereo_wav(sys.argv[1]))
