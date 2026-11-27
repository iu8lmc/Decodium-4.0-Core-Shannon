#!/usr/bin/env python3
"""Compare local QSY guard decisions without a radio."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'bool DecodiumBridge::shouldIgnoreCatFrequencyDuringLocalQsy(','void DecodiumBridge::applyRemoteDialFrequency(')+sec(b,'bool DecodiumBridge::shouldIgnoreLegacyAudioFrequencyDuringLocalQsy(','void DecodiumBridge::applyRemoteBandChange(')
new=sec((R/'src/bridge/DecodiumBridgeQsyGuard.cpp').read_text(),'bool DecodiumBridge::shouldIgnoreCatFrequencyDuringLocalQsy(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-qsyguard-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_qsyguard_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 if results[0].splitlines()!=['inactive|0','stale|1|1','target|0|0','manual|0|0','rx|1|0|0','expired|0|0']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
