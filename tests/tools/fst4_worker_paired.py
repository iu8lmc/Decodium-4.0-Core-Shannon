#!/usr/bin/env python3
"""Compare two native FST4 workers on the same PCM in alternating order."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--before', type=Path, required=True)
parser.add_argument('--after', type=Path, required=True)
parser.add_argument('--pcm', type=Path, required=True)
parser.add_argument('--period', type=int, required=True)
parser.add_argument('--fst4w', action='store_true')
parser.add_argument('--tolerance', type=int, default=20)
parser.add_argument('--pairs', type=int, choices=(1, 2), default=2)
parser.add_argument('--timeout', type=int, default=180)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
env = {**os.environ, 'DECODIUM_FST4_LAB_RUNS': '1', 'OMP_NUM_THREADS': '2'}
if not 1 <= args.tolerance <= 500:
    raise ValueError('tolerance must be between 1 and 500 Hz')
env['DECODIUM_FST4_LAB_TOLERANCE'] = str(args.tolerance)
observations = []
order = [('before', args.before), ('after', args.after)]
if args.pairs == 2:
    order += [('after', args.after), ('before', args.before)]
for name, binary in order:
    command = [str(binary.resolve()), str(args.pcm.resolve()), str(args.period)]
    if args.fst4w:
        command.append('--fst4w')
    result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=args.timeout, check=True)
    runs = json.loads(result.stdout)['runs']
    if len(runs) != 1:
        raise RuntimeError('expected exactly one run')
    observations.append({'version': name, **runs[0]})
    print(json.dumps({'version': name, 'elapsedMs': runs[0]['elapsedMs'], 'rows': len(runs[0]['rows'])}), flush=True)
report = {'pcmSha256': hashlib.sha256(args.pcm.read_bytes()).hexdigest(),
          'period': args.period, 'fst4w': args.fst4w, 'toleranceHz': args.tolerance,
          'observations': observations}
args.output.write_text(json.dumps(report, indent=2) + '\n')
if any(item['rows'] != observations[0]['rows'] for item in observations):
    raise SystemExit('decoded rows differ; inspect ' + str(args.output))
print(json.dumps({'rows': len(observations[0]['rows']),
                  'timings': [(v['version'], round(v['elapsedMs'], 1)) for v in observations]}))
