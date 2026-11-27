#!/usr/bin/env python3
"""Run original and extracted launcher against temporary terminal stubs."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def section(s):return s[s.index('// ── Decodium RX dal menu'):]
old=section(run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout)
new=section((R/'src/bridge/DecodiumBridgeRxTerminal.cpp').read_text()).replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-rxterminal-ab-') as tmp:
 results=[]
 for i,code in enumerate((old,new)):
  d=Path(tmp)/str(i);d.mkdir();(d/'test.cpp').write_text((R/'tests/bridge_refactor_rxterminal_harness.cpp').read_text());(d/'bridge_refactor_under_test.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('A/B mismatch')
 if results[0].splitlines()!=['missing|0','missing-error|1','no-terminal|1|1','launch|1|1|1|1','fallback|1|1']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
