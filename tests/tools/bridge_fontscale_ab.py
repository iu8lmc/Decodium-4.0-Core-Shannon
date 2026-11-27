#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def section(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
a=section(run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout,'void DecodiumBridge::increaseFontScale()','void DecodiumBridge::loadSettings()')
b=section((R/'src/bridge/DecodiumBridgeFontSettings.cpp').read_text(),'void DecodiumBridge::increaseFontScale()')
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-fontscale-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_fontscale_harness.cpp').read_text());results=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1] or results[0].splitlines()!=['118|1','108|2','200|3','50|4']:raise RuntimeError('unexpected '+repr(results))
 print(results[0],end='')
