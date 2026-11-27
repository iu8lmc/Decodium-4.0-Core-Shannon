#!/usr/bin/env python3
"""Compare cache descriptions and signatures with simulated device metadata."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b):i=s.index(a);return s[i:s.index(b,i)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'static QStringList audioDeviceDescriptions(', 'static inline void activeCatSetFreq(')
s=(R/'src/bridge/DecodiumBridgeAudioDeviceCacheRefresh.cpp').read_text()
new=sec(s,'static QStringList audioDeviceDescriptions(', 'void DecodiumBridge::refreshAudioDeviceCache(').replace('decodiumCacheDeviceId(', 'audioDeviceIdForLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-audiocachehelpers-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_audiocachehelpers_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 expected=['names|Mic|Speaker','signature|di=01;do=03;i=Mic|01;i=mic|02;i=|<empty>;o=Speaker|03','empty|0|di=<empty>;do=<empty>']
 if results[0].splitlines()!=expected:raise RuntimeError('unexpected results '+results[0])
 print(results[0],end='')
