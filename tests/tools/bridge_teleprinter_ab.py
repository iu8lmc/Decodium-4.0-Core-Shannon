#!/usr/bin/env python3
"""Compare teleprinter methods against the release baseline with real Qt file I/O."""
import os, shlex, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(*args,**kw): return subprocess.run(args,check=True,text=True,**kw)
def sec(s,start,end=None):
 i=s.index(start); return s[i:s.index(end,i) if end else len(s)]
baseline=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=ROOT,stdout=subprocess.PIPE).stdout
old=sec(baseline,'void DecodiumBridge::aggiungiRigaRtty (','void DecodiumBridge::appendDecodeMapToList(')
new=sec((ROOT/'src/bridge/DecodiumBridgeTeleprinter.cpp').read_text(),'void DecodiumBridge::aggiungiRigaRtty (')
if old!=new: raise RuntimeError('teleprinter source differs from baseline')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-teleprinter-ab-') as tmp:
 d=Path(tmp); (d/'test.cpp').write_text((ROOT/'tests/bridge_refactor_teleprinter_harness.cpp').read_text()); outputs=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  outputs.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if outputs[0]!=outputs[1]: raise RuntimeError('teleprinter output differs')
 print(outputs[1],end='')
