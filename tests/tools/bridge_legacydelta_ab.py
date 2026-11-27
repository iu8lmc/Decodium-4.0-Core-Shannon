#!/usr/bin/env python3
"""A/B compare legacy incremental update logic with Qt timer execution."""
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=ROOT,stdout=subprocess.PIPE).stdout
old=sec(b,'static QVariantMap decodeDeltaBoundaryEntry(', 'static DecodeUiSnapshotResult prepareDecodeUiSnapshot(')+sec(b,'void DecodiumBridge::invalidateLegacyDecodeModelDeltaFastPath()', 'void DecodiumBridge::syncLegacyBackendDecodeList()')
new=sec((ROOT/'src/bridge/DecodiumBridgeLegacyDecodeDelta.cpp').read_text(),'static QVariantMap decodeDeltaBoundaryEntry(')
if old!=new:raise RuntimeError('legacy delta source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-delta-ab-') as t:
 d=Path(t);h=(ROOT/'tests/bridge_refactor_legacydelta_harness.cpp').read_text();out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);(d/'test.cpp').write_text(h)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('legacy delta output differs')
 print(out[1],end='')
