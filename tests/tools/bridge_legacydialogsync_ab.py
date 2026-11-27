#!/usr/bin/env python3
"""Compare legacy dialog state synchronization with a recording backend."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::syncLegacyBackendDialogState()','void DecodiumBridge::syncLegacyBackendTxState()')
new=sec((R/'src/bridge/DecodiumBridgeLegacyDialogSync.cpp').read_text(),'void DecodiumBridge::syncLegacyBackendDialogState()')
for x,y in [('decodiumBridgeLegacyTxAttenuationFromLevel(', 'legacyTxAttenuationFromLevel('),('decodiumBridgeIsFt2LinkApplicationMode(', 'isFt2LinkApplicationMode('),('decodiumBridgeIsStreamingKeyboardMode(', 'isStreamingKeyboardMode('),('decodiumBridgeAudioLevelLog(', 'bridgeLog('),('decodiumBridgeSpecialOpHound()', 'kSpecialOpHound')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-legacydialogsync-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_legacydialogsync_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 lines=results[0].splitlines()
 if lines[0]!='unavailable|0' or not lines[1].startswith('ft8|identity=K1ABC:FN42,') or 'setAudioInputChannel=3' not in lines[1] or 'setAudioOutputChannel=0' not in lines[1] or 'selectTxMessage=6' not in lines[1] or lines[2:]!=['ft2|1|1','rtty|1|1|0','hound|1|1|1']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
