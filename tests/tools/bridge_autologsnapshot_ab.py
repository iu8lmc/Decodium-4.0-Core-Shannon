#!/usr/bin/env python3
"""Compare QSO snapshot lifecycle with substitute callsign and report helpers."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'QString DecodiumBridge::inferredPartnerForAutolog() const','void DecodiumBridge::clearPendingAutoSeqTx(')+sec(b,'void DecodiumBridge::armLateAutoLogSnapshot()','bool DecodiumBridge::shouldPreserveDeferredAutoSeqTxForRearm(')
new=sec((R/'src/bridge/DecodiumBridgeAutoLogSnapshot.cpp').read_text(),'QString DecodiumBridge::inferredPartnerForAutolog() const')
for x,y in [('decodiumBridgeInferPartnerForAutolog(', 'inferPartnerFromDirectedMessage('),('decodiumBridgeLateAutoLogGraceSeconds()', 'kLateAutoLogGraceWindowSeconds'),('decodiumBridgeAudioLevelLog(', 'bridgeLog(')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('autolog snapshot source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-autologsnapshot-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_autologsnapshot_harness.cpp').read_text());out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('autolog snapshot results differ')
 print(out[0],end='')
