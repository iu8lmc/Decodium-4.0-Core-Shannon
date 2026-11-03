#!/usr/bin/env python3
"""Measure a local HAMDRM replay process while its existing lab verifies output."""
import argparse
import json
import os
import re
import signal
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('mode', choices=('standard', 'adaptive', 'temporal'))
parser.add_argument('wav', type=Path)
parser.add_argument('reference', type=Path)
parser.add_argument('--profile', required=True)
parser.add_argument('--duration-ms', type=int, default=150000)
parser.add_argument('--disable-auto-early', action='store_true',
                    help='compare the standard receiver without the automatic early candidate')
args = parser.parse_args()
if not 10000 <= args.duration_ms <= 600000: parser.error('invalid duration')
if args.disable_auto_early and args.mode != 'standard':
    parser.error('--disable-auto-early requires standard mode')
for path in (args.wav, args.reference): path.resolve(strict=True)
env = os.environ.copy()
env['DECODIUM_HAMDRM_LAB_PROFILE'] = args.profile
env['DECODIUM_HAMDRM_LAB_DURATION_MS'] = str(args.duration_ms)
for name in tuple(env):
    if (name.startswith('DECODIUM_LAB_HAMDRM_')
        or name.startswith('DECODIUM_LAB_EXPECT_')
        or name in ('DECODIUM_LAB_AUTO_EARLY_FRAME_LIMIT', 'DECODIUM_LAB_TEMPORAL_FRAME_LIMIT',
                    'DECODIUM_HAMDRM_RX_TRACE')):
        env.pop(name, None)
if args.disable_auto_early: env['DECODIUM_LAB_HAMDRM_DISABLE_AUTO_EARLY'] = '1'
if args.mode == 'adaptive': env['DECODIUM_LAB_HAMDRM_USER_TIMING'] = '1'
if args.mode == 'temporal': env['DECODIUM_LAB_HAMDRM_TEMPORAL_PILOTS'] = '1'
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
replay_log = (root/'replay.log').read_text(errors='replace')
if proc.returncode == 0 and args.mode == 'standard':
    match = re.search(r'artifacts (\S+)', replay_log)
    if not match: raise RuntimeError('missing replay artifacts; artifacts '+str(root))
    rx_log = (Path(match.group(1))/'rx.log').read_text(errors='replace')
    enabled = '[HAMDRM] automatic adaptive early receiver enabled' in rx_log
    if enabled == args.disable_auto_early:
        raise RuntimeError('automatic receiver selection mismatch; artifacts '+str(root))
    temporal_enabled = '[HAMDRM] automatic temporal pilot receiver enabled' in rx_log
    if temporal_enabled == args.disable_auto_early:
        raise RuntimeError('automatic temporal receiver selection mismatch; artifacts '+str(root))
cpu = [sample['cpu_percent'] for sample in samples]
rss = [sample['rss_kib'] for sample in samples]
report = {'mode': args.mode, 'samples': len(samples), 'exit_code': proc.returncode,
          'automatic_early_disabled': args.disable_auto_early,
          'cpu_median_percent': statistics.median(cpu), 'cpu_max_percent': max(cpu),
          'rss_max_kib': max(rss), 'wall_seconds': round(time.monotonic()-start, 2),
          'note': 'process samples include startup and shutdown; CPU is percent of one core',
          'artifacts': str(root)}
(root/'samples.json').write_text(json.dumps({'report': report, 'samples': samples}, indent=2)+'\n')
print(json.dumps(report), flush=True)
print(replay_log[-2000:], flush=True)
sys.exit(proc.returncode)
