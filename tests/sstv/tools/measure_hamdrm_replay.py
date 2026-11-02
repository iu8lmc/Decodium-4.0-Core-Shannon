#!/usr/bin/env python3
"""Measure a local HAMDRM replay process while its existing lab verifies output."""
import argparse
import json
import os
import signal
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('mode', choices=('standard', 'adaptive'))
parser.add_argument('wav', type=Path)
parser.add_argument('reference', type=Path)
parser.add_argument('--profile', required=True)
parser.add_argument('--duration-ms', type=int, default=150000)
args = parser.parse_args()
if not 10000 <= args.duration_ms <= 600000: parser.error('invalid duration')
for path in (args.wav, args.reference): path.resolve(strict=True)
env = os.environ.copy()
env['DECODIUM_HAMDRM_LAB_PROFILE'] = args.profile
env['DECODIUM_HAMDRM_LAB_DURATION_MS'] = str(args.duration_ms)
if args.mode == 'adaptive': env['DECODIUM_LAB_HAMDRM_USER_TIMING'] = '1'
else: env.pop('DECODIUM_LAB_HAMDRM_USER_TIMING', None)
root = Path(tempfile.mkdtemp(prefix='decodium-hamdrm-measure-'+args.mode+'-'))
samples = []
start = time.monotonic()
with (root/'replay.log').open('wb') as log:
    proc = subprocess.Popen([sys.executable, 'tests/sstv/tools/run_qsstv_exp_drm_app_lab.py',
                             str(args.wav), str(args.reference)], stdout=log, stderr=subprocess.STDOUT,
                            env=env, start_new_session=True)
    try:
        while proc.poll() is None:
            listing = subprocess.run(['ps', '-axo', 'pid=,ppid=,pcpu=,rss=,comm='],
                                     capture_output=True, text=True, check=True)
            for line in listing.stdout.splitlines():
                fields = line.split(None, 4)
                if len(fields) == 5 and int(fields[1]) == proc.pid and Path(fields[4]).name == 'decodium':
                    samples.append({'seconds': round(time.monotonic()-start, 2),
                                    'cpu_percent': float(fields[2]), 'rss_kib': int(fields[3])})
            time.sleep(2)
    finally:
        if proc.poll() is None:
            os.killpg(proc.pid, signal.SIGTERM)
            try: proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait(timeout=5)
if not samples: raise RuntimeError('no Decodium process samples; artifacts '+str(root))
cpu = [sample['cpu_percent'] for sample in samples]
rss = [sample['rss_kib'] for sample in samples]
report = {'mode': args.mode, 'samples': len(samples), 'exit_code': proc.returncode,
          'cpu_median_percent': statistics.median(cpu), 'cpu_max_percent': max(cpu),
          'rss_max_kib': max(rss), 'wall_seconds': round(time.monotonic()-start, 2),
          'note': 'process samples include startup and shutdown; CPU is percent of one core',
          'artifacts': str(root)}
(root/'samples.json').write_text(json.dumps({'report': report, 'samples': samples}, indent=2)+'\n')
print(json.dumps(report), flush=True)
print((root/'replay.log').read_text(errors='replace')[-2000:], flush=True)
sys.exit(proc.returncode)
