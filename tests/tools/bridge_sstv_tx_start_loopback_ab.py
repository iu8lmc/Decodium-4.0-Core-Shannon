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
module = root / 'src/bridge/DecodiumBridgeSstvTxStart.cpp'
original_main, original_module = main.read_text(), module.read_text()
fixture = (root / 'tests/bridge_refactor_fixtures/sstv_tx_start_before.inc').read_text()
assert original_module.endswith(fixture.replace('sstvOutputMatchesConfiguredSelection(', 'decodiumSstvOutputMatchesConfiguredSelection('))
anchor = 'void DecodiumBridge::initialiseSstvRuntime()'
assert original_main.count(anchor) == 1
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
env = os.environ.copy()
env.update(DECODIUM_FORCE_STANDALONE_UDP='1')
env.pop('DECODIUM_LAB_AVT_NO_CAPTURE', None)
failures = []
try:
    for label in ('before', 'after'):
        if label == 'before':
            main.write_text(original_main.replace(anchor, original_module[:original_module.index('bool DecodiumBridge::startSstvTx(')] + fixture.replace('sstvOutputMatchesConfiguredSelection(', 'decodiumSstvOutputMatchesConfiguredSelection(') + anchor, 1))
            module.write_text('#include "DecodiumBridge.h"\n')
        else:
            main.write_text(original_main)
            module.write_text(original_module)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True)
        result = subprocess.run(['sandbox-exec', '-p', policy, sys.executable, 'tests/sstv/tools/run_avt_two_app_lab.py', 'robot-c12', 'a'], cwd=root, env=env, capture_output=True, text=True, timeout=100)
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
            if not diagnostics or not diagnostics[-1].get('imageAvailable') or diagnostics[-1].get('imageMode') != 'robot-c12':
                raise RuntimeError('no Robot 12 image observed on virtual RX')
            if result.returncode != 0:
                failures.append(label)
            print(label, 'tx=', json.dumps(reports[-1], sort_keys=True), 'rxCoverage=', diagnostics[-1].get('imageCoverage'), 'rxComplete=', diagnostics[-1].get('imageComplete'), 'endToEndPassed=', result.returncode == 0, flush=True)
        finally:
            # Remove only the two unique test profiles named by this run.
            for log_name in ('tx.log', 'rx.log'):
                log_path = artifacts / log_name
                if not log_path.is_file():
                    continue
                identity = re.search(
                    r'Application identity: (Decodium - AVT(?:rx|tx)[0-9a-f]{8} - test)',
                    log_path.read_text(errors='replace'))
                if identity:
                    for parent in (Path.home() / '.qttest/Library/Application Support/IU8LMC',
                                   Path.home() / '.qttest/Library/Caches/IU8LMC'):
                        profile = parent / identity.group(1)
                        if profile.is_dir():
                            shutil.rmtree(profile)
            shutil.rmtree(artifacts)
    if failures:
        raise RuntimeError('End-to-end image check failed: ' + ', '.join(failures))

finally:
    main.write_text(original_main)
    module.write_text(original_module)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True)
