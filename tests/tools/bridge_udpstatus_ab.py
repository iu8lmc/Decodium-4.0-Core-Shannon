#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,n):i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
new=(R/'src/bridge/DecodiumBridgeUdpDecode.cpp').read_text();name='void DecodiumBridge::udpSendStatus()';a=sec(old,name);b=sec(new,name)
if a!=b:raise RuntimeError('UDP status source differs')
for k in ('kSpecialOpNone = 0','kSpecialOpMax = 9'):
 if k not in old or k not in (R/'src/bridge/SpecialOperationBounds.h').read_text():raise RuntimeError('special operation bounds changed')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-udpstatus-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_udpstatus_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_udpstatus.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or 'IU8LMC' not in out[0] or '-10' not in out[0] or '+03' not in out[0]:raise RuntimeError('unexpected status output')
 print(f'UDP status A/B: {len(out[0].splitlines())} legacy, filter, report and bounds combinations match')
