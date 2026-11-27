#!/usr/bin/env python3
"""Offline coherent-tone survey of recorded AVT audio; not a live detector."""
import argparse
from array import array
import json
import math
from pathlib import Path
import sys

parser = argparse.ArgumentParser()
parser.add_argument('capture', type=Path)
parser.add_argument('--start', type=int, required=True)
parser.add_argument('--seconds', type=int, default=4)
parser.add_argument('--tone-hz', type=int, default=1700)
parser.add_argument('--tone-peak', type=float, default=0.0)
args = parser.parse_args()
if not 0 <= args.start <= 90 or not 2 <= args.seconds <= 8 or not 1500 <= args.tone_hz <= 2300 or not math.isfinite(args.tone_peak) or not 0 <= args.tone_peak <= 0.2:
    raise SystemExit('invalid survey parameters')
size = args.capture.stat().st_size
if not size or size % 8 or size > 90 * 48000 * 8 or (args.start + args.seconds) * 48000 * 8 > size:
    raise SystemExit('invalid or short 48 kHz stereo float capture')
values = array('f')
with args.capture.open('rb') as source:
    source.seek(args.start * 48000 * 8)
    values.frombytes(source.read(args.seconds * 48000 * 8))
if sys.byteorder != 'little': values.byteswap()
if any(not math.isfinite(value) or abs(value) > 1 for value in values):
    raise SystemExit('non-finite or out-of-range capture')
results = []
history = {hz: [] for hz in range(1500, 2301, 20)}
for window in range(args.seconds):
    samples = values[window * 96000:(window + 1) * 96000:2]
    if args.tone_peak:
        for index in range(len(samples)):
            samples[index] += args.tone_peak * math.sin(2 * math.pi * args.tone_hz * index / 48000)
    scores = []
    for hz in range(1500, 2301, 20):
        coefficient = 2 * math.cos(2 * math.pi * hz / 48000)
        previous = before = 0.0
        for sample in samples:
            current = sample + coefficient * previous - before
            before, previous = previous, current
        power = max(0.0, previous * previous + before * before - coefficient * previous * before)
        amplitude = 2 * math.sqrt(power) / len(samples)
        scores.append((amplitude, hz))
        history[hz].append(amplitude)
    power, frequency = max(scores)
    results.append({'second': args.start + window, 'frequency_hz': frequency, 'peak_amplitude': round(power, 6)})
stable = sorted(((min(amplitudes), hz) for hz, amplitudes in history.items()), reverse=True)
print(json.dumps({'capture': str(args.capture), 'injected_tone_hz': args.tone_hz, 'injected_peak': args.tone_peak,
                  'windows': results, 'persistent': [{'frequency_hz': hz, 'minimum_amplitude': round(amplitude, 6)}
                                                       for amplitude, hz in stable[:3]]}, sort_keys=True))
