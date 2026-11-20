#!/usr/bin/env python3
"""Report runs of exact stereo silence inside a 48 kHz float capture.

Leading and trailing silence is ignored. Internal silence may be intentional;
this diagnostic does not by itself establish a device or transmission fault.
"""
import argparse
import array
import json
from pathlib import Path
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('capture', type=Path)
parser.add_argument('--min-frames', type=int, default=480)
parser.add_argument('--fail-on-gap', action='store_true')
args = parser.parse_args()
if args.min_frames < 1:
    parser.error('--min-frames must be positive')
if args.capture.stat().st_size % 8:
    parser.error('expected interleaved stereo float32 samples')

frame = 0
active = False
run_start = None
gaps = []
with args.capture.open('rb') as source:
    while raw := source.read(65536 * 8):
        values = array.array('f')
        values.frombytes(raw)
        if sys.byteorder != 'little':
            values.byteswap()
        for i in range(0, len(values), 2):
            zero = values[i] == 0.0 and values[i + 1] == 0.0
            if zero and active and run_start is None:
                run_start = frame
            elif not zero:
                if run_start is not None and frame - run_start >= args.min_frames:
                    gaps.append({'start_frame': run_start, 'frames': frame - run_start})
                active = True
                run_start = None
            frame += 1
if not active:
    raise SystemExit('capture contains no non-zero samples')
print(json.dumps({'frames': frame, 'sample_rate': 48000, 'internal_zero_runs': gaps}))
if args.fail_on_gap and gaps:
    raise SystemExit(1)
