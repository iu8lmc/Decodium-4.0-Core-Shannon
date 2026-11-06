#!/usr/bin/env python3
"""Insert one bounded false white pulse in captured 240-LPM Radiofax phasing."""
import argparse
from array import array
import math
from pathlib import Path
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
parser.add_argument('destination', type=Path)
parser.add_argument('variant', choices=('a', 'b'))
group = parser.add_mutually_exclusive_group()
group.add_argument('--two-pulses', action='store_true')
group.add_argument('--four-pairs', action='store_true', help='insert four separated pairs of pulses')
group.add_argument('--close-pairs', action='store_true', help='insert four pairs two lines apart')
group.add_argument('--persistent-pairs', action='store_true', help='insert pairs on 102 drifting phasing lines')
group.add_argument('--broad-white', action='store_true', help='insert eight 65%%-line white disturbances')
args = parser.parse_args()
if sys.byteorder != 'little':
    raise SystemExit('this diagnostic requires little-endian float PCM')
if args.source.resolve() == args.destination.resolve() or args.destination.exists():
    raise SystemExit('refusing to overwrite input or output')
size = args.source.stat().st_size
if size % 8 or not 48000 * 8 * 45 <= size <= 48000 * 8 * 240:
    raise SystemExit('expected bounded stereo 48 kHz float capture')
samples = array('f')
with args.source.open('rb') as source:
    samples.frombytes(source.read())
rate = 48000
block = rate // 100
onset = None
for frame in range(0, len(samples) // 2 - block, block):
    power = sum(samples[2 * i] ** 2 for i in range(frame, frame + block)) / block
    if power > 0.05 ** 2:
        onset = frame
        break
if onset is None:
    raise SystemExit('no audio onset')
# 5 s APT, then 18 complete 0.25 s phasing periods and an interior
# black interval. Both variants use a full nominal 5% white-pulse length.
within = 0.10 if args.variant == 'a' else 0.15
start = onset + round((5.0 + 18 * 0.25 + within) * rate)
length = rate // 80
if args.broad_white:
    start = onset + round((5.0 + 10 * 0.25 + 0.0625) * rate)
    length = round(0.1625 * rate)
starts = ([start + round(line * (11964 if args.variant == 'a' else 12036)
                         if args.persistent_pairs else line * 0.25 * rate) + offset
           for line in (range(102) if args.persistent_pairs else
                        (0, 2, 4, 6) if args.close_pairs else (0, 18, 36, 54))
           for offset in (0, round(0.06 * rate))]
          if args.four_pairs or args.close_pairs or args.persistent_pairs else
          [start, start + round(0.06 * rate)] if args.two_pulses else [start])
if args.broad_white:
    starts = [start + line * rate // 4 for line in range(8)]
if starts[-1] + length >= len(samples) // 2:
    raise SystemExit('pulse falls outside capture')
def magnitude(at, frequency):
    real = imag = 0.0
    for j in range(length):
        angle = 2 * math.pi * frequency * j / rate
        real += samples[2 * (at + j)] * math.cos(angle)
        imag += samples[2 * (at + j)] * math.sin(angle)
    return math.hypot(real, imag)
for at in starts:
    black, white = magnitude(at, 1500), magnitude(at, 2300)
    if black < 1.5 * white or black < length * 0.05:
        raise SystemExit('target interval is not a clear black phasing tone')
for at in starts:
    for j in range(length):
        value = 0.35 * math.sin(2 * math.pi * 2300 * j / rate)
        samples[2 * (at + j)] = samples[2 * (at + j) + 1] = value
with args.destination.open('xb') as destination:
    samples.tofile(destination)
print(f'onset={onset} pulse_start={start} pulse_frames={length} pulse_count={len(starts)} black={black:.2f} white={white:.2f}')
