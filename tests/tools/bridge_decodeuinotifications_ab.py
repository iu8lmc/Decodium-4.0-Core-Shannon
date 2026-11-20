#!/usr/bin/env python3
"""Compare coalesced notifications with real Qt timers."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::emitDecodeListChangedThrottled()','DecodiumBridge::DecodiumBridge(QObject* parent)')
new=sec((R/'src/bridge/DecodiumBridgeDecodeUiNotifications.cpp').read_text(),'void DecodiumBridge::emitDecodeListChangedThrottled()')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-decodeuinotifications-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_decodeuinotifications_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 expected=['queued|500|250|1|1','rx|0|1|0','band|1|1|1|1','requeue|3|3|3']
 if results[0].splitlines()!=expected:raise RuntimeError('unexpected results '+results[0])
 print(results[0],end='')
