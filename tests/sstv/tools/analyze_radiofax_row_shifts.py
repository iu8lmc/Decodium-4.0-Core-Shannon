#!/usr/bin/env python3
"""Compare per-row cyclic shifts; diagnostic only, not a corrected image."""
import argparse
import json
from pathlib import Path
from analyze_radiofax_2d_alignment import dimensions, gray


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=Path)
    parser.add_argument('received', type=Path)
    parser.add_argument('--max-shift', type=int, default=32)
    args = parser.parse_args()
    width, height = dimensions(args.reference)
    if dimensions(args.received) != (width, height): parser.error('image dimensions differ')
    if width < 2 or not 0 <= args.max_shift < width or width * height > 16_777_216:
        parser.error('invalid raster or shift bounds')
    reference, received = gray(args.reference, width, height), gray(args.received, width, height)
    rows = []
    for y in range(height):
        start = y * width
        a, b = reference[start:start + width], received[start:start + width]
        candidates = sorted((sum(abs(a[x] - b[(x + shift) % width]) for x in range(width)), shift)
                            for shift in range(-args.max_shift, args.max_shift + 1))
        best, shift = candidates[0]
        next_best = candidates[1][0] if len(candidates) > 1 else best
        rows.append({'row': y, 'shift': shift, 'mae': round(best / width, 6),
                     'margin': round((next_best - best) / width, 6)})
    print(json.dumps({'width': width, 'height': height, 'rows': rows}, sort_keys=True))


if __name__ == '__main__': main()
