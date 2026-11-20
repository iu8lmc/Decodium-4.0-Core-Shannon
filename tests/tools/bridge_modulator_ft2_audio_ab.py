#!/usr/bin/env python3
"""Compare natural FT2-Link modulator completion using virtual audio."""
import argparse, json, subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--kind', choices=('ft2link', 'cw'), default='ft2link')
parser.add_argument('--fix-cw', action='store_true')
parser.add_argument('--two-cw', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
main = root / 'src/bridge/DecodiumBridge.cpp'
module = root / 'src/bridge/DecodiumBridgeModulatorFinish.cpp'
original_main, original_module = main.read_text(), module.read_text()
fixture = (root / 'tests/bridge_refactor_fixtures/modulator_idle_before.inc').read_text()
marker = 'void DecodiumBridge::finishModulatorIdlePlayback(const QString& reason)'
head = original_module.split(marker, 1)[0]
instrumented = fixture.replace(marker + '\n{', marker + '\n{\n qWarning("[LAB-MODULATOR-FINISH]");', 1)
if args.fix_cw:
    if original_module == head + fixture: raise RuntimeError('CW correction is missing from production')
    fixed = original_module[len(head):].replace(marker + '\n{', marker + '\n{\n qWarning("[LAB-MODULATOR-FINISH]");', 1)
else:
    if original_module != head + fixture: raise RuntimeError('production includes the CW fix; use --fix-cw')
    fixed = instrumented
results, failures = [], []
try:
    for label in ('before', 'after'):
        if label == 'before':
            point = '// BRIDGE_MODULATOR_FINISH_INSERTION_POINT\n'
            if original_main.count(point) != 1: raise RuntimeError('missing insertion point')
            main.write_text(original_main.replace(point, '#undef bridgeLog\n' + head + instrumented + '\n#undef bridgeLog\n#undef txTimelineLog\n#undef activeCatCanPtt\n#undef activeCatSetPtt\n' + point, 1)); module.write_text(head)
        else:
            main.write_text(original_main); module.write_text(head + (fixed if args.fix_cw else instrumented))
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        command = ['python3', 'tests/tools/bridge_complete_tx_audio_runner.py', '--binary', 'build/decodium', '--kind', args.kind, '--expect-modulator']
        if args.two_cw: command.append('--two-cw')
        run = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=100)
        if run.returncode:
            detail = run.stdout[-8000:] + run.stderr[-3000:]
            failures.append(label)
            print(label, 'FAILED', detail, flush=True)
            continue
        value = json.loads(run.stdout.strip().splitlines()[-1])
        if not value['modulatorSeen'] or not value['stopped'] or value['cat'] or value['audioPeak'] <= 0.003 or value['tailPeak'] >= 0.003: raise RuntimeError(str(value))
        results.append(value); print(label, json.dumps(value, sort_keys=True), flush=True)
    if args.fix_cw and args.kind == 'cw':
        if failures != ['before'] or len(results) != 1: raise RuntimeError('unexpected CW fix comparison: ' + str(failures))
        print('CW fix comparison: baseline failed, corrected passed', flush=True)
        raise SystemExit(0)
    if failures: raise RuntimeError('failed placements: ' + ', '.join(failures))
    keys = ('seen', 'accepted', 'cat', 'stopped', 'tx', 'enabled', 'modulatorSeen', 'audioPrepared')
    if any(results[0][key] != results[1][key] for key in keys): raise RuntimeError('A/B differs')
finally:
    main.write_text(original_main); module.write_text(original_module)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
