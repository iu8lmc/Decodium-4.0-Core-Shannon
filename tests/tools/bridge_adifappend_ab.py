#!/usr/bin/env python3
"""Compare ADIF append and metadata side effects using isolated files."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;old=sec(s,'bool DecodiumBridge::appendAdifRecord(','QVariantList DecodiumBridge::logbookProfiles() const')+sec(s,'bool DecodiumBridge::openActiveLogbookFolder() const','bool DecodiumBridge::exportAdif(');new=sec((R/'src/bridge/DecodiumBridgeAdifAppend.cpp').read_text(),'bool DecodiumBridge::appendAdifRecord(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-adifappend-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_adifappend_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('A/B mismatch')
 if results[0].splitlines()!=['write|1|1|1|1|1|1|1|1|1|1|1','append|1|1|2|2','failure|0|0','folder|1']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
