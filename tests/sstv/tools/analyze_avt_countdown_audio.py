#!/usr/bin/env python3
"""Diagnostic for consecutive AVT24 countdown words in a bounded audio capture."""
import array
import json
import math
from pathlib import Path
import subprocess
import sys

if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != '--narrow'):
    raise SystemExit('expected WAV or 48 kHz stereo f32le capture [--narrow]')
path = Path(sys.argv[1]).resolve(strict=True)
narrow = len(sys.argv) == 3
args = ['ffmpeg', '-v', 'error']
if path.suffix == '.f32':
    args += ['-f', 'f32le', '-ar', '48000', '-ac', '2']
raw = subprocess.check_output(args + ['-i', str(path), '-t', '15', '-ar', '12000', '-ac', '1',
                                    '-f', 's16le', '-'], timeout=30)
samples = array.array('h'); samples.frombytes(raw)
if sys.byteorder != 'little': samples.byteswap()
if len(samples) < 12000:
    raise SystemExit('audio shorter than one second')
step, size = 12, 100
frequencies = (1700, 1900, 2100) if narrow else (1600, 1900, 2200)
coefficients = [2 * math.cos(2 * math.pi * f / 12000) for f in frequencies]
tones = []
for pos in range(0, len(samples) - size, step):
    block = samples[pos:pos + size]
    energy = sum(x * x for x in block)
    if energy < size * 20 * 20:
        tones.append(None); continue
    powers = []
    for coefficient in coefficients:
        a = b = 0.0
        for value in block:
            a, b = value + coefficient * a - b, a
        powers.append(a * a + b * b - coefficient * a * b)
    best = max(range(3), key=powers.__getitem__)
    tones.append(best if 2 * powers[best] / (size * energy) >= 0.48 else None)

symbol_samples = 12000 / 102.4
matches = []
for start in range(len(tones)):
    if tones[start] != 1: continue
    bits = []
    for bit in range(16):
        index = start + round((bit + 1) * symbol_samples / step)
        if index >= len(tones) or tones[index] not in (0, 2): break
        bits.append(1 if tones[index] == 2 else 0)
    if len(bits) != 16: continue
    first = sum(bits[i] << (7 - i) for i in range(8))
    inverse = sum(bits[i + 8] << (7 - i) for i in range(8))
    if (first ^ inverse) == 255 and first >> 5 == 2:
        matches.append((start * step / 12000, first & 31))

best = []
for start, counter in matches:
    chain = [(start, counter)]
    skipped = 0
    for value in range(counter + 1, 32):
        expected = start + (value - counter) * 17 / 102.4
        found = next(((time, count) for time, count in matches
                      if count == value and abs(time - expected) <= 0.006), None)
        if found is None:
            skipped += 1
            if skipped > 1: break
            continue
        skipped = 0
        chain.append(found)
    if len(chain) > len(best): best = chain
print(json.dumps({'file': str(path), 'seconds': len(samples) / 12000,
                  'matching_frames': len(matches), 'longest_run_allowing_single_gaps': len(best),
                  'first_time': best[0][0] if best else None,
                  'first_counter': best[0][1] if best else None,
                  'last_counter': best[-1][1] if best else None}, sort_keys=True))
