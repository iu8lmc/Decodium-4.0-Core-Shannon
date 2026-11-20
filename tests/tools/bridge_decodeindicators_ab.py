#!/usr/bin/env python3
"""Compare decode indicator state, thresholds and notifications."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def section(s):
 a=s.index('void DecodiumBridge::resetLedStatus()');b=s.find('QString DecodiumBridge::diagnosticLogPath()',a);return s[a:b if b>=0 else len(s)]
old=section(run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout);new=section((R/'src/bridge/DecodiumBridgeDecodeIndicators.cpp').read_text())
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-indicators-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_decodeindicators_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('A/B mismatch')
 if results[0].splitlines()!=['set|111|3|1|9|111111','clear|000|0|0|0|222222','threshold|0|8','reset|000|0|333334']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
