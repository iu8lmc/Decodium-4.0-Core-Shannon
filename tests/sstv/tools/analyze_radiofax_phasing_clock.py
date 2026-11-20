#!/usr/bin/env python3
"""Estimate the captured phasing period from 1500-to-2300 Hz crossings.

Requires a user-selected window containing only the alternating phasing
pattern. The estimate describes the captured clock, not the sender's clock.
"""
import argparse
from array import array
import json
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--window', nargs=2, type=float, required=True)
    parser.add_argument('--rate', type=int, default=48000)
    parser.add_argument('--lpm', type=int, default=120, choices=(60, 90, 120, 240))
    args = parser.parse_args()
    start, end = args.window
    if not (8000 <= args.rate <= 192000 and 0 <= start < end <= 3600 and end - start <= 30):
        parser.error('invalid sample rate or window')
    first = int(start * args.rate)
    count = int(end * args.rate) - first
    values = array('f')
    try:
        with args.capture.open('rb') as source:
            source.seek(first * 8)
            values.frombytes(source.read(count * 8))
    except OSError as error:
        parser.error(str(error))
    if sys.byteorder != 'little':
        values.byteswap()
    if len(values) != count * 2:
        parser.error('capture is shorter than the selected window')
    samples = values[::2]
    crossings = []
    for i in range(1, len(samples)):
        left, right = samples[i - 1], samples[i]
        if left < 0 <= right:
            crossings.append((first + i - 1 - left / (right - left)) / args.rate)
    frequencies = [(crossings[i], 1.0 / (crossings[i] - crossings[i - 1]))
                   for i in range(1, len(crossings))]
    edges = []
    for i in range(3, len(frequencies) - 3):
        if (all(1200 < v[1] < 1800 for v in frequencies[i - 3:i])
                and all(2000 < v[1] < 2600 for v in frequencies[i:i + 3])):
            edges.append((frequencies[i - 1][0] + frequencies[i][0]) / 2)
    if len(edges) < 5:
        parser.error('fewer than five phasing edges')
    nominal = 60.0 / args.lpm
    periods = 0
    for previous, current in zip(edges, edges[1:]):
        gap = current - previous
        lines = max(1, round(gap / nominal))
        if abs(gap / lines - nominal) > nominal * .01:
            parser.error('irregular phasing edges; window may contain image data')
        periods += lines
    measured = (edges[-1] - edges[0]) / periods
    print(json.dumps({'edge_count': len(edges), 'period_count': periods,
                      'first_edge_s': edges[0], 'last_edge_s': edges[-1],
                      'line_period_s': measured, 'nominal_line_period_s': nominal,
                      'relative_ppm': (measured / nominal - 1) * 1e6,
                      'projected_120_line_duration_s': measured * 120}, sort_keys=True))


if __name__ == '__main__':
    main()
