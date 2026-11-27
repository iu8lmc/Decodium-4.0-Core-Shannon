#!/usr/bin/env python3
"""Shorten a centered 240-LPM lab recording after 19 phasing lines.

Apply prepare_radiofax_phasing_glitch.py --two-pulses first. The splice keeps
the recorded white end line and image; it is a controlled synthetic signal,
not an unmodified transmission.
"""
import argparse
from array import array
from pathlib import Path
import shutil
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    if sys.byteorder != 'little':
        parser.error('requires little-endian float PCM')
    size = args.source.stat().st_size
    rate, frame_bytes = 48000, 8
    if size % frame_bytes or not 45 * rate * frame_bytes <= size <= 240 * rate * frame_bytes:
        parser.error('expected bounded 48 kHz stereo float recording')
    if args.source.resolve() == args.destination.resolve() or args.destination.exists():
        parser.error('refusing to overwrite input or output')
    with args.source.open('rb') as source:
        values = array('f')
        values.frombytes(source.read(min(size, 30 * rate * frame_bytes)))
        block = rate // 100
        onset = next((frame for frame in range(0, len(values) // 2 - block, block)
                      if sum(values[2 * i] ** 2 for i in range(frame, frame + block)) / block
                      > 0.05 ** 2), None)
        if onset is None:
            parser.error('no audio onset in first 30 seconds')
        first = onset + 39 * rate // 4
        last = onset + 35 * rate
        if last * frame_bytes >= size:
            parser.error('recording ends before the white end line')
        source.seek(0)
        with args.destination.open('xb') as destination:
            remaining = first * frame_bytes
            while remaining:
                chunk = source.read(min(1024 * 1024, remaining))
                if not chunk:
                    raise EOFError('truncated input')
                destination.write(chunk)
                remaining -= len(chunk)
            source.seek(last * frame_bytes)
            shutil.copyfileobj(source, destination, 1024 * 1024)
    print(f'onset={onset} removed_frames={last-first} splice={first}')


if __name__ == '__main__':
    main()
