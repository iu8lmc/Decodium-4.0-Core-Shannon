#!/usr/bin/env python3
"""A/B test backlog behaviour using substitute workers and real Qt queues."""
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(*args,**kw):return subprocess.run(args,check=True,text=True,**kw)
def section(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
base=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=ROOT,stdout=subprocess.PIPE).stdout
old=section(base,'bool DecodiumBridge::ft8LiveDecodeBacklogActive(', 'void DecodiumBridge::requestRigFrequencyFromBridge(')
new=section((ROOT/'src/bridge/DecodiumBridgeDecodeBacklog.cpp').read_text(),'bool DecodiumBridge::ft8LiveDecodeBacklogActive(').replace('decodiumBridgeDecodeBacklogLog(', 'bridgeLog(').replace('decodiumBridgeIsFtx77MessageMode(', 'isFtx77MessageMode(')
if old!=new:raise RuntimeError('backlog source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-backlog-ab-') as tmp:
 d=Path(tmp);h=(ROOT/'tests/bridge_refactor_backlog_harness.cpp').read_text();outputs=[]
 for label,code in [('A',old),('B',new)]:
  (d/'bridge_refactor_under_test.inc').write_text(code);(d/'test.cpp').write_text(h)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  outputs.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if outputs[0]!=outputs[1]:raise RuntimeError('backlog output differs')
 print(outputs[1],end='')
