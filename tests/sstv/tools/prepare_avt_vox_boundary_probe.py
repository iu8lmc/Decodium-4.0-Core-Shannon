#!/usr/bin/env python3
"""Replace 10 ms at a known 500 ms VOX tail boundary in a lab capture."""
import array
import math
from pathlib import Path
import sys

if len(sys.argv) != 3:
    raise SystemExit('expected INPUT.f32 OUTPUT.f32')
source = Path(sys.argv[1]).resolve(strict=True)
samples = array.array('f'); samples.frombytes(source.read_bytes())
if sys.byteorder != 'little': samples.byteswap()
if not samples or len(samples) % 2:
    raise SystemExit('expected stereo float32')
end = next((i//2+1 for i in range(len(samples)-2, -1, -2)
            if max(abs(samples[i]), abs(samples[i+1])) > .1), None)
length, vox_hang = 480, 24000
boundary = end - vox_hang if end is not None else -1
if boundary < length or boundary + length > len(samples)//2:
    raise SystemExit('capture does not contain the expected VOX interval')

def amplitude(start, hz):
    z = sum(samples[2*(start+i)] * complex(math.cos(2*math.pi*hz*i/48000),
                                           -math.sin(2*math.pi*hz*i/48000))
            for i in range(length))
    return 2*abs(z)/length

if amplitude(boundary-length, 1700) < .3 or amplitude(boundary, 1900) < .2:
    raise SystemExit('boundary does not have the expected black-to-VOX tones')
# 10 ms is an integer number of cycles at both 1700 and 1900 Hz. Copying the
# preceding interval retains the recorded image tone and interference phase.
samples[2*boundary:2*(boundary+length)] = samples[2*(boundary-length):2*boundary]
if sys.byteorder != 'little': samples.byteswap()
with Path(sys.argv[2]).open('xb') as out: samples.tofile(out)
print('estimated-boundary-frame', boundary, 'replaced-frames', length)
