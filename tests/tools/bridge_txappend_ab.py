#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a):i=s.index(a);j=s.index('\n}',i);return s[i:j+2]+'\n'
a=sec(run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout,'void DecodiumBridge::appendTxDecodeEntry(');b=sec((R/'src/bridge/DecodiumBridgeDecodeRxAppend.cpp').read_text(),'void DecodiumBridge::appendTxDecodeEntry(').replace('decodiumBridgeCurrentTxVisualTimeToken(', 'currentTxVisualTimeToken(').replace('decodiumBridgeDecodeEntryTimestampMsecs(', 'decodeEntryTimestampMsecs(')
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-txappend-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_txappend_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or out[0].splitlines()!=['empty|0','report|4|-08|1|K1ABC IU8LMC -08|1|1|1|123','cq|1|K1ABC IU8LMC -08|1']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
