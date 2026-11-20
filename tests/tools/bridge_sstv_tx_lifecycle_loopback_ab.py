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
module = root / 'src/bridge/DecodiumBridgeSstvTxLifecycle.cpp'
original_main, original_module = main.read_text(), module.read_text()
fixture = (root / 'tests/bridge_refactor_fixtures/sstv_tx_lifecycle_before.inc').read_text()
assert original_module.endswith(fixture)
head=original_module[:-len(fixture)]
play='    if (snapshot.audioAttached && m_sstvTxAudioDevice && m_soundOutput) {\n';release='    m_sstvOwnsBridgeTx = false;\n';assert fixture.count(play)==1 and fixture.count(release)==1
instrumented=fixture.replace(play,play+'        qWarning("[LAB-TX-PLAYBACK-TRACKED]");\n',1).replace(release,release+'    qWarning("[LAB-TX-OWNERSHIP-RELEASED]");\n',1)
anchor = 'void DecodiumBridge::initialiseSstvRuntime()'
assert original_main.count(anchor) == 1
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
env = os.environ.copy()
env.update(DECODIUM_LAB_AVT_NO_CAPTURE='1', DECODIUM_FORCE_STANDALONE_UDP='1')
failures = []
try:
    for label in ('before', 'after'):
        if label == 'before':
            main.write_text(original_main.replace(anchor, instrumented + anchor, 1))
            module.write_text(head)
        else:
            main.write_text(original_main)
            module.write_text(head+instrumented)
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
            if '[LAB-TX-PLAYBACK-TRACKED]' not in tx or '[LAB-TX-OWNERSHIP-RELEASED]' not in tx:
                raise RuntimeError('real playback tracking or ownership release was not observed: '+tx[-5000:])
            if not reports or not reports[-1].get('success') or reports[-1].get('state') != 'Completed':
                raise RuntimeError('TX completion missing: ' + tx[-3000:])
            if not diagnostics or not diagnostics[-1].get('imageAvailable') or diagnostics[-1].get('imageMode') != 'hffax-experimental':
                raise RuntimeError('no Radiofax image observed on virtual RX')
            if result.returncode != 0:
                failures.append(label)
            print(label, 'tx=', json.dumps(reports[-1], sort_keys=True), 'rxCoverage=', diagnostics[-1].get('imageCoverage'), 'rxComplete=', diagnostics[-1].get('imageComplete'), 'endToEndPassed=', result.returncode == 0, flush=True)
            evidence=root/'tests/bridge_refactor_evidence/sstv_tx_lifecycle';evidence.mkdir(parents=True,exist_ok=True)
            (evidence/(label+'.json')).write_text(json.dumps({'tx':reports[-1],'coverage':diagnostics[-1].get('imageCoverage'),'complete':diagnostics[-1].get('imageComplete'),'playback_tracked':True,'ownership_released':True,'end_to_end':result.returncode==0},indent=2)+'\n')
            for text in (tx,rx):
                match=re.search(r'Application identity: (Decodium - RFAX(?:tx|rx)[0-9a-f]{8} - test)',text)
                if match:
                    for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
                        target=parent/match.group(1)
                        if target.is_dir():shutil.rmtree(target)
        finally:
            shutil.rmtree(artifacts)
    if failures:
        raise RuntimeError('End-to-end image check failed: ' + ', '.join(failures))

finally:
    main.write_text(original_main)
    module.write_text(original_module)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
