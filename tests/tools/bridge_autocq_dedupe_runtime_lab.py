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
    Timer {interval:12000;running:true;repeat:false;onTriggered:{
      var f=[];bridge.clearCallerQueue();bridge.enqueueStation(__FIRST__);bridge.enqueueStation(__FIRST__);bridge.enqueueStation(__SECOND__);
      if(bridge.callerQueueSize!==2)f.push("queue="+bridge.callerQueueSize);bridge.dequeueStation(__FIRST__);if(bridge.callerQueueSize!==1)f.push("dequeue="+bridge.callerQueueSize);bridge.clearCallerQueue();if(bridge.callerQueueSize!==0)f.push("clear");
      if(bridge.transmitting)f.push("transmitting");console.log("[ADV-DECODE] "+(f.length?"FAIL "+f.join(","):"PASS"));
    }}
'''.replace('__FIRST__','"K1ABC"' if a.variant=='a' else '"W1AW"').replace('__SECOND__','"W1AW"' if a.variant=='a' else '"K1ABC"')

try:
    with tempfile.TemporaryDirectory(prefix='decodium-adv-decode-') as tmp:
        qml.write_text(original.replace(marker, marker+probe, 1))
        env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
        cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','BridgeAutoCqDedupe'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','20000']
        result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
        if result.returncode or '[ADV-DECODE] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
            print(result.stdout[-16000:]);raise RuntimeError(f'advanced decoder test failed: {result.returncode}')
        print(f'{a.variant}: caller queue duplicate prevention and removal passed; CAT disabled')
finally:
    qml.write_text(original)
