#!/usr/bin/env python3
"""Compare map and cluster deferral with Qt timer and controlled consumers."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'bool DecodiumBridge::visualFeedsDeferredForTx() const','void DecodiumBridge::queueWorldMapEntryForReplay(')+sec(b,'void DecodiumBridge::scheduleDeferredWorldMapFeedFlush(','void DecodiumBridge::resetWorldMapDisplayFromCurrentDecodes()')
new=sec((R/'src/bridge/DecodiumBridgeVisualFeedScheduling.cpp').read_text(),'bool DecodiumBridge::visualFeedsDeferredForTx() const').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-visualfeedscheduling-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_visualfeedscheduling_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 if results[0].splitlines()!=['notready|1|3','replay|1|0','chunk|A0,B0|1|0','complete|A0,B0,C0|1','tx|0|1|1','flush|1|0']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
