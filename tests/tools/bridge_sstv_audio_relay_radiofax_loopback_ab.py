#!/usr/bin/env python3
"""Compare two-session SSTV TX lifecycle over isolated BlackHole audio."""
import os
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
main = root / 'src/bridge/DecodiumBridgeSstv.cpp'
module = root / 'src/bridge/DecodiumSstvAudioRelay.h'
original_main, original_module = main.read_text(), module.read_text()
fixture = (root / 'tests/bridge_refactor_fixtures/sstv_audio_relay_before.inc').read_text()
assert original_module.endswith(fixture)
anchor = '#include "DecodiumSstvAudioRelay.h"\n'
assert original_main.count(anchor) == 1
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
env = os.environ.copy()
env.update(DECODIUM_LAB_AVT_NO_CAPTURE='1', DECODIUM_FORCE_STANDALONE_UDP='1')
failures = []
try:
    for label in ('before', 'after'):
        if label == 'before':
            main.write_text(original_main.replace(anchor, fixture, 1))
        else:
            main.write_text(original_main)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        result = subprocess.run(['sandbox-exec', '-p', policy, sys.executable, 'tests/sstv/tools/run_radiofax_centered_two_app_lab.py', 'a', '240'], cwd=root, env=env, capture_output=True, text=True, timeout=180)
        match = re.search(r'artifacts\s+(/\S+)', result.stdout + result.stderr)
        if not match:
            raise RuntimeError('test did not report its artifact directory: ' + result.stderr[-2000:])
        artifacts = Path(match.group(1))
        try:
            tx = (artifacts / 'tx.log').read_text(errors='replace')
            rx = (artifacts / 'rx.log').read_text(errors='replace')
            reports = [json.loads(line.split('tx.completed ', 1)[1]) for line in tx.splitlines() if 'tx.completed ' in line]
            diagnostics = [json.loads(line.split(' RX diagnostics ', 1)[1]) for line in rx.splitlines() if '[LAB][SSTV] RX diagnostics ' in line]
            if not reports or not reports[-1].get('success') or reports[-1].get('state') != 'Completed':
                raise RuntimeError('TX completion missing: ' + tx[-3000:])
            if not diagnostics or not diagnostics[-1].get('imageAvailable') or diagnostics[-1].get('imageMode') != 'hffax-experimental':
                raise RuntimeError('no Radiofax image observed on virtual RX')
            if result.returncode != 0:
                failures.append(label)
            print(label, 'tx=', json.dumps(reports[-1], sort_keys=True), 'rxCoverage=', diagnostics[-1].get('imageCoverage'), 'rxComplete=', diagnostics[-1].get('imageComplete'), 'endToEndPassed=', result.returncode == 0, flush=True)
        finally:
            shutil.rmtree(artifacts)
    if failures:
        raise RuntimeError('End-to-end image check failed: ' + ', '.join(failures))

finally:
    main.write_text(original_main)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
