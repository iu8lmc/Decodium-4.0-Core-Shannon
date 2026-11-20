#!/usr/bin/env python3
"""Compare CW CAT/PTT port reservations using real serial registry and synthetic devices."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def section(s):
 a=s.index('bool DecodiumBridge::cwRigKeyerAvailable() const');b=s.find('bool DecodiumBridge::cwSendRigMorse(',a);return s[a:b if b>=0 else len(s)]
old=section(run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout);new=section((R/'src/bridge/DecodiumBridgeCwPorts.cpp').read_text())
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-cwports-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_cwports_registry_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/net'),str(d/'test.cpp'),str(R/'src/net/DecodiumSerialPortRegistry.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('A/B mismatch')
 if results[0].splitlines()!=['keyer|1|0','lease|1','cat|1','ptt|1','ptt-cat|1','network|1','alias|1']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
