#!/usr/bin/env python3
"""Compare decode UI worker on synthetic rows and controlled collaborators."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'struct DecodeUiClassifiedSource','static QVariantMap decodeDeltaBoundaryEntry(')+sec(b,'static DecodeUiSnapshotResult prepareDecodeUiSnapshot(', '} // namespace')
s=(R/'src/bridge/DecodiumBridgeDecodeUiSnapshot.cpp').read_text();new=sec(s,'struct DecodeUiClassifiedSource','} // namespace')
for x,y in [('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey('),('decodiumBridgeDecodeTelemetryOnly(', 'decodeListModel_isTelemetryOnlyMessage('),('decodiumBridgeNormalizeDecodeEntries(', 'normalizeDecodeEntriesForDisplay('),('decodiumBridgeDecodeRxClearEntryKey(', 'decodeRxClearEntryKey('),('decodiumBridgeCoalesceRxPaneTxRows(', 'coalesceRxPaneTxRows(')]:new=new.replace(x,y)
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-decodeuisnapshot-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_decodeuisnapshot_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 expected=['normal|19|7|A,SEP,B,TX|A,SEP,B,TX|A','reverse|T,TX,B,A|A,T','key|1|0']
 if results[0].splitlines()!=expected:raise RuntimeError('unexpected results '+results[0])
 print(results[0],end='')
