#!/usr/bin/env python3
"""Compare cache access and refresh scheduling using an actual Qt event loop."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::scheduleAudioDeviceRefresh(','void DecodiumBridge::refreshAudioDeviceCache(')+sec(b,'QList<QAudioDevice> DecodiumBridge::cachedAudioInputs(','QAudioDevice DecodiumBridge::resolveRxInputDevice(')+sec(b,'void DecodiumBridge::enumerateAudioDevices()','// ============================================================')
new=sec((R/'src/bridge/DecodiumBridgeAudioDeviceCacheAccess.cpp').read_text(),'void DecodiumBridge::scheduleAudioDeviceRefresh(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-audiocacheaccess-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_audiocacheaccess_harness.cpp').read_text());out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('results differ')
 expected=['sync|1|sync|1|cached-in','scheduled|0|cached-out','fired|1|single-shot device refresh|0','default|preferred-in|system-out|0','verbose|2|0','timer|1|0|1','shutdown|0|0','enumerate|1|enumerateAudioDevices|1|1']
 if out[0].splitlines()!=expected:raise RuntimeError('unexpected behavior '+out[0])
 print(out[0],end='')
