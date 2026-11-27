#!/usr/bin/env python3
"""Exercise advanced decoder controls in an isolated application session."""
import argparse, os, secrets, subprocess, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--variant', choices=('a', 'b'), required=True)
p.add_argument('--binary', type=Path, default=ROOT/'build/decodium')
p.add_argument('--qml', type=Path, default=ROOT/'build/qml/decodium/Main.qml')
a = p.parse_args()
qml = a.qml.resolve()
original = qml.read_text()
marker = 'GlassApplicationWindow {'
if original.count(marker) != 1:
    raise RuntimeError('unexpected QML root')
probe = '''
    Timer { interval:12000; running:true; repeat:false; onTriggered: {
      var f=[];bridge.lockFt2LinkAccess();if(bridge.ft2LinkAccessUnlocked)f.push("lock");
      if(bridge.verifyFt2LinkAccessPassword(__PASSWORD__))f.push("unexpected-unlock");
      if(bridge.ft2LinkAccessUnlocked)f.push("unlocked");bridge.lockFt2LinkAccess();
      console.log("[ADV-DECODE] "+(f.length ? "FAIL "+f.join(",") : "PASS"));
    } }
'''.replace('__PASSWORD__', '""' if a.variant == 'a' else '"synthetic-invalid-password-4de38c53"')

try:
    with tempfile.TemporaryDirectory(prefix='decodium-adv-decode-') as tmp:
        qml.write_text(original.replace(marker, marker+probe, 1))
        env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
        cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','BridgeFt2Access'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','20000']
        result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
        if result.returncode or '[ADV-DECODE] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
            print(result.stdout[-16000:]);raise RuntimeError(f'advanced decoder test failed: {result.returncode}')
        print(f'{a.variant}: invalid password rejected and access remains locked; CAT disabled')
finally:
    qml.write_text(original)
