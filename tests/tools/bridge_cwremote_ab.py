#!/usr/bin/env python3
"""Compare CW remote audio, keying and PTT with controlled collaborators."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
oldsrc=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=(sec(oldsrc,'bool DecodiumBridge::cwSendAudio(','bool DecodiumBridge::playCwMonitor(')+sec(oldsrc,'// Il PTT della radio remota','bool DecodiumBridge::registraQsoCw(')+sec(oldsrc,'void DecodiumBridge::cwAbortAudio()','void DecodiumBridge::setSpectrumVisible('));new=sec((R/'src/bridge/DecodiumBridgeCwRemote.cpp').read_text(),'bool DecodiumBridge::cwSendAudio(', 'bool DecodiumBridge::cwCanTransmit()').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-cwremote-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_cwremote_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('A/B mismatch')
 if results[0].splitlines()!=['local|0|0','supported|1|150','busy|0','send|1|1|1','failed|0|1','ptt|1|1|1|0','blocked-audio|0|0','audio|1|1|2','abort|1|2']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
