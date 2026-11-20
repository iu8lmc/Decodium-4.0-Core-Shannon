#!/usr/bin/env python3
"""Report duplicate visible message/frequency rows per period in live probe logs."""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('logs',nargs='+',type=Path)
args=p.parse_args()
for path in args.logs:
    max_duplicates=defaultdict(int);period_rows=defaultdict(set);probes=0
    for line in path.read_text(errors='replace').splitlines():
        if '[LIVE-PROBE] ' not in line:continue
        state=json.loads(line.split('[LIVE-PROBE] ',1)[1]);probes+=1
        grouped=defaultdict(list)
        for row in state['rows']:
            grouped[row['time']].append((row['message'],row['freq']))
        for period,rows in grouped.items():
            counts=Counter(rows);period_rows[period].update(counts)
            max_duplicates[period]=max(max_duplicates[period],sum(n-1 for n in counts.values() if n>1))
    if not probes or not period_rows:raise RuntimeError('no populated probes: '+str(path))
    print(json.dumps({'log':str(path),'probes':probes,'periods':[
        {'time':t,'distinctMessageFrequencies':len(period_rows[t]),'maxDuplicateRows':max_duplicates[t]}
        for t in sorted(period_rows)]},sort_keys=True))
