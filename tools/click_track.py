#!/usr/bin/env python3
"""
tools/click_track.py
Generates a 48 kHz, 16-bit stereo WAV click track with sharp 1 kHz pulses every 1000 ms.
Used for precise acoustic and electrical time-offset measurements between host and clients.
"""

import math
import struct
import wave
import sys
from pathlib import Path

def generate_click_track(filename: str = "click_track.wav", duration_seconds: int = 60, bpm: int = 60, sample_rate: int = 48000):
    num_samples = duration_seconds * sample_rate
    interval_samples = int((60.0 / bpm) * sample_rate)
    click_duration_samples = int(0.005 * sample_rate)  # 5 ms click pulse
    click_freq = 1000.0  # 1 kHz sine pulse

    print(f"Generating {duration_seconds}s click track at {sample_rate} Hz, {bpm} BPM...")

    audio_data = bytearray()

    for i in range(num_samples):
        pos_in_beat = i % interval_samples
        sample_val = 0.0

        if pos_in_beat < click_duration_samples:
            # Apply Hanning window envelope to avoid broadband spectral splatter
            t = pos_in_beat / sample_rate
            env = 0.5 * (1.0 - math.cos(2.0 * math.pi * pos_in_beat / click_duration_samples))
            sample_val = math.sin(2.0 * math.pi * click_freq * t) * env * 0.9

        # 16-bit signed PCM
        int_sample = int(max(-32768, min(32767, sample_val * 32767.0)))
        # Stereo: duplicate to both channels
        audio_data.extend(struct.pack("<hh", int_sample, int_sample))

    out_path = Path(filename)
    with wave.open(str(out_path), "wb") as wav_file:
        wav_file.setnchannels(2)
        wav_file.setsampwidth(2)
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(audio_data)

    print(f"Click track successfully written to {out_path.resolve()}")

if __name__ == "__main__":
    out_file = sys.argv[1] if len(sys.argv) > 1 else "click_track.wav"
    duration = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    generate_click_track(out_file, duration)
