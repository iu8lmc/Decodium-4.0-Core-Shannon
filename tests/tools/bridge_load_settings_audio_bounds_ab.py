#!/usr/bin/env python3
"""Compare audio setting bounds at startup in both Bridge placements.

Uses isolated profiles and disabled CAT/monitor/TX. Restores source and QML
even on failure; never edits the user's settings.
"""
import json
import os
import secrets
import shutil
import socket
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
main = root / 'src/bridge/DecodiumBridge.cpp'
module = root / 'src/bridge/DecodiumBridgeLoadSettings.cpp'
qml = root / 'build/qml/decodium/Main.qml'
original_main, original_module, original_qml = (p.read_text() for p in (main, module, qml))
start = original_module.index('void DecodiumBridge::loadSettings()')
end = original_module.index('void DecodiumBridge::reloadBridgeSettingsFromPersistentStore()')
head, body, tail = original_module[:start], original_module[start:end], original_module[end:]
rigs = []
results = {}
try:
    for placement in ('before', 'after'):
        main.write_text(original_main + '\n' + head + body if placement == 'before' else original_main)
        module.write_text(head + tail if placement == 'before' else original_module)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL)
        for case, values, expected in (
            ('low', (-25, -20, -1, -2), (0, 0, 0, 0)),
            ('high', (125, 500, 8, 9), (100, 450, 3, 3)),
        ):
            rig = 'Bounds' + secrets.token_hex(5)
            rigs.append(rig)
            with tempfile.TemporaryDirectory(prefix='decodium-settings-bounds-') as tmp:
                settings = Path(tmp) / 'Decodium'
                settings.mkdir()
                (settings / 'Decodium3.ini').write_text('[General]\nCurrentMultiSettingsConfiguration=\n' + ''.join(
                    f'{k}={v}\n' for k, v in zip(('rxInputLevel', 'txOutputLevel', 'audioInputChannel', 'audioOutputChannel'), values)))
                probe = '''\n Timer {interval:12000;running:true;repeat:false;onTriggered:{console.log('[BOUNDS] '+JSON.stringify([bridge.rxInputLevel,bridge.txOutputLevel,bridge.audioInputChannel,bridge.audioOutputChannel,bridge.transmitting]));Qt.quit();}}\n'''
                qml.write_text(original_qml.replace('GlassApplicationWindow {', 'GlassApplicationWindow {' + probe, 1))
                env = os.environ.copy()
                env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', QT_QPA_PLATFORM='offscreen', QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms', QT_QUICK_BACKEND='software')
                sockets = []
                for port in (2237, 2239):
                    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    sock.bind(('127.0.0.1', port))
                    sockets.append(sock)
                policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
                command = ['sandbox-exec', '-p', policy, str(root / 'build/decodium'), '--test-mode', '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor', '--lab-quit-ms', '20000']
                run = subprocess.run(command, cwd=root, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=50)
                for sock in sockets:
                    sock.close()
                rows = [x.split('[BOUNDS] ', 1)[1] for x in run.stdout.splitlines() if '[BOUNDS] ' in x]
                if run.returncode or len(rows) != 1 or '[LAB] CAT auto-connect disabled' not in run.stdout:
                    raise RuntimeError(run.stdout[-6000:])
                result = json.loads(rows[0])
                if result != list(expected) + [False]:
                    raise AssertionError((placement, case, result, expected))
                results[(placement, case)] = result
                print(placement, case, json.dumps(result), flush=True)
    for case in ('low', 'high'):
        assert results['before', case] == results['after', case]
finally:
    for path, content in ((main, original_main), (module, original_module), (qml, original_qml)):
        path.write_text(content)
    for rig in rigs:
        for parent in (Path.home() / '.qttest/Library/Application Support/IU8LMC', Path.home() / '.qttest/Library/Caches/IU8LMC'):
            path = parent / ('Decodium - ' + rig + ' - test')
            if path.is_dir():
                shutil.rmtree(path)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL)
