#!/usr/bin/env python3
"""Compare PSK bridge decisions with a local mock reporter (no network)."""
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=ROOT,stdout=subprocess.PIPE).stdout
old=sec(b,'bool DecodiumBridge::pskReporterConnected()', 'void DecodiumBridge::setFtThreads(int v)')+sec(b,'QString DecodiumBridge::pskReporterProgramInfo()', 'void DecodiumBridge::setCallsign(')
new=sec((ROOT/'src/bridge/DecodiumBridgePskReporter.cpp').read_text(),'bool DecodiumBridge::pskReporterConnected()')
for x,y in [('decodiumBridgeUnresolvedGridOnlyDecode(', 'messageIsUnresolvedGridOnlyDecode('),('decodiumBridgeFst4wUnresolvedHashDecode(', 'isFst4wUnresolvedHashDecode('),('decodiumBridgePskDebugEnabled(', 'decodiumPskDebugEnabled('),('decodiumBridgeExtractRightCallsign(', 'extractRightCallsign('),('decodiumBridgeSecondsFromUtcDisplayToken(', 'secondsFromUtcDisplayToken(')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('PSK bridge source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-psk-ab-') as t:
 d=Path(t);h=(ROOT/'tests/bridge_refactor_psk_harness.cpp').read_text();out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);(d/'test.cpp').write_text(h)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('PSK output differs')
 print(out[1],end='')
