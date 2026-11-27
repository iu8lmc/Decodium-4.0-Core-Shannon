#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a):i=s.index(a);j=s.index('\n}',i);return s[i:j+2]+'\n'
a=sec(run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout,'void DecodiumBridge::rebuildRxDecodeList()');b=sec((R/'src/bridge/DecodiumBridgeDecodeRxAppend.cpp').read_text(),'void DecodiumBridge::rebuildRxDecodeList()')
for new,old in [('decodiumBridgeDecodeRxClearEntryKey(', 'decodeRxClearEntryKey('),('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey('),('decodiumBridgeCoalesceRxPaneTxRows(', 'coalesceRxPaneTxRows('),('decodiumBridgeNormalizeDecodeEntries(', 'normalizeDecodeEntriesForDisplay(')]:b=b.replace(new,old)
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-rxrebuild-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_rxrebuild_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or out[0].splitlines()!=['rebuild|2|a|b|1|1','stable|1|1','legacy|2|1']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
