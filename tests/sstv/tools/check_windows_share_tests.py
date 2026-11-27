#!/usr/bin/env python3
"""Check that a CTest JSON inventory contains the Windows sharing gates."""
import json
import sys

BASE = 'test_sstv_share_windows_'
REQUIRED = {BASE + 'transfer', BASE + 'tamper'} | {
    BASE + case + '_' + variant
    for case in ('expiry', 'expiry_awaiting', 'expiry_partial', 'expiry_restart',
                 'payload_tamper', 'payload_swap', 'replay', 'manifest_tamper', 'downgrade')
    for variant in ('a', 'b')
}


def check(data):
    if not isinstance(data, dict) or not isinstance(data.get('tests'), list):
        raise ValueError('invalid CTest JSON inventory')
    names = [test.get('name') for test in data['tests'] if isinstance(test, dict)]
    if len(names) != len(data['tests']) or not all(isinstance(name, str) for name in names):
        raise ValueError('invalid CTest test entry')
    missing = sorted(REQUIRED - set(names))
    if missing:
        raise ValueError('missing required Windows sharing tests: ' + ', '.join(missing))
    return len(REQUIRED)


def main():
    try:
        count = check(json.load(sys.stdin))
    except (ValueError, json.JSONDecodeError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
    print(f'Windows sharing inventory includes all {count} required tests')
    return 0


if __name__ == '__main__':
    sys.exit(main())
