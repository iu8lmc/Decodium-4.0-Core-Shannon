#!/usr/bin/env python3
"""Compare actual baseline and extracted log-prompt lifecycle with Qt events."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'bool DecodiumBridge::promptToLogEnabled() const','void DecodiumBridge::impostaModoRadioRtty(')+sec(b,'void DecodiumBridge::logQso()','void DecodiumBridge::capturePromptLogSnapshot(')
new=sec((R/'src/bridge/DecodiumBridgeLogPromptLifecycle.cpp').read_text(),'bool DecodiumBridge::promptToLogEnabled() const').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-promptlifecycle-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_prompt_lifecycle_harness.cpp').read_text());outputs=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));outputs.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if outputs[0]!=outputs[1]:raise RuntimeError('results differ')
 expected=['empty|No QSO to log|0','tx|Finish TX before logging the QSO|0','reject|1|1|0|K1ABC|4|1|0','fallback|1|1','accept|1|1|0','native|1|0','direct|1|0','pending|K4ABC|1','late|K5ABC|1','already|0|0']
 if outputs[0].splitlines()!=expected:raise RuntimeError('unexpected behavior '+outputs[0])
 print(outputs[0],end='')
