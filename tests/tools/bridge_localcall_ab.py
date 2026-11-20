#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,n):
 i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
new=(R/'src/bridge/DecodiumBridgeDecodeIdentity.cpp').read_text();name='QStringList DecodiumBridge::localDecodeCallCandidates() const';a=sec(old,name);b=sec(new,name)
if a!=b:raise RuntimeError('implementation differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-localcall-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_localcall_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_localcall.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),str(d/'test.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('local candidates differ')
 print(f'Local calls A/B: {len(out[0].splitlines())} cases match with real token rules and simulated legacy backend')
