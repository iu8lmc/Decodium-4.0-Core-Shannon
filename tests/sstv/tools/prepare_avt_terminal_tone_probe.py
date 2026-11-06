#!/usr/bin/env python3
"""Create a post-capture AVT tail probe, never a production waveform."""
import array
import math
from pathlib import Path
import sys

if len(sys.argv) != 3:
    raise SystemExit('expected INPUT.f32 OUTPUT.f32')
source = Path(sys.argv[1]).resolve(strict=True)
destination = Path(sys.argv[2])
samples = array.array('f')
samples.frombytes(source.read_bytes())
if sys.byteorder != 'little':
    samples.byteswap()
if not samples or len(samples) % 2:
    raise SystemExit('expected nonempty stereo float32')
# The laboratory interferer is bounded to 0.08 after gain. A threshold of 0.1
# locates the end of the stronger transmitted signal, not a protocol marker.
end = next((i // 2 + 1 for i in range(len(samples)-2, -1, -2)
            if max(abs(samples[i]), abs(samples[i+1])) > 0.1), None)
length = 480  # 10 ms of the 1700 Hz black tone used by these fixtures.
if end is None or end + length > len(samples):
    raise SystemExit('no suitable trailing silence/tone interval')
for frame in range(length):
    envelope = min(1.0, frame / 48.0, (length-frame) / 48.0)
    value = 0.4 * envelope * math.sin(2 * math.pi * 1700 * frame / 48000)
    for channel in (0, 1):
        idx = 2 * (end + frame) + channel
        samples[idx] += value
        if not math.isfinite(samples[idx]) or abs(samples[idx]) > 1:
            raise SystemExit('probe would clip')
if sys.byteorder != 'little':
    samples.byteswap()
with destination.open('xb') as out:
    samples.tofile(out)
print('terminal-frame', end, 'added-frames', length, 'output', destination)
