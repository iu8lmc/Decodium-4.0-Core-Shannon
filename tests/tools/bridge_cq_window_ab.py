#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/cq_reply_window_before.inc').read_text();sig='void DecodiumBridge::armCqAutoReplyWindow('
s=(R/'src/bridge/DecodiumBridgeCqReplyWindow.cpp').read_text();b=sig+s.split(sig,1)[1];b=b.replace('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')
if a!=b:raise SystemExit('CQ reply window differs')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-cqwindow-ab-') as t:
 d=Path(t)
 for code in (a,b):
  (d/'method.inc').write_text(code);exe=d/'test';files=[R/'tests/bridge_refactor_cq_window_harness.cpp',R/'src/bridge/DecodeUtcTime.cpp'];subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(d),'-I'+str(R/'src/bridge'),*[str(p) for p in files],*flags,'-o',str(exe)],check=True);outputs.append(subprocess.check_output([str(exe)],text=True))
if outputs[0]!=outputs[1]:raise SystemExit('CQ window A/B mismatch')
print('CQ reply window A/B: '+outputs[0],end='')
