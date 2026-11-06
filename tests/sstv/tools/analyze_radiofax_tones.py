#!/usr/bin/env python3
"""Report diagnostic coherent tone amplitudes in a float-stereo capture."""
import argparse
from array import array
import json
import math
from pathlib import Path
import sys

parser = argparse.ArgumentParser()
parser.add_argument('capture', type=Path)
parser.add_argument('--start', type=float, required=True)
parser.add_argument('--seconds', type=float, default=1.0)
parser.add_argument('--rate', type=int, default=48000)
args = parser.parse_args()
if not 0 <= args.start <= 3600 or not 0 < args.seconds <= 10 or not 8000 <= args.rate <= 192000:
    raise SystemExit('invalid time or sample rate')
count = int(args.seconds * args.rate)
values = array('f')
with args.capture.open('rb') as source:
    source.seek(int(args.start * args.rate) * 8)
    values.frombytes(source.read(count * 8))
if sys.byteorder != 'little':
    values.byteswap()
if len(values) != count * 2:
    raise SystemExit('capture is shorter than requested')
samples = values[::2]
rms = math.sqrt(sum(x * x for x in samples) / len(samples))
amplitudes = {}
for frequency in (450, 1500, 1900, 2300):
    coefficient = 2 * math.cos(2 * math.pi * frequency / args.rate)
    previous = before_previous = 0.0
    for sample in samples:
        current = sample + coefficient * previous - before_previous
        before_previous, previous = previous, current
    power = max(0.0, previous * previous + before_previous * before_previous
                - coefficient * previous * before_previous)
    amplitudes[str(frequency)] = math.sqrt(power) / len(samples)
print(json.dumps({'capture': str(args.capture), 'start': args.start, 'seconds': args.seconds,
                  'channel': 0, 'rms': rms, 'coherent_amplitude': amplitudes}, sort_keys=True))
