#!/usr/bin/env python3
"""Compare backend ownership for simulated macOS and other platforms."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'bool DecodiumBridge::usingLegacyBackendForTx() const','void DecodiumBridge::ensureDecodeWorkerForMode(')+sec(b,'bool DecodiumBridge::useModernSpectrumFeedWithLegacy() const','bool DecodiumBridge::shouldIgnoreDecodeCallbacks() const')
new=sec((R/'src/bridge/DecodiumBridgeLegacyOwnership.cpp').read_text(),'bool DecodiumBridge::usingLegacyBackendForTx() const')
for x,y in [('decodiumBridgeIsFt2LinkApplicationMode(', 'isFt2LinkApplicationMode('),('decodiumBridgeIsStreamingKeyboardMode(', 'isStreamingKeyboardMode('),('decodiumBridgeOwnsLegacyJtRxWhenCatSuppressed(', 'bridgeOwnsLegacyJtRxWhenCatSuppressed('),('decodiumBridgeOwnsFst4Audio(', 'bridgeOwnsFst4Audio(')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-legacyownership-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_legacyownership_harness.cpp').read_text())
 for mac in (False,True):
  results=[]
  for code in (old,new):
   (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',*(['-DSIMULATE_MAC'] if mac else []),str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
  if results[0]!=results[1]:raise RuntimeError('results differ')
  lines=results[0].splitlines()
  if lines[0]!='ft8|1|1|1|0|0|1' or lines[1]!='q65|0|0|0|0|0|0' or lines[2]!='rtty|0|0|0|0|0|0' or lines[5] != ('ft2link|0|1|1|1|0|1' if mac else 'ft2link|0|0|0|0|0|0') or lines[7]!='rtl|0|0|0|0|0|0' or lines[9]!='dual|1|1|1|1|1|0':raise RuntimeError('unexpected '+results[0])
  print(('mac-simulated' if mac else 'other')+'\n'+results[0],end='')
