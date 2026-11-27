#!/usr/bin/env python3
"""Compare mode-change audio recovery with real Qt timers and fake backends."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::restartAudioCaptureForModeChange(', 'void DecodiumBridge::scheduleRttyExitRxRecovery()')+sec(b,'void DecodiumBridge::scheduleMonitorRecovery(', 'void DecodiumBridge::restartAudioCaptureFromWatchdog(')
new=sec((R/'src/bridge/DecodiumBridgeAudioRecovery.cpp').read_text(),'void DecodiumBridge::restartAudioCaptureForModeChange(', '\nvoid DecodiumBridge::restartAudioCaptureFromWatchdog(')
for x,y in [('decodiumBridgeAudioLevelLog(', 'bridgeLog('),('decodiumBridgeRxInputGainFromLevel(', 'rxInputGainFromLevel('),('decodiumBridgeIsNativePcmKeyboardMode(', 'isNativePcmKeyboardMode(')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('audio recovery source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-audiorecovery-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_audiorecovery_harness.cpp').read_text());out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('audio recovery results differ')
 print(out[0],end='')
