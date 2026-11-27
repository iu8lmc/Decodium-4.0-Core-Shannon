#!/usr/bin/env python3
"""Compare input selection including simulated Linux Pulse paths."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'QAudioDevice DecodiumBridge::resolveRxInputDevice(','void DecodiumBridge::enumerateAudioDevices()')
new=sec((R/'src/bridge/DecodiumBridgeAudioInputSelection.cpp').read_text(),'QAudioDevice DecodiumBridge::resolveRxInputDevice(')
for x,y in [('decodiumBridgeResolveAudioDeviceByIdentity(', 'tryResolveAudioDeviceByIdentity('),('decodiumBridgeLinuxPulseSourceNameFromDisplay(', 'linuxPulseSourceNameFromDisplay('),('decodiumBridgeLinuxSetDefaultPulseSource(', 'linuxSetDefaultPulseSource('),('decodiumBridgeAudioLevelLog(', 'bridgeLog(')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-audioinputselection-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_audioinputselection_harness.cpp').read_text())
 for linux in (False,True):
  out=[]
  for code in (old,new):
   (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',*(['-DSIMULATE_LINUX'] if linux else []),str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout)
  if out[0]!=out[1]:raise RuntimeError('results differ')
  expected=['two|1|name','one|1|id','default|0|not-found','default|1|default','one|0|not-found']+(['pulse-default|1|linux-pulse-source','one|0|not-found'] if linux else [])+['null-ok']
  if out[0].splitlines()!=expected:raise RuntimeError('unexpected behavior '+out[0])
  print(('linux-simulated' if linux else 'normal')+'\n'+out[0],end='')
