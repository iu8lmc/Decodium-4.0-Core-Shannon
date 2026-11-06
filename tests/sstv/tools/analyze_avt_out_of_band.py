#!/usr/bin/env python3
"""Offline spectral diagnostic for a 48 kHz stereo float AVT capture."""
import argparse
import json
import math
from pathlib import Path

import numpy as np


def measure(samples, rate=48000, injected_hz=0.0, injected_peak=0.0):
    """Estimate strongest out-of-band tone in one bounded, one-second window."""
    samples = np.asarray(samples, dtype=np.float64)
    if samples.ndim != 1 or samples.size != rate:
        raise ValueError('expected exactly one second of mono samples')
    if not np.isfinite(samples).all() or not math.isfinite(injected_hz) or not math.isfinite(injected_peak):
        raise ValueError('non-finite input')
    if not 0 <= injected_hz <= 5500 or not 0 <= injected_peak <= 0.2:
        raise ValueError('invalid injected tone')
    if injected_peak:
        samples = samples + injected_peak * np.sin(2 * np.pi * injected_hz * np.arange(rate) / rate)
    spectrum = np.abs(np.fft.rfft(samples * np.hanning(rate)))
    frequencies = np.fft.rfftfreq(rate, 1.0 / rate)
    outside = ((frequencies >= 100) & (frequencies <= 800)) | ((frequencies >= 3000) & (frequencies <= 5000))
    inside = (frequencies >= 1500) & (frequencies <= 2300)
    guard = ((frequencies >= 801) & (frequencies <= 1400)) | ((frequencies >= 2400) & (frequencies <= 2999))
    indices = np.flatnonzero(outside)
    peak_index = int(indices[np.argmax(spectrum[indices])])
    peak = float(spectrum[peak_index])
    in_peak = float(np.max(spectrum[inside]))
    in_power = float(np.sum(spectrum[inside] ** 2))
    out_power = float(np.sum(spectrum[outside] ** 2))
    guard_indices = np.flatnonzero(guard)
    guard_peak_index = int(guard_indices[np.argmax(spectrum[guard_indices])])
    guard_peak = float(spectrum[guard_peak_index])
    guard_power = float(np.sum(spectrum[guard] ** 2))
    # Hann-windowed magnitude ratios; this is a diagnostic, not a detector.
    return {'frequency_hz': float(frequencies[peak_index]),
            'outside_to_inside_peak': peak / in_peak if in_peak > 0 else None,
            'outside_to_inside_power': out_power / in_power if in_power > 0 else None,
            'outside_peak': peak,
            'guard_frequency_hz': float(frequencies[guard_peak_index]),
            'guard_to_inside_peak': guard_peak / in_peak if in_peak > 0 else None,
            'guard_to_inside_power': guard_power / in_power if in_power > 0 else None}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--start', type=float, required=True)
    parser.add_argument('--inject-hz', type=float, default=0.0)
    parser.add_argument('--inject-peak', type=float, default=0.0)
    args = parser.parse_args()
    if not math.isfinite(args.start) or args.start < 0:
        parser.error('start must be non-negative')
    raw = np.memmap(args.capture, dtype='<f4', mode='r')
    if raw.size % 2:
        parser.error('capture must contain complete stereo frames')
    stereo = raw.reshape(-1, 2)
    start = int(args.start * 48000)
    if start + 48000 > len(stereo):
        parser.error('one-second window exceeds capture')
    mono = np.asarray(stereo[start:start + 48000], dtype=np.float64).mean(axis=1)
    print(json.dumps(measure(mono, injected_hz=args.inject_hz, injected_peak=args.inject_peak), sort_keys=True))


if __name__ == '__main__':
    main()
