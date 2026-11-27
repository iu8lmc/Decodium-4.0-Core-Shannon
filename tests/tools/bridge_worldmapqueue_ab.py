#!/usr/bin/env python3
"""Compare map queue and enrichment with controlled map services."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::queueWorldMapEntryForReplay(','void DecodiumBridge::scheduleDeferredWorldMapFeedFlush(')
new=sec((R/'src/bridge/DecodiumBridgeWorldMapQueue.cpp').read_text(),'void DecodiumBridge::queueWorldMapEntryForReplay(').replace('decodiumBridgeUnresolvedGridOnlyDecode(message, nullptr)','messageIsUnresolvedGridOnlyDecode(message)').replace('decodiumBridgeFst4wUnresolvedHashDecode(','isFst4wUnresolvedHashDecode(').replace('decodiumBridgeAudioLevelLog(','bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-worldmapqueue-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_worldmapqueue_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 if results[0].splitlines()!=['initial|1|Testland|20m|1|0|1','duplicate|2|1','filtered|4|1','disabled|5|1','bounded|300|0|1','defer|600|1']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
