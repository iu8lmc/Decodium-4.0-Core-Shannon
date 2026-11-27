#!/usr/bin/env python3
"""Compare legacy RX handoff with controlled audio/backend collaborators."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::migrateActiveMonitoringToLegacyBackend()','void DecodiumBridge::applyDirectVisualAudioCaptureMode(')
new=sec((R/'src/bridge/DecodiumBridgeLegacyRxHandoff.cpp').read_text(),'void DecodiumBridge::migrateActiveMonitoringToLegacyBackend()').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-legacyrxhandoff-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_legacyrxhandoff_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 expected=['guard|0|0','normal|1|0|1|1|1|1','dedicated|2|1|2|0','txguard|0|2','handoff|2|3|0|3','tap|1|4|4']
 if results[0].splitlines()!=expected:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
