#!/usr/bin/env python3
"""Compare synthetic weather replies with real JSON and telemetry codec."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def section(s):
 a=s.index('void DecodiumBridge::fetchWeatherForGrid()');b=s.find('bool DecodiumBridge::ft2LinkSatelliteHalfDuplexRequested()',a);return s[a:b if b>=0 else len(s)]
old=section(run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout);new=section((R/'src/bridge/DecodiumBridgeStationWeather.cpp').read_text()).replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Network',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-stationweather-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_stationweather_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/radio'),str(d/'test.cpp'),str(R/'src/radio/StationTelemetryCodec.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('A/B mismatch')
 if results[0].splitlines()!=['guards|0|0','request|1|1|1|1','network-error|0|1','json-error|0|1','weather|1|19|22|1|1','stale|0']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
