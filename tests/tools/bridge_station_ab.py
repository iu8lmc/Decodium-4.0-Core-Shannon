#!/usr/bin/env python3
"""Compare station setters and TX-message preparation with mock collaborators."""
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=ROOT,stdout=subprocess.PIPE).stdout
full=sec(b,'void DecodiumBridge::setCallsign(const QString& v)', 'double DecodiumBridge::frequency() const')
new=sec((ROOT/'src/bridge/DecodiumBridgeStation.cpp').read_text(),'void DecodiumBridge::setCallsign(const QString& v)')
for x,y in [('decodiumBridgeNormalizeWsprPowerDbm(', 'normalizeWsprPowerDbm('),('decodiumBridgeWsprPowerDbmLabel(', 'wsprPowerDbmLabel('),('decodiumBridgeCustomCqPrefix(', 'customCqPrefixFromMessage('),('decodiumBridgeFtxCqWithPrefix(', 'ftxCqMessageWithCustomPrefix('),('decodiumBridgeStandardFtxCq(', 'standardFtxCqMessage('),('decodiumBridgeStandardFtxQso(', 'standardFtxQsoMessage('),('decodiumBridgeStationLog(', 'bridgeLog(')]:new=new.replace(x,y)
if full!=new:raise RuntimeError('station source differs')
def stripmoon(s):return sec(s,'void DecodiumBridge::setCallsign(const QString& v)','void DecodiumBridge::refreshMapMoonOverlay()')+sec(s,'QString DecodiumBridge::grid() const')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-station-ab-') as t:
 d=Path(t);h=(ROOT/'tests/bridge_refactor_station_harness.cpp').read_text();out=[]
 for code in (full,new):
  (d/'bridge_refactor_under_test.inc').write_text(stripmoon(code));(d/'test.cpp').write_text(h)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('station output differs')
 print(out[1],end='')
