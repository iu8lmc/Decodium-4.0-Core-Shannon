#!/usr/bin/env python3
"""Compare spectrum and TX priority behavior, including simulated Windows APIs."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::suspendNonAudioTxWork(', 'bool DecodiumBridge::noteTxPlaybackFinished(')
new=sec((R/'src/bridge/DecodiumBridgeTxScheduling.cpp').read_text(),'void DecodiumBridge::suspendNonAudioTxWork(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('TX scheduling source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-txscheduling-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_txscheduling_harness.cpp').read_text())
 for variant in ([],['-DSIMULATE_WINDOWS']):
  out=[]
  for code in (old,new):
   (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',*variant,str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
  if out[0]!=out[1]:raise RuntimeError('TX scheduling outputs differ')
  print(('simulated Windows: ' if variant else 'host: ')+out[0],end='')
