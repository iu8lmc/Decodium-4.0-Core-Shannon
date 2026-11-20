#!/usr/bin/env python3
"""Compare station-frequency persistence in isolated before/after app sessions."""
import json
import os
from pathlib import Path
import secrets
import shutil
import socket
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
module = root / 'src/bridge/DecodiumBridgeStationFrequencies.cpp'
settings = root / 'src/bridge/DecodiumBridgeLegacySettings.cpp'
qml = root / 'build/qml/decodium/Main.qml'
original_module, original_settings, original_qml = (p.read_text() for p in (module, settings, qml))
baseline = subprocess.check_output(['git', 'show', 'v1.0.737:src/bridge/DecodiumBridge.cpp'], cwd=root, text=True)
start = 'QVariantList DecodiumBridge::stationFrequencyRows() const'
end = 'double DecodiumBridge::frequencyCalibrationSlopePpm()'
old = baseline[baseline.index(start):baseline.index(end, baseline.index(start))]
old = old.replace('bridgeLog(', 'decodiumBridgeLogbookLog(').replace('parseFrequencyOffsetHzValue(', 'decodiumBridgeParseFrequencyOffsetHzValue(')
prefix = original_module[:original_module.index(start)]
if original_module[original_module.index(start):] != old:
    raise RuntimeError('station method source differs from baseline')
begin = 'QString DecodiumBridge::legacyIniPath() const\n{'
finish = '\nQString DecodiumBridge::legacyConfigGroupName() const'
isolated = original_settings[:original_settings.index(begin)] + begin + '''
    return QDir(qEnvironmentVariable("DECODIUM_LAB_SETTINGS_DIR")).filePath(QStringLiteral("station-test.ini"));
}
''' + original_settings[original_settings.index(finish):]
marker = 'GlassApplicationWindow {'
if original_qml.count(marker) != 1:
    raise RuntimeError('unexpected QML root')
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
rigs = []

try:
    settings.write_text(isolated)
    results = {}
    for label in ('before', 'after'):
        module.write_text(prefix + old if label == 'before' else original_module)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        rig = 'StationAB' + secrets.token_hex(5)
        rigs.append(rig)
        with tempfile.TemporaryDirectory(prefix='decodium-station-restart-') as tmp:
            env = os.environ.copy()
            env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
            sinks = []
            for port in (2237, 2239):
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sock.bind(('127.0.0.1', port))
                sinks.append(sock)
            observations = []
            for phase in ('write', 'edit', 'verify_edit', 'delete', 'empty'):
                actions = {
                    'write': 'var a=bridge.addStationFrequencyRow("20m", "100 Hz", "dipole");',
                    'edit': 'var a=bridge.updateStationFrequencyRow(0, "20m", "200 Hz", "loop");',
                    'verify_edit': 'var a=true;',
                    'delete': 'var a=bridge.deleteStationFrequencyRow(0);',
                    'empty': 'var a=true;',
                }
                action = actions[phase]
                probe = '''\n Timer { interval:10000; running:true; repeat:false; onTriggered: { __ACTION__
                    console.log("[STATION-RESTART] "+JSON.stringify({ok:a,rows:bridge.stationFrequencyRows(),tx:bridge.transmitting,monitor:bridge.monitoring})); Qt.quit(); } }
                '''.replace('__ACTION__', action)
                qml.write_text(original_qml.replace(marker, marker + probe, 1))
                command = ['sandbox-exec', '-p', policy, str(root / 'build/decodium'), '--test-mode', '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor', '--lab-quit-ms', '30000']
                run = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=70)
                lines = [line.split('[STATION-RESTART] ', 1)[1] for line in (run.stdout + '\n' + run.stderr).splitlines() if '[STATION-RESTART] ' in line]
                if run.returncode or len(lines) != 1 or '[LAB] CAT auto-connect disabled' not in run.stdout + run.stderr:
                    raise RuntimeError(f'exit={run.returncode} markers={len(lines)} cat_marker={"[LAB] CAT auto-connect disabled" in run.stdout + run.stderr}\n' + run.stdout[-8000:] + run.stderr[-1000:])
                value = json.loads(lines[0])
                expected_count = 1 if phase in ('write', 'edit', 'verify_edit') else 0
                if not value['ok'] or value['tx'] or value['monitor'] or len(value['rows']) != expected_count:
                    raise RuntimeError(f'{label} {phase}: {value}')
                if expected_count:
                    row = value['rows'][0]
                    expected_offset, expected_antenna = (100, 'dipole') if phase == 'write' else (200, 'loop')
                    if row['band'] != '20m' or row['offsetHz'] != expected_offset or row['antenna'] != expected_antenna:
                        raise RuntimeError(f'{label} {phase}: unexpected station {row}')
                observations.append(value)
            results[label] = observations
            print(label, json.dumps(observations, sort_keys=True), flush=True)
    if results['before'] != results['after']:
        raise RuntimeError('before/after observations differ')
finally:
    module.write_text(original_module)
    settings.write_text(original_settings)
    qml.write_text(original_qml)
    for rig in rigs:
        for parent in (Path.home() / '.qttest/Library/Application Support/IU8LMC', Path.home() / '.qttest/Library/Caches/IU8LMC'):
            path = parent / ('Decodium - ' + rig + ' - test')
            if path.is_dir():
                shutil.rmtree(path)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
