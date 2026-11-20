#!/usr/bin/env python3
"""Compare pressure guards using real Qt timers and the real stall policy."""
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=ROOT,stdout=subprocess.PIPE).stdout
old=sec(b,'bool DecodiumBridge::cpuPressureActive() const','int DecodiumBridge::effectiveFt4ThreadLimit() const')+sec(b,'int DecodiumBridge::effectiveSpectrumTimerIntervalMs() const','int DecodiumBridge::effectiveDecodeDepth() const')
new=sec((ROOT/'src/bridge/DecodiumBridgeCpuPressure.cpp').read_text(),'bool DecodiumBridge::cpuPressureActive() const')
for x,y in [('decodiumBridgeMaxFtDecodeThreads()','kMaxFtDecodeThreads'),('decodiumBridgeFtAutoUiReserveForCores(', 'ftAutoUiReserveForCores('),('decodiumBridgeAutoFtThreadCountForCores(', 'autoFtThreadCountForCores('),('decodiumBridgeAdaptiveInteractiveFtThreadCount(', 'adaptiveInteractiveFtThreadCount('),('decodiumBridgeApplyFtOpenMpThreadLimit(', 'applyFtOpenMpThreadLimit('),('decodiumBridgeCpuPressureLog(', 'bridgeLog(')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('pressure source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-pressure-ab-') as t:
 d=Path(t);h=(ROOT/'tests/bridge_refactor_cpupressure_harness.cpp').read_text();out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);(d/'test.cpp').write_text(h)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(ROOT/'src/core'),str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE,stderr=subprocess.DEVNULL).stdout)
 if out[0]!=out[1]:raise RuntimeError('pressure output differs')
 print(out[1],end='')
