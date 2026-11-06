#!/usr/bin/env python3
"""Run isolated full Decodium seed/restart checks for a saved HAMDRM partial."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile

if len(sys.argv) not in (2, 3) or sys.argv[1] not in ('a', 'b'):
    raise SystemExit('usage: run_hamdrm_partial_restart_app_lab.py a|b [decodium-binary]')
variant = sys.argv[1]
repo = Path(__file__).resolve().parents[3]
app = (Path(sys.argv[2]) if len(sys.argv) == 3 else repo / 'build/decodium').resolve(strict=True)
source = repo / 'tests/sstv/fixtures/easypal-2014-live' / ('reverse-' + variant + '.jpg')
profiles = ('hamdrm-A-2300-high-4-QAM-long', 'hamdrm-A-2300-high-4-QAM-short')
first = 0 if variant == 'a' else 1
rig = 'PartialRestart' + variant + secrets.token_hex(5)
with tempfile.TemporaryDirectory(prefix='decodium-partial-restart-') as tmp:
    root = Path(tmp).resolve()
    fixture = root / 'image.jpg'
    fixture.write_bytes(source.read_bytes())
    partial_root = root / 'partials'
    partial_root.mkdir()
    outputs = []
    for phase, index in (('seed', first), ('resume', 1 - first)):
        output = root / (phase + '.json')
        env = os.environ.copy()
        env.update(HOME=str(root), CFFIXED_USER_HOME=str(root),
                   QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
                   DECODIUM_LAB_SETTINGS_DIR=str(root), DECODIUM_LAB_PARTIAL_PHASE=phase,
                   DECODIUM_LAB_HAMDRM_PARTIAL_ROOT=str(partial_root),
                   DECODIUM_LAB_PARTIAL_INPUT=str(fixture), DECODIUM_LAB_PARTIAL_OUTPUT=str(output))
        command = [str(app), '--test-mode', '--lab-no-cat', '--lab-no-monitor',
                   '--lab-hamdrm', '--lab-hamdrm-profile', profiles[index],
                   '--lab-sstv-rx-ms', '500', '--lab-quit-ms', '9000', '--rig-name', rig]
        process = subprocess.run(command, env=env, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, timeout=40)
        if process.returncode != 0 or not output.exists():
            raise RuntimeError(phase + ' failed:\n' + process.stdout.decode(errors='replace')[-8000:])
        result = json.loads(output.read_text())
        try:
            Path(result['appData']).resolve().relative_to(root)
        except ValueError:
            raise RuntimeError('application data is outside the isolated laboratory: ' + result['appData'])
        if result.get('phase') != phase or result.get('selectedProfile') != profiles[index]:
            raise RuntimeError(phase + ' selected unexpected profile: ' + repr(result))
        outputs.append(result)
        if phase == 'seed' and not (partial_root / 'hamdrm-partials' / '4a31.hamdrm-partial').is_file():
            raise RuntimeError('the partial was not saved in the isolated laboratory')
    if outputs[0]['appData'] != outputs[1]['appData']:
        raise RuntimeError('the restarted process used a different partial-store root')
    if outputs[1].get('bsrProfileCode') != first or outputs[1].get('complete') is not True:
        raise RuntimeError('the original profile or completed object was not preserved: ' + repr(outputs[1]))
    print('PASS', variant, 'full Decodium restart, original BSR profile and exact object')
