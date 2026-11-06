#!/usr/bin/env python3
"""Characterise FFmpeg's zero-shift filter with A/B synthetic tones.

Requires numpy and ffmpeg; writes only to a temporary directory. This is a
laboratory measurement, not a HAMDRM decoder or a radio test.
"""
import subprocess
import tempfile
import wave
from pathlib import Path

import numpy as np


def main():
    rate = 48000
    frequencies = np.arange(500, 2501, 250)
    with tempfile.TemporaryDirectory(prefix="decodium-hamdrm-tones-") as directory:
        root = Path(directory)
        for label, phase in (("a", 0.0), ("b", 0.71)):
            ratios = []
            for frequency in frequencies:
                time = np.arange(rate) / rate
                original = np.rint(14000 * np.cos(2 * np.pi * frequency * time + phase)).astype("<i2")
                source, filtered = root / "in.wav", root / "out.wav"
                with wave.open(str(source), "wb") as audio:
                    audio.setnchannels(1)
                    audio.setsampwidth(2)
                    audio.setframerate(rate)
                    audio.writeframes(original.tobytes())
                subprocess.run(["ffmpeg", "-y", "-v", "error", "-i", str(source), "-af",
                                "afreqshift=shift=0", str(filtered)], check=True)
                raw = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(filtered),
                                               "-f", "s16le", "-ac", "1", "-ar", str(rate), "-"])
                received = np.frombuffer(raw, dtype="<i2")
                end = min(len(original), len(received)) - rate // 5
                indices = np.arange(rate // 5, end)
                if len(indices) < rate // 2:
                    raise RuntimeError("filtered tone is too short")
                basis = np.exp(-2j * np.pi * frequency * indices / rate)
                ratios.append(np.sum(received[indices] * basis) / np.sum(original[indices] * basis))
            phases = np.unwrap(np.angle(ratios))
            residual = (phases - np.polyval(np.polyfit(frequencies, phases, 1), frequencies)) * 180 / np.pi
            print(label, "amplitude", np.round(np.abs(ratios), 5).tolist(),
                  "phase_degrees", np.round(phases * 180 / np.pi, 2).tolist(),
                  "linear_phase_residual_degrees", np.round(residual, 2).tolist())


if __name__ == "__main__":
    main()
