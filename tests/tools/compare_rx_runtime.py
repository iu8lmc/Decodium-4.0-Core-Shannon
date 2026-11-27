#!/usr/bin/env python3
"""Compare complete per-slot message multisets from paired RX runtime reports."""
import argparse
from collections import Counter
import json
import math
from pathlib import Path


def decode_counts(report):
    mode = report.get('mode', 'FT8')  # Original reports only supported FT8.
    periods = {'FT8': 15, 'FT4': 7.5, 'FT2': 3.75}
    if mode not in periods or report.get('period_seconds', periods[mode]) != periods[mode]:
        raise ValueError('Unsupported mode or inconsistent period')
    period_ms = periods[mode] * 1000
    start_ms = math.floor(report['audio_start_epoch_s'] / periods[mode]) * period_ms
    result = Counter()
    for row in report['messages']:
        relative = row['ts_utc'] - start_ms
        slot = round(relative / period_ms)
        if row['mode'] != mode or abs(relative - slot * period_ms) > 1000:
            raise ValueError('Decode mode or UTC does not match the injected slots')
        if not 0 <= slot < report['injected_slots']:
            raise ValueError(f'Decode outside the injected slots: {slot}, {row["message"]}')
        result[(slot, row['mode'], row['message'].strip())] += 1
    return result


def compare(before, after):
    for name, report in [('before', before), ('after', after)]:
        if not report['valid'] or report['udp_failure'] or report['timed_out']:
            raise ValueError(f'Invalid {name} runtime report')
        if report['stack_sampling']:
            raise ValueError('Do not use stack-sampled runs as paired performance evidence')
    for field in ('fixture_sha256', 'playback_resampler', 'settings', 'injected_slots'):
        if before[field] != after[field]:
            raise ValueError(f'Incomparable input/configuration: {field}')
    for field in ('requested_profile', 'credential_isolation'):
        if before.get(field) != after.get(field):
            raise ValueError(f'Incomparable input/configuration: {field}')
    if before.get('mode', 'FT8') != after.get('mode', 'FT8'):
        raise ValueError('Incomparable input/configuration: mode')
    a, b = decode_counts(before), decode_counts(after)

    def serialise(counter):
        return [{'slot': key[0], 'mode': key[1], 'message': key[2], 'count': count}
                for key, count in sorted(counter.items())]

    missing, extra = a - b, b - a
    return {'no_baseline_decode_loss': not missing, 'identical_message_multisets': a == b,
            'before_rows': sum(a.values()), 'after_rows': sum(b.values()),
            'missing': serialise(missing), 'extra': serialise(extra),
            'scope': 'Per-slot message identities and multiplicities; not an RF or universal no-loss guarantee'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        result = compare(json.loads(args.before.read_text()), json.loads(args.after.read_text()))
    except (ValueError, KeyError) as error:
        parser.error(str(error))
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(text)
    print(text, end='')
    return 0 if result['no_baseline_decode_loss'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
