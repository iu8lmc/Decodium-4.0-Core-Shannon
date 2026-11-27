#!/usr/bin/env python3
"""Compare world-map entry freshness and reset with controlled entries."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'QString DecodiumBridge::worldMapFeedEntryKey(','bool DecodiumBridge::visualFeedsDeferredForTx() const')+sec(b,'void DecodiumBridge::resetWorldMapDisplayFromCurrentDecodes()','void DecodiumBridge::replayWorldMapEntry(')
new=sec((R/'src/bridge/DecodiumBridgeWorldMapReset.cpp').read_text(),'QString DecodiumBridge::worldMapFeedEntryKey(')
for x,y in [('decodiumBridgeMirrorEntryKey(','decodeMirrorEntryKey('),('decodiumBridgeSecondsFromUtcDisplayToken(','secondsFromUtcDisplayToken('),('decodiumBridgeSignedUtcSecondDelta(','signedUtcSecondDelta(')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-worldmapreset-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_worldmapreset_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 if results[0].splitlines()!=['old|0|1','fresh|1|1','fallback|CQ K1ABC','reset|3|0|0|0|1']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
