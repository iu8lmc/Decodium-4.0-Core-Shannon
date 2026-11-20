#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/wait_pounce_before.inc').read_text();sig='bool DecodiumBridge::tryStartWaitPounceFromEntry('
s=(R/'src/bridge/DecodiumBridgeWaitPounce.cpp').read_text();b=sig+s.split(sig,1)[1]
for new,old in [('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey('),('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')]:b=b.replace(new,old)
if a!=b:raise SystemExit('wait-pounce differs')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-wait-ab-') as t:
 d=Path(t)
 for code in (a,b):
  (d/'method.inc').write_text(code);exe=d/'test';subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(d),str(R/'tests/bridge_refactor_wait_pounce_harness.cpp'),*flags,'-o',str(exe)],check=True);outputs.append(subprocess.check_output([str(exe)],text=True))
if outputs[0]!=outputs[1]:raise SystemExit('wait-pounce A/B mismatch')
print('Wait-pounce A/B: '+outputs[0],end='')
