#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/alc_calibration_before.inc').read_text();sig='void DecodiumBridge::setAlcTarget('
s=(R/'src/bridge/DecodiumBridgeAlcCalibration.cpp').read_text();b=sig+s.split(sig,1)[1];b=b.replace('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')
if a!=b:raise SystemExit('ALC source differs')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-alc-ab-') as t:
 d=Path(t);env=os.environ.copy();env['ALC_LAB_SETTINGS']=t
 for code in (a,b):
  (d/'method.inc').write_text(code);exe=d/'test';subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(d),'-I'+str(R/'src/bridge'),str(R/'tests/bridge_refactor_alc_harness.cpp'),*flags,'-o',str(exe)],check=True);outputs.append(subprocess.check_output([str(exe)],text=True,env=env))
if outputs[0]!=outputs[1]:raise SystemExit('ALC A/B mismatch')
print('ALC A/B: '+outputs[0],end='')
