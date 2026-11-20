#!/usr/bin/env python3
"""Compare real QThread lifecycle while substituting decoder worker objects."""
import os, shlex, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def section(s,start,end=None):
 i=s.index(start);return s[i:s.index(end,i) if end else len(s)]
def run(*cmd,**kw):return subprocess.run(cmd,check=True,text=True,**kw)
base=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=ROOT,stdout=subprocess.PIPE).stdout
old=section(base,'void DecodiumBridge::ensureDecodeWorkerForMode(', 'void DecodiumBridge::loadDxccLookupAsync()')
new=section((ROOT/'src/bridge/DecodiumBridgeDecodeWorkers.cpp').read_text(),'void DecodiumBridge::ensureDecodeWorkerForMode(').replace('decodiumBridgeCanonicalApplicationDecodeMode(', 'canonicalApplicationDecodeMode(').replace('decodiumBridgeWorkerLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('worker implementation differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
moc=run('qtpaths','--query','QT_INSTALL_LIBEXECS',stdout=subprocess.PIPE).stdout.strip()+'/moc'
with tempfile.TemporaryDirectory(prefix='decodium-workers-ab-') as tmp:
 d=Path(tmp);harness=(ROOT/'tests/bridge_refactor_workers_harness.cpp').read_text();outputs=[]
 for label,code in [('A',old),('B',new)]:
  (d/'bridge_refactor_under_test.inc').write_text(code);(d/'workers.cpp').write_text(harness)
  run(moc,str(d/'workers.cpp'),'-o',str(d/'workers_harness.moc'))
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'workers.cpp'),*flags,'-o',str(d/'workers'))
  outputs.append(run(str(d/'workers'),stdout=subprocess.PIPE).stdout)
 if outputs[0]!=outputs[1]:raise RuntimeError('worker lifecycle differs')
 print(outputs[1],end='')
