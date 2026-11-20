#!/usr/bin/env python3
"""Check learned-call persistence after interrupting an active FST4W worker.

The input is little-endian 16-bit mono 12 kHz PCM containing K1ABC FN42 33.
Delay may need adjusting on machines with different decode speeds.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--pcm', type=Path, required=True)
p.add_argument('--probe', type=Path, default=root / 'build-baseline/tests/fst4_performance_probe')
p.add_argument('--delay-ms', type=int, default=10000)
p.add_argument('--output', type=Path)
a = p.parse_args()
env = dict(os.environ, DECODIUM_FST4_SHUTDOWN_DELAY_MS=str(a.delay_ms),
           DECODIUM_FST4_LAB_TOLERANCE='20', OMP_NUM_THREADS='2')
result = subprocess.run([str(a.probe.resolve()), str(a.pcm.resolve()), '120', '--shutdown-fst4w'],
                        env=env, capture_output=True, text=True, check=True, timeout=150)
report = json.loads(result.stdout)
if report['completedBeforeShutdown'] or report['rows'] or 'K1ABC FN42' not in report['savedCalls'].splitlines():
    raise SystemExit('The worker may have stopped too early/late or failed to save the expected call: ' + json.dumps(report))
normal = subprocess.run([str(a.probe.resolve()), str(a.pcm.resolve()), '120', '--complete-fst4w'],
                        env=env, capture_output=True, text=True, check=True, timeout=150)
complete = json.loads(normal.stdout)
missing = set(report['savedCalls'].splitlines()) - set(complete['savedCalls'].splitlines())
if not complete['completedBeforeShutdown'] or not complete['rows'] or missing:
    raise SystemExit('Completed decode did not preserve interrupted-run calls: ' + json.dumps({'complete': complete, 'missing': sorted(missing)}))
comparison = {'interrupted': report, 'completed': complete}
if a.output:
    a.output.write_text(json.dumps(comparison, indent=2) + '\n')
print(json.dumps(comparison))
