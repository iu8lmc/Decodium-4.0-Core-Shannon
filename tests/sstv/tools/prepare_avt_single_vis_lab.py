#!/usr/bin/env python3
"""Copy a manually anchored single-VIS AVT section from 48 kHz stereo float PCM."""
import argparse
import json
import math
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
parser.add_argument('destination', type=Path)
parser.add_argument('--countdown-seconds', type=float, required=True)
parser.add_argument('--duration-seconds', type=float, default=36.0)
args = parser.parse_args()
source = args.source.resolve(strict=True)
size = source.stat().st_size
if (not source.is_file() or not size or size % 8 or size > 180 * 48000 * 8
        or not math.isfinite(args.countdown_seconds) or args.countdown_seconds < 1.01
        or not math.isfinite(args.duration_seconds) or not 1.01 < args.duration_seconds <= 120):
    raise SystemExit('invalid bounded PCM input or timing')
start = round((args.countdown_seconds - 1.01) * 48000)
count = round(args.duration_seconds * 48000)
if (start + count) * 8 > size:
    raise SystemExit('requested interval exceeds input')
with source.open('rb') as incoming, args.destination.open('xb') as output:
    incoming.seek(start * 8)
    remaining = count * 8
    while remaining:
        data = incoming.read(min(1024 * 1024, remaining))
        if not data: raise RuntimeError('input truncated while reading')
        output.write(data)
        remaining -= len(data)
print(json.dumps({'start_frame': start, 'frames': count, 'countdown_seconds': args.countdown_seconds,
                  'destination': str(args.destination)}, sort_keys=True))
