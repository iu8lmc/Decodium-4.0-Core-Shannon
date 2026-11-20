#!/usr/bin/env python3
"""Check online-service bridge forwarding against the original implementation."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def section(s):
 start='bool DecodiumBridge::offlineMode() const';end='QObject * DecodiumBridge::callsignIntelligence() const'
 return s[s.index(start):s.index(end)]
old=section(run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=r,stdout=subprocess.PIPE).stdout)
new=(r/'src/bridge/DecodiumBridgeOffline.cpp').read_text();new=new[new.index('bool DecodiumBridge::offlineMode() const'):]
if old!=new:raise RuntimeError('implementation differs from original')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-offline-ab-') as tmp:
 d=Path(tmp); results=[]
 for source in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(source)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(d),str(r/'tests/bridge_refactor_offline_harness.cpp'),*flags,'-o',str(d/'test'))
  results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('A/B outputs differ')
 print(results[0],end='')
