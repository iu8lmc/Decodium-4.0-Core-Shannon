#!/usr/bin/env python3
"""A/B natural FT2-Link completion with captured BlackHole audio."""
import json, subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
main = root / 'src/bridge/DecodiumBridge.cpp'
module = root / 'src/bridge/DecodiumBridgeCompleteTxPlayback.cpp'
modulator = root / 'src/bridge/DecodiumBridgeModulatorFinish.cpp'
original_main, original_module = main.read_text(), module.read_text()
original_modulator = modulator.read_text()
fixture = (root / 'tests/bridge_refactor_fixtures/complete_tx_playback_before.inc').read_text()
marker = 'void DecodiumBridge::completeTxPlayback(const QString& reason, bool error)'
head = original_module.split(marker, 1)[0]
if original_module != head + fixture: raise RuntimeError('unexpected completion method')
instrumented = fixture.replace(marker + '\n{', marker + '\n{\n qWarning("[LAB-COMPLETE-TX]");', 1)
results = []
try:
    modulator.write_text(original_modulator.replace('void DecodiumBridge::finishModulatorIdlePlayback(const QString& reason)\n{', 'void DecodiumBridge::finishModulatorIdlePlayback(const QString& reason)\n{\n qWarning("[LAB-MODULATOR-FINISH]");', 1))
    for label in ('before', 'after'):
        if label == 'before':
            point = '// BRIDGE_COMPLETE_TX_INSERTION_POINT\n'
            if original_main.count(point) != 1: raise RuntimeError('missing insertion point')
            main.write_text(original_main.replace(point, '#undef bridgeLog\n' + head + instrumented + '\n#undef bridgeLog\n#undef txTimelineLog\n#undef retireAudioSink\n#undef activeCatCanPtt\n#undef activeCatSetPtt\n' + point, 1))
            module.write_text(head)
        else:
            main.write_text(original_main); module.write_text(head + instrumented)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        run = subprocess.run(['python3', 'tests/tools/bridge_complete_tx_audio_runner.py', '--binary', 'build/decodium', '--kind', 'ft2link'], cwd=root, capture_output=True, text=True, timeout=100)
        if run.returncode: raise RuntimeError(run.stdout[-8000:] + run.stderr[-3000:])
        value = json.loads(run.stdout.strip().splitlines()[-1])
        if not value['completeSeen'] or not value['stopped'] or value['cat'] or value['audioPeak'] <= 0.003 or value['tailPeak'] >= 0.003: raise RuntimeError(str(value))
        results.append(value); print(label, json.dumps(value, sort_keys=True), flush=True)
    keys = ('seen', 'accepted', 'cat', 'stopped', 'tx', 'enabled', 'completeSeen', 'audioPrepared')
    if any(results[0][key] != results[1][key] for key in keys): raise RuntimeError('A/B differs')
finally:
    main.write_text(original_main); module.write_text(original_module); modulator.write_text(original_modulator)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
