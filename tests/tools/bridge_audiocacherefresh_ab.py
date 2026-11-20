#!/usr/bin/env python3
"""Compare audio device refresh with simulated Qt and Linux devices."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::refreshAudioDeviceCache(','QList<QAudioDevice> DecodiumBridge::cachedAudioInputs(')
new=sec((R/'src/bridge/DecodiumBridgeAudioDeviceCacheRefresh.cpp').read_text(),'void DecodiumBridge::refreshAudioDeviceCache(')
for x,y in [('decodiumCacheDescriptions(','audioDeviceDescriptions('),('decodiumCacheSignature(','audioDeviceCacheSignature('),('decodiumCacheDeviceId(','audioDeviceIdForLog('),('decodiumCacheAudioFormat(','audioFormatForLog('),('decodiumCachePulseSources(','linuxPulseSourcesForLog('),('decodiumCachePulseSinks(','linuxPulseSinksForLog('),('decodiumCacheAlsaCards(','linuxAlsaCardsForLog('),('decodiumCacheLooksUsb(','audioDeviceLooksUsb('),('decodiumCacheSourceName(','linuxPulseSourceNameFromShortLine('),('decodiumCacheSourceDisplay(','linuxPulseSourceDisplayName('),('decodiumCacheSinkName(','linuxPulseSinkNameFromShortLine('),('decodiumCacheSinkPrefix()','kLinuxPulseSinkPrefix'),('decodiumBridgeAudioLevelLog(','bridgeLog(')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-audiocacherefresh-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_audiocacherefresh_harness.cpp').read_text())
 for linux in (False,True):
  results=[]
  for code in (old,new):
   (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',*(['-DSIMULATE_LINUX'] if linux else []),str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout)
  if results[0]!=results[1]:raise RuntimeError('results differ')
  first='first|Mic,pulse:source1,pulse:source2|Speaker,pulse:sink1|1|1|0|1|0' if linux else 'first|Mic|Speaker|1|1|0|1|0'
  expected=[first,'same|1|1|1','changed-silent|1|1|0|2|1','unchanged|1|1']
  if results[0].splitlines()!=expected:raise RuntimeError('unexpected results '+results[0])
  print(('linux-simulated' if linux else 'normal')+'\n'+results[0],end='')
