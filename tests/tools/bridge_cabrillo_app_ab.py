#!/usr/bin/env python3
"""Compare real app Cabrillo exports using isolated synthetic logs."""
import json
import os
from pathlib import Path
import secrets
import shutil
import socket
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
module = root / 'src/bridge/DecodiumBridgeCabrillo.cpp'
paths = root / 'src/bridge/DecodiumBridgeLegacyAllTxt.cpp'
qml = root / 'build/qml/decodium/Main.qml'
original_module, original_paths, original_qml = (p.read_text() for p in (module, paths, qml))
baseline = subprocess.check_output(['git', 'show', 'v1.0.737:src/bridge/DecodiumBridge.cpp'], cwd=root, text=True)
start = 'bool DecodiumBridge::exportCabrillo('
end = '// ============================================================\n// C16'
old = baseline[baseline.index(start):baseline.index(end, baseline.index(start))]
old = old.replace('expandedLocalFilePath(', 'decodiumBridgeExpandedLocalFilePath(').replace('ensureParentDirectoryForFile(', 'decodiumBridgeEnsureParentDirectoryForFile(')
prefix = original_module[:original_module.index(start)]
if original_module[original_module.index(start):].strip() != old.strip():
    raise RuntimeError('Cabrillo method differs from baseline')
begin = 'QString DecodiumBridge::logAllTxtPath() const\n{'
index = original_paths.index(begin)
open_brace = original_paths.index('{', index)
depth, close = 1, open_brace + 1
while depth:
    if original_paths[close] == '{': depth += 1
    if original_paths[close] == '}': depth -= 1
    close += 1
isolated = original_paths[:index] + begin + '\n    return qEnvironmentVariable("DECODIUM_LAB_CABRILLO_INPUT");\n}' + original_paths[close:]
marker = 'GlassApplicationWindow {'
if original_qml.count(marker) != 1: raise RuntimeError('unexpected QML root')
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
rigs = []

try:
    paths.write_text(isolated)
    results = {}
    for label in ('before', 'after'):
        module.write_text(prefix + old if label == 'before' else original_module)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        rig = 'CabrilloAB' + secrets.token_hex(5)
        rigs.append(rig)
        with tempfile.TemporaryDirectory(prefix='decodium-cabrillo-app-') as tmp:
            directory = Path(tmp)
            source, output = directory / 'all.txt', directory / 'export.log'
            source.write_text('20261109_123456 K1ABC W1XYZ FN31 14.074000MHz FT8\n20261109_123556 K1ABC N0CALL EM10 7.074000MHz FT8\nmalformed\n')
            env = os.environ.copy()
            env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_LAB_CABRILLO_INPUT=str(source), DECODIUM_DISABLE_QML_CACHE='1', QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
            probe = '''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{
                console.log("[CABRILLO-APP] "+JSON.stringify({ok:bridge.exportCabrillo(__PATH__),tx:bridge.transmitting,monitor:bridge.monitoring}));Qt.quit();}}
            '''.replace('__PATH__', json.dumps(str(output)))
            qml.write_text(original_qml.replace(marker, marker + probe, 1))
            sinks = []
            for port in (2237, 2239):
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); sock.bind(('127.0.0.1', port)); sinks.append(sock)
            command = ['sandbox-exec', '-p', policy, str(root / 'build/decodium'), '--test-mode', '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor', '--lab-callsign', 'K1ABC', '--lab-quit-ms', '30000']
            run = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=70)
            combined = run.stdout + '\n' + run.stderr
            lines = [line.split('[CABRILLO-APP] ', 1)[1] for line in combined.splitlines() if '[CABRILLO-APP] ' in line]
            if run.returncode or len(lines) != 1 or '[LAB] CAT auto-connect disabled' not in combined:
                raise RuntimeError(combined[-9000:])
            value = json.loads(lines[0])
            if not value['ok'] or value['tx'] or value['monitor']:
                raise RuntimeError(str(value))
            text = output.read_text()
            if (text.count('QSO: ') != 2 or 'W1XYZ' not in text or 'N0CALL' not in text
                    or 'END-OF-LOG:' not in text or 'CALLSIGN: K1ABC' not in text
                    or '2026-11-09 1234' not in text or '2026-11-09 1235' not in text):
                raise RuntimeError('unexpected export: ' + text)
            qso_lines = [line for line in text.splitlines() if line.startswith('QSO: ')]
            if not qso_lines[0].startswith('QSO: 14074 ') or not qso_lines[1].startswith('QSO: 7074 '):
                raise RuntimeError('incorrect exported frequency: ' + repr(qso_lines))
            results[label] = text
            print(label, 'exported', text.count('QSO: '), 'QSOs', flush=True)
    if results['before'] != results['after']:
        raise RuntimeError('before and after Cabrillo exports differ')
finally:
    module.write_text(original_module)
    paths.write_text(original_paths)
    qml.write_text(original_qml)
    for rig in rigs:
        for parent in (Path.home() / '.qttest/Library/Application Support/IU8LMC', Path.home() / '.qttest/Library/Caches/IU8LMC'):
            path = parent / ('Decodium - ' + rig + ' - test')
            if path.is_dir(): shutil.rmtree(path)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
