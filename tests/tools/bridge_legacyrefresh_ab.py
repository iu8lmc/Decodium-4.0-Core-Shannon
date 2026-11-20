#!/usr/bin/env python3
"""Compare legacy refresh scheduling with Qt timers and controlled backend."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::syncLegacyBackendTxState()','void DecodiumBridge::migrateActiveMonitoringToLegacyBackend()')
new=sec((R/'src/bridge/DecodiumBridgeLegacyRefresh.cpp').read_text(),'void DecodiumBridge::syncLegacyBackendTxState()').replace('decodiumBridgeLegacyRigControlFallback(', 'useLegacyRigControlFallback(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-legacyrefresh-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_legacyrefresh_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 expected=['blocked|0|0','fallback|1','unavailable|0','started|1|1','burst|4','queued|1|4','resumed|0|8']
 if results[0].splitlines()!=expected:raise RuntimeError('unexpected results '+results[0])
 print(results[0],end='')
