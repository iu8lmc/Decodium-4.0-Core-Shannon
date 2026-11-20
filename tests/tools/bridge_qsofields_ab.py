#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
a=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
start='QString DecodiumBridge::dxGrid() const';end='void DecodiumBridge::setAutoSeq('
old=a[a.index(start):a.index(end,a.index(start))].strip()
new=(R/'src/bridge/DecodiumBridgeQsoFields.cpp').read_text();new=new[new.index(start):].strip()
if old!=new:raise RuntimeError('QSO fields implementation differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-qsofields-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_qsofields_harness.cpp').read_text());out=[]
 for code in (old,new):
  (d/'bridge_refactor_qsofields.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),str(d/'test.cpp'),str(R/'src/bridge/DecodeSignalReport.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('QSO fields A/B differs')
 print(f'QSO fields A/B: {len(out[0].splitlines())} states match with real normalization and simulated legacy backend')
