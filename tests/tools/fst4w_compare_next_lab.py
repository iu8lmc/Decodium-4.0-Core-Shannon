#!/usr/bin/env python3
"""Compare complete next-mode decode rows captured by the FST4W switch lab."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

MARKER = '[FST4W-NEXT-ROWS] '
SESSION_TIMES = {'formattedTime', 'time', 'timestamp'}


def load_rows(path):
    contents = path.read_bytes()
    hits = [json.loads(line.split(MARKER, 1)[1])
            for line in contents.decode('utf-8', errors='replace').splitlines()
            if MARKER in line]
    if len(hits) != 1 or not isinstance(hits[0], list) or not hits[0]:
        raise ValueError(f'{path}: expected exactly one nonempty row list')
    rows = []
    for row in hits[0]:
        if not isinstance(row, dict) or not row.get('message') or not row.get('freq'):
            raise ValueError(f'{path}: invalid decode row: {row!r}')
        rows.append({key: value for key, value in row.items() if key not in SESSION_TIMES})
    return hashlib.sha256(contents).hexdigest(), rows


def compare(before, after):
    def counts(rows):
        return Counter(json.dumps(row, sort_keys=True, ensure_ascii=False) for row in rows)
    old, new = counts(before), counts(after)
    return ([json.loads(row) for row in (old - new).elements()],
            [json.loads(row) for row in (new - old).elements()])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before-log', type=Path, required=True)
    parser.add_argument('--after-log', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    before_hash, before = load_rows(args.before_log)
    after_hash, after = load_rows(args.after_log)
    missing, added = compare(before, after)
    report = {'before': {'sha256': before_hash, 'rows': before},
              'after': {'sha256': after_hash, 'rows': after},
              'missing': missing, 'added': added}
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
    print(json.dumps({'before': len(before), 'after': len(after),
                      'missing': len(missing), 'added': len(added)}))
    if missing or added:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
