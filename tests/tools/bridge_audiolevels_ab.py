#!/usr/bin/env python3
"""Compare audio level bridge routing with substituted audio and legacy sinks."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'double DecodiumBridge::audioLevel() const','QStringList DecodiumBridge::audioInputDevices() const')
new=sec((R/'src/bridge/DecodiumBridgeAudioLevels.cpp').read_text(),'double DecodiumBridge::audioLevel() const')
for a,c in [('decodiumBridgeAudioLevelLog(', 'bridgeLog('),('decodiumBridgeTxAttenuationFromSlider(', 'txAttenuationFromSlider('),('decodiumBridgeRxInputGainFromLevel(', 'rxInputGainFromLevel('),('decodiumBridgeTxGainFromSlider(', 'txGainFromSlider('),('decodiumBridgeLegacyTxAttenuationFromLevel(', 'legacyTxAttenuationFromLevel(')]:new=new.replace(a,c)
if old!=new:raise RuntimeError('audio levels source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-audiolevels-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_audiolevels_harness.cpp').read_text());out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('audio level outputs differ')
 print(out[0],end='')
