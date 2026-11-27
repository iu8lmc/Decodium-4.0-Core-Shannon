#!/usr/bin/env python3
"""Compare FT2 access control with synthetic compile-time credentials.

The original application sources and executable are restored after the test.
No production password, CAT device, or external service is used.
"""
import base64
import hashlib
import json
import os
import secrets
import shutil
import socket
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
main = ROOT / 'src/bridge/DecodiumBridge.cpp'
module = ROOT / 'src/bridge/DecodiumBridgeFt2Access.cpp'
crypto = ROOT / 'src/bridge/Ft2LinkAccessCrypto.cpp'
qml = ROOT / 'build/qml/decodium/Main.qml'
originals = {p: p.read_text() for p in (main, module, crypto, qml)}
salt = b'isolated-test-01'
password = 'synthetic-ft2-only'
digest = hashlib.pbkdf2_hmac('sha256', password.encode(), salt, 10000)
credentials = ('#undef DECODIUM_FT2LINK_ACCESS_SALT_B64\n#undef DECODIUM_FT2LINK_ACCESS_HASH_B64\n#undef DECODIUM_FT2LINK_ACCESS_ITERATIONS\n'
               '#define DECODIUM_FT2LINK_ACCESS_SALT_B64 "' + base64.b64encode(salt).decode() + '"\n'
               '#define DECODIUM_FT2LINK_ACCESS_HASH_B64 "' + base64.b64encode(digest).decode() + '"\n'
               '#define DECODIUM_FT2LINK_ACCESS_ITERATIONS 10000\n')
probe = '''
Timer { interval:12000; running:true; repeat:false; onTriggered: {
  bridge.lockFt2LinkAccess();
  var bad=bridge.verifyFt2LinkAccessPassword("wrong-synthetic");
  var locked=!bridge.ft2LinkAccessUnlocked;
  var good=bridge.verifyFt2LinkAccessPassword("synthetic-ft2-only");
  var unlocked=bridge.ft2LinkAccessUnlocked;
  bridge.lockFt2LinkAccess();
  console.log("[FT2-ACCESS-LAB] "+JSON.stringify({bad:bad,locked:locked,good:good,unlocked:unlocked,relocked:!bridge.ft2LinkAccessUnlocked,cat:bridge.catConnected,tx:bridge.transmitting}));
  Qt.quit();
} }
'''
rigs = []
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
try:
    crypto.write_text(credentials + originals[crypto])
    results = []
    for label in ('before', 'after'):
        main.write_text(originals[main] + '\n' + originals[module] if label == 'before' else originals[main])
        module.write_text('#include "DecodiumBridge.h"\n' if label == 'before' else originals[module])
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=ROOT, check=True)
        marker = 'GlassApplicationWindow {'
        assert originals[qml].count(marker) == 1
        qml.write_text(originals[qml].replace(marker, marker + probe, 1))
        rig = 'Ft2AccessLab' + secrets.token_hex(5)
        rigs.append(rig)
        with tempfile.TemporaryDirectory(prefix='decodium-ft2-access-lab-') as tmp:
            env = os.environ.copy()
            env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1',
                       QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
            sockets = []
            for port in (2237, 2239):
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                s.bind(('127.0.0.1', port)); sockets.append(s)
            cmd = ['sandbox-exec', '-p', policy, str(ROOT/'build/decodium'), '--test-mode',
                   '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor', '--lab-quit-ms', '22000']
            run = subprocess.run(cmd, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
            lines = [line.split('[FT2-ACCESS-LAB] ', 1)[1] for line in run.stdout.splitlines() if '[FT2-ACCESS-LAB] ' in line]
            if run.returncode or len(lines) != 1 or '[LAB] CAT auto-connect disabled' not in run.stdout:
                raise RuntimeError(run.stdout[-6000:])
            actual = json.loads(lines[0])
            expected = {'bad': False, 'locked': True, 'good': True, 'unlocked': True, 'relocked': True, 'cat': False, 'tx': False}
            if actual != expected:
                raise RuntimeError(str(actual))
            results.append(actual)
            print(label, json.dumps(actual, sort_keys=True), flush=True)
            for s in sockets: s.close()
    assert results[0] == results[1]
finally:
    for path, content in originals.items(): path.write_text(content)
    for rig in rigs:
        for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC', Path.home()/'.qttest/Library/Caches/IU8LMC'):
            path = parent/('Decodium - ' + rig + ' - test')
            if path.is_dir(): shutil.rmtree(path)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
