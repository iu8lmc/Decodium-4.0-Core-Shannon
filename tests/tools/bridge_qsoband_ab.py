#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,n):i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
new=(R/'src/bridge/DecodiumBridgeQsoLimit.cpp').read_text();names=['QString DecodiumBridge::autoCqBandKeyForFrequency(', 'bool DecodiumBridge::qsoLimitReached(', 'bool DecodiumBridge::isRecentMamWorkedOrLoggedDuplicate('];a='\n'.join(sec(old,n) for n in names);b='\n'.join(sec(new,n) for n in names)
if a!=b or 'kMamRecentWorkedWindowSeconds {600}' not in old or 'kMamRecentWorkedWindowSeconds {600}' not in new:raise RuntimeError('QSO band/limit source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-qsoband-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_qsoband_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_qsoband.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),str(d/'test.cpp'),str(R/'src/radio/Radio.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or 'band=14000000:20M' not in out[0] or 'limit=2:0:1:0' not in out[0] or 'recent=700:0' not in out[0] or 'logged=1' not in out[0]:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
