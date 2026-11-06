#!/usr/bin/env python3
"""Compare a measured tone-scale hypothesis; this cannot prove clock drift."""
import argparse
import json
import math


def analyze(period, old_black, old_white, new_black, new_white):
    values = (period, old_black, old_white, new_black, new_white)
    if (not all(math.isfinite(value) and value > 0 for value in values)
            or old_white <= old_black or new_white <= new_black):
        raise ValueError('invalid period or tone levels')
    black_ratio = new_black / old_black
    white_ratio = new_white / old_white
    ratio = (black_ratio + white_ratio) / 2
    return {'black_ratio': black_ratio, 'white_ratio': white_ratio,
            'ratio_disagreement': abs(black_ratio - white_ratio),
            'implied_period': period / ratio,
            'relative_frequency_ppm': (ratio - 1) * 1e6,
            'caveat': 'frequency scaling does not establish line-clock scaling'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('period', 'old_black', 'old_white', 'new_black', 'new_white'):
        parser.add_argument(name, type=float)
    args = parser.parse_args()
    try:
        result = analyze(args.period, args.old_black, args.old_white,
                         args.new_black, args.new_white)
    except ValueError as error:
        parser.error(str(error))
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
