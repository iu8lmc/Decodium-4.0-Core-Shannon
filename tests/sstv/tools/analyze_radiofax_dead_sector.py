#!/usr/bin/env python3
"""Post-hoc, full-width diagnostic for a fax line with a 4.5% dead sector."""
import argparse
import json

from analyze_radiofax_2d_alignment import dimensions, gray, measure


def compare(reference, received, width, height, received_width, row, start):
    """Sample active-sector pixel centres on the continuous received raster."""
    active = received_width - received_width * 45 // 1000
    if (width <= 0 or height <= 0 or received_width <= 0 or row < 0
            or not 0 <= start < received_width or len(reference) != width * height):
        raise ValueError('invalid raster or offset')
    indexes = [((row + y) * received_width + start
                + (2 * x + 1) * active // (2 * width))
               for y in range(height) for x in range(width)]
    if indexes[-1] >= len(received):
        raise ValueError('insufficient received raster')
    return measure([abs(reference[i] - received[index]) for i, index in enumerate(indexes)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference')
    parser.add_argument('received')
    parser.add_argument('--row', type=int, required=True)
    parser.add_argument('--start', type=int, required=True)
    args = parser.parse_args()
    width, height = dimensions(args.reference)
    received_width, received_height = dimensions(args.received)
    reference = gray(args.reference, width, height)
    received = gray(args.received, received_width, received_height)
    print(json.dumps({'reference': args.reference, 'received': args.received,
                      'row': args.row, 'start': args.start,
                      'dead_sector_fraction': 0.045,
                      'posthoc_full_width': compare(reference, received, width, height,
                                                   received_width, args.row, args.start)},
                     sort_keys=True))


if __name__ == '__main__':
    main()
