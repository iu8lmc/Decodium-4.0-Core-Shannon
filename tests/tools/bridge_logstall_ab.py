#!/usr/bin/env python3
"""Compare QSO UI-stall probe with real Qt events and a synthetic delay."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def section(s):
 a=s.index('// Sonda delle pause');b=s.find('void DecodiumBridge::logQsoNow()',a);return s[a:b if b>=0 else len(s)]
old=section(run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout);new=section((R/'src/bridge/DecodiumBridgeLogStallProbe.cpp').read_text()).replace('void decodiumBridgeStartLogUiStallProbe(', 'static void startLogUiStallProbe(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-logstall-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_logstall_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1] or results[0].splitlines()!=['reports|1|1|2|1']:raise RuntimeError('unexpected '+str(results))
 print(results[0],end='')
