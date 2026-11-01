#!/usr/bin/env python3
"""Measure selected tone transitions in a little-endian float-stereo capture.

Windows and thresholds must be selected for known signals.  This diagnostic
does not detect arbitrary Radiofax image boundaries.
"""
import argparse
from array import array
import json
from pathlib import Path
import sys


def transition(path, rate, start, end, threshold):
    if not 0 <= start < end <= 3600 or end - start > 5:
        raise ValueError('invalid transition window')
    first = int(start * rate)
    count = int(end * rate) - first
    values = array('f')
    with path.open('rb') as source:
        source.seek(first * 8)
        values.frombytes(source.read(count * 8))
    if sys.byteorder != 'little':
        values.byteswap()
    if len(values) != count * 2:
        raise ValueError('capture is shorter than the selected window')
    samples = values[::2]
    crossings = []
    for i in range(1, len(samples)):
        left, right = samples[i - 1], samples[i]
        if left < 0 <= right:
            crossings.append((first + i - 1 - left / (right - left)) / rate)
    frequencies = [(crossings[i], 1.0 / (crossings[i] - crossings[i - 1]))
                   for i in range(1, len(crossings))]
    candidates = []
    for i in range(3, len(frequencies) - 3):
        if (all(v[1] > threshold for v in frequencies[i - 3:i])
                and all(v[1] < threshold for v in frequencies[i:i + 3])):
            candidates.append((frequencies[i - 1][0], frequencies[i][0]))
    if len(candidates) != 1:
        raise ValueError('expected one stable downward transition, found %d' % len(candidates))
    before, after = candidates[0]
    return {'before_s': before, 'after_s': after, 'estimate_s': (before + after) / 2}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--rate', type=int, default=48000)
    parser.add_argument('--start-window', nargs=2, type=float, required=True)
    parser.add_argument('--end-window', nargs=2, type=float, required=True)
    parser.add_argument('--start-threshold', type=float, default=1900)
    parser.add_argument('--end-threshold', type=float, default=900)
    args = parser.parse_args()
    if not 8000 <= args.rate <= 192000 or not (0 < args.start_threshold < args.rate / 2
                                                and 0 < args.end_threshold < args.rate / 2):
        parser.error('invalid sample rate or threshold')
    try:
        first = transition(args.capture, args.rate, *args.start_window, args.start_threshold)
        last = transition(args.capture, args.rate, *args.end_window, args.end_threshold)
        if last['before_s'] <= first['after_s']:
            raise ValueError('end transition precedes start')
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps({'start': first, 'end': last,
                      'duration_estimate_s': last['estimate_s'] - first['estimate_s'],
                      'duration_min_s': last['before_s'] - first['after_s'],
                      'duration_max_s': last['after_s'] - first['before_s']}, sort_keys=True))


if __name__ == '__main__':
    main()
