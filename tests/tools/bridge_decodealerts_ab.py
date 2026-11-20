#!/usr/bin/env python3
"""Compare decode alert decisions using an isolated settings profile."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::maybePlayDecodeAlert(','// ============================================================\n// WAV decode:')
new=sec((R/'src/bridge/DecodiumBridgeDecodeAlerts.cpp').read_text(),'void DecodiumBridge::maybePlayDecodeAlert(').replace('decodiumBridgeAudioLevelLog(','bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Widgets',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-decodealerts-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_decodealerts_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));env=os.environ.copy();env['QT_QPA_PLATFORM']='offscreen';results.append(run(str(d/'test'),stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 if results[0].splitlines()!=['mycall|MyCall,WantedCallsign','cq|MyCall,WantedCallsign,CQ','wanted|MyCall,WantedCallsign,CQ|K1ABC','different|4|K1XYZ','disabled|4']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
