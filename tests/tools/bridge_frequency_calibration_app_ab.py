#!/usr/bin/env python3
"""Compare persisted nonzero calibration and bounds in isolated app sessions."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
module = root / 'src/bridge/DecodiumBridgeFrequencyCalibration.cpp'
settings = root / 'src/bridge/DecodiumBridgeLegacySettings.cpp'
qml = root / 'build/qml/decodium/Main.qml'
sstv = root / 'src/bridge/DecodiumBridgeSstv.cpp'
original_module, original_settings, original_qml = (p.read_text() for p in (module, settings, qml))
original_sstv = sstv.read_text()
baseline = subprocess.check_output(['git', 'show', 'v1.0.737:src/bridge/DecodiumBridge.cpp'], cwd=root, text=True)
start, end = 'double DecodiumBridge::frequencyCalibrationSlopePpm()', 'QStringList DecodiumBridge::satelliteOptions()'
old = baseline[baseline.index(start):baseline.index(end, baseline.index(start))].replace('bridgeLog(', 'decodiumBridgeLogbookLog(')
prefix = original_module[:original_module.index(start)]
if original_module[original_module.index(start):] != old: raise RuntimeError('calibration source differs from baseline')
begin, finish = 'QString DecodiumBridge::legacyIniPath() const\n{', '\nQString DecodiumBridge::legacyConfigGroupName() const'
isolated = original_settings[:original_settings.index(begin)] + begin + '''
    return QDir(qEnvironmentVariable("DECODIUM_LAB_SETTINGS_DIR")).filePath(QStringLiteral("calibration-test.ini"));
}
''' + original_settings[original_settings.index(finish):]
marker = 'GlassApplicationWindow {'
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
rigs, results = [], []
try:
    settings.write_text(isolated)
    anchor = 'QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
    if original_sstv.count(anchor) != 1: raise RuntimeError('unexpected test insertion point')
    injection = '''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values); bool added=addStationFrequencyRow(QStringLiteral("20m"), QStringLiteral("100 Hz"), QStringLiteral("test"));
   double const logical=14074000.0; double const dial=applyFrequencyCalibration(logical);
   QString band; qint64 offset=0; double const restored=logicalFrequencyFromCatDial(dial, &band, &offset);
   return {{"added",added},{"dial",dial},{"restored",restored},{"band",band},{"offset",offset}}; }
#endif
'''
    sstv.write_text(original_sstv.replace(anchor, anchor + injection, 1))
    for label in ('before', 'after'):
        module.write_text(prefix + old if label == 'before' else original_module)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        rig = 'CalibrateAB' + secrets.token_hex(5); rigs.append(rig)
        with tempfile.TemporaryDirectory(prefix='decodium-calibration-') as tmp:
            env = os.environ.copy(); env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
            sinks = []
            for port in (2237, 2239):
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); sock.bind(('127.0.0.1', port)); sinks.append(sock)
            observed = []
            for phase in ('write', 'read', 'bounds'):
                actions = {'write': 'bridge.setFrequencyCalibrationSlopePpm(12.5);bridge.setFrequencyCalibrationInterceptHz(-45);',
                           'read': '', 'bounds': 'bridge.setFrequencyCalibrationSlopePpm(999);bridge.setFrequencyCalibrationInterceptHz(-9999);'}
                probe = '''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{__ACTION__
                 console.log("[CAL-APP] "+JSON.stringify({slope:bridge.frequencyCalibrationSlopePpm(),intercept:bridge.frequencyCalibrationInterceptHz(),tx:bridge.transmitting,monitor:bridge.monitoring,chain:__CHAIN__}));Qt.quit();}}\n'''.replace('__ACTION__', actions[phase]).replace('__CHAIN__', 'bridge.logSstvQso({})' if phase == 'read' else 'null')
                if original_qml.count(marker) != 1: raise RuntimeError('unexpected QML root')
                qml.write_text(original_qml.replace(marker, marker + probe, 1))
                command = ['sandbox-exec', '-p', policy, str(root / 'build/decodium'), '--test-mode', '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor', '--lab-quit-ms', '30000']
                run = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=70)
                combined = run.stdout + '\n' + run.stderr
                rows = [line.split('[CAL-APP] ', 1)[1] for line in combined.splitlines() if '[CAL-APP] ' in line]
                if run.returncode or len(rows) != 1 or '[LAB] CAT auto-connect disabled' not in combined: raise RuntimeError(combined[-9000:])
                value = json.loads(rows[0]); chain = value.pop('chain')
                expected = {'slope': 200 if phase == 'bounds' else 12.5, 'intercept': -1000 if phase == 'bounds' else -45, 'tx': False, 'monitor': False}
                if value != expected: raise RuntimeError(f'{label} {phase}: {value}')
                if phase == 'read':
                    expected_dial = (14074000.0 + 100.0) * (1.0 + 12.5e-6) - 45.0
                    if not chain['added'] or chain['band'] != '20m' or chain['offset'] != 100 or abs(chain['dial'] - expected_dial) > 0.001 or abs(chain['restored'] - 14074000.0) > 0.001:
                        raise RuntimeError(f'{label} calibration chain: {chain}')
                    value['chain'] = chain
                observed.append(value)
            results.append(observed); print(label, json.dumps(observed, sort_keys=True), flush=True)
    if results[0] != results[1]: raise RuntimeError('A/B calibration differs')
finally:
    module.write_text(original_module); settings.write_text(original_settings); qml.write_text(original_qml); sstv.write_text(original_sstv)
    for rig in rigs:
        for parent in (Path.home() / '.qttest/Library/Application Support/IU8LMC', Path.home() / '.qttest/Library/Caches/IU8LMC'):
            path = parent / ('Decodium - ' + rig + ' - test')
            if path.is_dir(): shutil.rmtree(path)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
