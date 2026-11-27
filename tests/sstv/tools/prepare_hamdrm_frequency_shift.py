#!/usr/bin/env python3
"""Frequency-shift a mono 12 kHz s16 recording for offline HAMDRM diagnostics.

Requires NumPy. This whole-recording FFT transform is a laboratory signal
generator, not an RF channel model or production receiver implementation.
"""
import argparse
from pathlib import Path
import wave

import numpy as np

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
parser.add_argument('destination', type=Path)
parser.add_argument('shift_hz', type=float)
args = parser.parse_args()
if args.source.resolve() == args.destination.resolve() or args.destination.exists():
    raise SystemExit('refusing to overwrite input or output')
if not np.isfinite(args.shift_hz) or abs(args.shift_hz) > 100:
    raise SystemExit('shift must be finite and within +/-100 Hz')
size = args.source.stat().st_size
if size < 2 or size % 2 or size > 12000 * 2 * 300:
    raise SystemExit('expected bounded mono 12 kHz s16le PCM')
samples = np.fromfile(args.source, dtype='<i2').astype(np.float64)
n = len(samples)
weights = np.zeros(n)
weights[0] = 1
weights[1:(n + 1) // 2] = 2
if n % 2 == 0:
    weights[n // 2] = 1
analytic = np.fft.ifft(np.fft.fft(samples) * weights)
oscillator = np.exp(2j * np.pi * args.shift_hz * np.arange(n) / 12000)
shifted = np.rint(np.real(analytic * oscillator)).clip(-32768, 32767).astype('<i2')
with args.destination.open('xb') as output:
    with wave.open(output, 'wb') as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(12000)
        wav.writeframes(shifted.tobytes())
print(f'frames={n} shift_hz={args.shift_hz:g} zero_control_exact='
      f'{int(args.shift_hz == 0 and np.array_equal(shifted, samples))}')
