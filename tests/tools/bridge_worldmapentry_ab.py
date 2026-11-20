#!/usr/bin/env python3
"""Compare map entry interpretation with controlled collaborators."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::replayWorldMapEntry(','void DecodiumBridge::emitCurrentWorldMapQsoPath()')
new=sec((R/'src/bridge/DecodiumBridgeWorldMapEntry.cpp').read_text(),'void DecodiumBridge::replayWorldMapEntry(')
for x,y in [('decodiumBridgeNormalizeWorldMapCall(','normalizeWorldMapCall('),('decodiumBridgeWorldMapCallKey(','worldMapCallKey('),('decodiumBridgeLooksLikeCallsignToken(','looksLikeCallsignToken('),('decodiumBridgeUnresolvedGridOnlyDecode(message, nullptr)','messageIsUnresolvedGridOnlyDecode(message)'),('decodiumBridgeFst4wUnresolvedHashDecode(','isFst4wUnresolvedHashDecode(')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-worldmapentry-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_worldmapentry_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 if results[0].splitlines()!=['cq|1|K1ABC:FN42:FN42:3|K1ABC:FN42','incoming|2|K1ABC:FN42:JN70:1','outgoing73|1|K1ABC:FN42:FN42:3','deferred|1|3','suppressed|3','filter|3']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
