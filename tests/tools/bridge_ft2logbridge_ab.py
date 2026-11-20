#!/usr/bin/env python3
"""Compare FT2 Log Bridge control with a simulated launcher; no process starts."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::setFt2LogBridgeStatus(','void DecodiumBridge::armStationTelemetryForNextSlot(')
new=sec((R/'src/bridge/DecodiumBridgeFt2LogBridge.cpp').read_text(),'void DecodiumBridge::setFt2LogBridgeStatus(').replace('decodiumBridgeAudioLevelLog(','bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-ft2logbridge-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_ft2logbridge_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 if results[0].splitlines()!=['status|1|0','disabled|0','missing|1','found|1','started|1|1','disabled-qso|2|1']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
