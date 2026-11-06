#!/usr/bin/env python3
"""Replace the first 100 ms of a bounded stereo AVT lab recording with a tone."""
import argparse
import math
from pathlib import Path
import struct

RATE = 48_000
FRAMES = RATE // 10
FRAME_BYTES = 8


def make_probe(source: Path, destination: Path, frequency: int) -> None:
    source = source.resolve(strict=True)
    size = source.stat().st_size
    if (not source.is_file() or size < FRAMES * FRAME_BYTES
            or size % FRAME_BYTES or size > 180 * RATE * FRAME_BYTES
            or frequency not in (0, 1100, 1500)):
        raise ValueError('invalid bounded stereo capture or probe frequency')
    with source.open('rb') as incoming, destination.open('xb') as output:
        for index in range(FRAMES):
            sample = 0.3 * math.sin(2.0 * math.pi * frequency * index / RATE)
            output.write(struct.pack('<ff', sample, sample))
        incoming.seek(FRAMES * FRAME_BYTES)
        while data := incoming.read(1024 * 1024):
            output.write(data)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--frequency', type=int, choices=(0, 1100, 1500), required=True)
    args = parser.parse_args()
    make_probe(args.source, args.destination, args.frequency)


if __name__ == '__main__':
    main()
