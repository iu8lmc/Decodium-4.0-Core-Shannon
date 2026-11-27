#!/usr/bin/env python3
"""Compare diagnostic overlay methods with real Qt timers and substitute bridge."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::recordFrameTimestamp()','void DecodiumBridge::emitDecodeListChangedThrottled()')
new=sec((R/'src/bridge/DecodiumBridgeDecodeMetrics.cpp').read_text(),'void DecodiumBridge::recordFrameTimestamp()').replace('decodiumBridgeOverlayLog(', 'bridgeLog(')
if old!=new:raise RuntimeError('overlay source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-overlay-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_overlay_harness.cpp').read_text());out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('overlay results differ')
 print(out[0],end='')
