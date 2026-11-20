#!/usr/bin/env python3
"""Compare binary Bridge file I/O in isolated release/current app sessions."""
import json
import os
import secrets
import shutil
import socket
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
module = root / 'src/bridge/DecodiumBridgeFileIO.cpp'
qml = root / 'build/qml/decodium/Main.qml'
original, original_qml = module.read_text(), qml.read_text()
start = 'QVariantMap DecodiumBridge::readTextFile('
end = 'QString DecodiumBridge::defaultFt2LinkReceivedFilesDirectory()'
baseline = subprocess.check_output(['git', 'show', 'v1.0.737:src/bridge/DecodiumBridge.cpp'], cwd=root, text=True)
old = baseline[baseline.index(start):baseline.index(end, baseline.index(start))]
prefix = original[:original.index(start)]
if (prefix + old).strip() != original.strip():
    raise RuntimeError('file I/O differs from release')
rigs, results = [], []
try:
    for label, source in (('release', prefix + old), ('current', original)):
        module.write_text(source)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        rig = 'FileIoAB' + secrets.token_hex(5)
        rigs.append(rig)
        with tempfile.TemporaryDirectory(prefix='decodium-fileio-app-') as tmp:
            path = Path(tmp) / 'nested' / 'synthetic.bin'
            probe = '''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{
                var w=bridge.writeFileBytes(__PATH__,"AAECA/8=");
                var full=bridge.readFileBytes(__PATH__,100);
                var short=bridge.readFileBytes(__PATH__,2);
                var bad=bridge.writeFileBytes("", "YQ==");
                console.log('[FILEIO-AB] '+JSON.stringify({write:w.ok,bytes:w.bytes,read:full.ok,data:full.base64,truncated:short.truncated,shortBytes:short.bytes,bad:bad.ok,tx:bridge.transmitting,cat:bridge.catConnected}));Qt.quit();}}
            '''.replace('__PATH__', json.dumps(str(path)))
            assert original_qml.count('GlassApplicationWindow {') == 1
            qml.write_text(original_qml.replace('GlassApplicationWindow {', 'GlassApplicationWindow {' + probe, 1))
            env = os.environ.copy()
            env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
            sockets = []
            for port in (2237, 2239):
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sock.bind(('127.0.0.1', port)); sockets.append(sock)
            policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
            cmd = ['sandbox-exec', '-p', policy, str(root / 'build/decodium'), '--test-mode', '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor', '--lab-quit-ms', '25000']
            result = subprocess.run(cmd, cwd=root, env=env, capture_output=True, text=True, timeout=45)
            lines = [line.split('[FILEIO-AB] ', 1)[1] for line in (result.stdout + '\n' + result.stderr).splitlines() if '[FILEIO-AB] ' in line]
            if result.returncode or len(lines) != 1:
                raise RuntimeError(result.stdout[-6000:] + result.stderr[-3000:])
            observed = json.loads(lines[0])
            expected = {'write': True, 'bytes': 5, 'read': True, 'data': 'AAECA/8=', 'truncated': True, 'shortBytes': 2, 'bad': False, 'tx': False, 'cat': False}
            if observed != expected or path.read_bytes() != bytes([0, 1, 2, 3, 255]):
                raise RuntimeError('unexpected result ' + repr(observed))
            results.append(observed); print(label, json.dumps(observed, sort_keys=True), flush=True)
            for sock in sockets: sock.close()
    if results[0] != results[1]: raise RuntimeError('application results differ')
finally:
    module.write_text(original); qml.write_text(original_qml)
    for rig in rigs:
        for parent in (Path.home() / '.qttest/Library/Application Support/IU8LMC', Path.home() / '.qttest/Library/Caches/IU8LMC'):
            path = parent / ('Decodium - ' + rig + ' - test')
            if path.is_dir(): shutil.rmtree(path)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
