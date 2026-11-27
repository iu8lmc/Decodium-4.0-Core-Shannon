#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
a=sec(s,'void DecodiumBridge::applyProcessPriority()','void DecodiumBridge::applyReadyProfile(');b=sec((R/'src/bridge/DecodiumBridgeCpuProfile.cpp').read_text(),'void DecodiumBridge::applyProcessPriority()').replace('decodiumBridgeCpuProfileLog(', 'bridgeLog(')
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-priority-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_priority_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or out[0].splitlines()!=['0|32|1|1','1|32768|2|2','2|128|3|3','3|256|4|4','3|256|4|5','failure|2|128|5|2']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
