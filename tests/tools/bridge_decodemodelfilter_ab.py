#!/usr/bin/env python3
"""Compare actual model filter code with deterministic collaborators."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'bool DecodiumBridge::entryBelongsToCurrentQso(', 'void DecodiumBridge::rebuildBandActivityModel()')
new=sec((R/'src/bridge/DecodiumBridgeDecodeModelFilter.cpp').read_text(),'bool DecodiumBridge::entryBelongsToCurrentQso(')
for x,y in [('decodiumBridgeAudioLevelLog(', 'bridgeLog('),('decodiumBridgeDecodeRxClearEntryKey(', 'decodeRxClearEntryKey('),('decodiumBridgeCoalesceRxPaneTxRows(', 'coalesceRxPaneTxRows('),('decodiumBridgeDecodeDisplaySortMsecs(', 'decodeDisplaySortMsecs(')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-decodemodelfilter-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_decodemodelfilter_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 expected=['band|A,SEP:120015,B,TX,SEP:120030,C','reverse|C,SEP:120015,TX,B,SEP:120000,A','rx|A,SEP:120030,C','nosep|A,B,TX,C','timestamp|X,SEP:,Y','empty|0']
 if results[0].splitlines()!=expected:raise RuntimeError('unexpected results '+results[0])
 print(results[0],end='')
