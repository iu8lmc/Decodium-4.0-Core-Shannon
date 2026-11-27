#!/usr/bin/env python3
"""Compare async history using Qt's worker pool and temporary files."""
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def section(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=ROOT,stdout=subprocess.PIPE).stdout
old=section(b,'void DecodiumBridge::loadDxccLookupAsync()','void DecodiumBridge::notifyMainQmlReady()')
new=section((ROOT/'src/bridge/DecodiumBridgeAsyncHistory.cpp').read_text(),'void DecodiumBridge::loadDxccLookupAsync()').replace('decodiumBridgeAsyncHistoryLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('history code differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-history-ab-') as t:
 d=Path(t);h=(ROOT/'tests/bridge_refactor_asynchistory_harness.cpp').read_text();out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);(d/'test.cpp').write_text(h)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('history outputs differ')
 print(out[1],end='')
