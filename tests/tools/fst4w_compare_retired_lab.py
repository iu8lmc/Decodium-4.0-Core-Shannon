#!/usr/bin/env python3
"""Compare the complete retired FST4W rows captured in two lab app logs."""
import argparse
import hashlib
import json
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--before-log',type=Path,required=True)
p.add_argument('--after-log',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--expected',default='K1ABC FN42 33')
a=p.parse_args()

def read(path):
    contents=path.read_bytes();lines=contents.decode('utf-8',errors='replace').splitlines()
    marker='[FST4W-RETIRED-LAB] '
    hits=[json.loads(line.split(marker,1)[1]) for line in lines if marker in line]
    if len(hits)!=1: raise RuntimeError(f'{path}: expected one retired result, got {len(hits)}')
    value=hits[0]
    # UTC represents the slot under test and can differ between sessions;
    # retain the remainder of each row byte-for-byte and preserve order.
    rows=[]
    for row in value['rows']:
        pieces=row.strip().split(maxsplit=1)
        if len(pieces)!=2 or not pieces[0].isdigit(): raise RuntimeError('invalid row: '+repr(row))
        rows.append(pieces[1])
    if not any(a.expected in row for row in rows): raise RuntimeError('expected message absent in '+str(path))
    return {'sha256':hashlib.sha256(contents).hexdigest(),'calls':value['calls'],'rows':rows}

before,after=read(a.before_log),read(a.after_log)
missing={'rowsDiffer':before['rows']!=after['rows'],'callsDiffer':before['calls']!=after['calls']}
report={'before':before,'after':after,'differences':missing}
a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'rows':len(before['rows']),'calls':len(before['calls'].splitlines()),**missing}))
if any(missing.values()): raise SystemExit(1)
