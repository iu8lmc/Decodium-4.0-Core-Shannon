#!/usr/bin/env python3
"""Check expected-message coverage for a recorded FT2 virtual-audio soak.

Extra/uncertain messages remain visible. Coverage is not full-row equivalence,
RF proof or an assertion that memory cannot leak during a longer session.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import statistics


def summarize(directory, expected):
    r = json.loads((directory / 'result.json').read_text())
    if not r['valid'] or r['mode'] != 'FT2' or r['period_seconds'] != 3.75:
        raise ValueError('Requires a valid FT2 runtime report')
    start = r['audio_start_epoch_s'] - r['audio_start_offset_in_slot_s']
    slots = Counter()
    extras = []
    for row in r['messages']:
        if row['message'] != expected:
            extras.append(row)
            continue
        relative = row['ts_utc'] / 1000 - start
        index = round(relative / 3.75)
        # Stored UTC is integer-second precision; allow its quantisation only.
        if abs(relative - index * 3.75) > 1:
            raise ValueError('Expected-message UTC is outside slot quantisation tolerance')
        slots[index] += 1
    required = Counter({i: 1 for i in range(r['injected_slots'])})
    windows = []
    maximum = max(s['seconds'] for s in r['samples'])
    for begin in range(30, int(maximum), 60):
        values = [s['rss_kib'] / 1024 for s in r['samples'] if begin <= s['seconds'] < begin + 60]
        if values:
            windows.append({'startSeconds': begin, 'medianRssMiB': statistics.median(values)})
    log = (directory / 'rx.diagnostic.log').read_text()
    return {'expectedCoverageExact': slots == required, 'expectedSlots': r['injected_slots'],
            'observedSlotCounts': dict(sorted(slots.items())), 'extraRows': extras,
            'udpFailure': r['udp_failure'], 'binarySha256': r['binary_sha256'],
            'rssMinuteWindows': windows,
            'watchdogStallReports': [line for line in log.splitlines() if 'event_loop_stalls samples=' in line],
            'scope': 'Expected-message slot coverage and sampled RSS only; extra rows retained, not full A/B equivalence or RF proof.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path)
    p.add_argument('--expected', default='CQ K1ABC FN42')
    args = p.parse_args()
    result = summarize(args.directory, args.expected)
    (args.directory / 'acceptance.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0 if result['expectedCoverageExact'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
