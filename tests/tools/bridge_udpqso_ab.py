#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,n):i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
new=(R/'src/bridge/DecodiumBridgeAdifUdp.cpp').read_text();name='void DecodiumBridge::udpSendLoggedQso(';a=sec(old,name);b=sec(new,name).replace('decodiumBridgeAdifUdpLog(', 'bridgeLog(')
if a!=b:raise RuntimeError('UDP QSO implementation differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core','Qt6Network',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-udpqso-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_udpqso_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_udpqso.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or 'Q:K1ABC:FN42:14074000:-10:-08' not in out[0] or 'A:<CALL:5>K1ABC <BAND:3>20m' not in out[0] or 'warnings=1' not in out[0]:raise RuntimeError('unexpected output')
 print(f'UDP QSO A/B: {len(out[0].splitlines())} legacy, availability and destination combinations match')
