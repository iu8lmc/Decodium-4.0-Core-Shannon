#!/usr/bin/env python3
"""Compare CW monitor settings with real isolated QSettings and output enumeration."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def section(s):
 a=s.index('bool DecodiumBridge::cwMonitorEnabled()');b=s.find('void DecodiumBridge::testCwMonitor()',a);return s[a:b if b>=0 else len(s)]
old=section(run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout)
new=section((R/'src/bridge/DecodiumBridgeCwMonitorSettings.cpp').read_text())
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Multimedia',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-cwsettings-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_cwsettings_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('A/B mismatch')
 if results[0].splitlines()!=['default|1|35|1','set|0|100|1|1|1|3','persist|0|1|1|5','outputs|1|1|1']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
