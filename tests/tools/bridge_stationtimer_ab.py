#!/usr/bin/env python3
"""Compare slot timing and deferred telemetry guard behavior without radio."""
import os, shlex, subprocess, tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw): return subprocess.run(a,check=True,text=True,**kw)
def section(s):
 start=s.index('void DecodiumBridge::armStationTelemetryForNextSlot(')
 end=s.find('// ── Decodium RX dal menu',start)
 if end<0:end=s.find('using decodium::seq::isGridTokenStrict;',start)
 return s[start:end if end>=0 else len(s)]
baseline=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=section(baseline)
new=section((R/'src/bridge/DecodiumBridgeStationTelemetry.cpp').read_text()).replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(').replace('decodiumBridgeTxPeriodIsEven(', 'bridgeTxPeriodIsEven(')
if old!=new: raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-stationtimer-ab-') as tmp:
 d=Path(tmp); (d/'test.cpp').write_text((R/'tests/bridge_refactor_stationtimer_harness.cpp').read_text()); results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]: raise RuntimeError('A/B mismatch')
 expected=['even|sendStationTelemetry: rimandata al prossimo slot TX (even) fra 215 ms|1','odd|sendStationTelemetry: rimandata al prossimo slot TX (odd) fra 115 ms','fallback|sendStationTelemetry: rimandata al prossimo slot TX (fallback) fra 1000 ms','forced|2|1','guards|0|0']
 if results[0].splitlines()!=expected: raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
