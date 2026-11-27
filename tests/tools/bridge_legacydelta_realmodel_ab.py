#!/usr/bin/env python3
"""Compare extracted legacy deltas against the real Qt DecodeListModel."""
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=ROOT,stdout=subprocess.PIPE).stdout
old=sec(b,'static QVariantMap decodeDeltaBoundaryEntry(', 'static DecodeUiSnapshotResult prepareDecodeUiSnapshot(')+sec(b,'void DecodiumBridge::invalidateLegacyDecodeModelDeltaFastPath()', 'void DecodiumBridge::syncLegacyBackendDecodeList()')
new=sec((ROOT/'src/bridge/DecodiumBridgeLegacyDecodeDelta.cpp').read_text(),'static QVariantMap decodeDeltaBoundaryEntry(')
if old!=new:raise RuntimeError('legacy delta source differs')
h=(ROOT/'tests/bridge_refactor_legacydelta_harness.cpp').read_text();a=h.index('class DecodeListModel {');z=h.index('class DecodiumBridge',a);h=h[:a]+'#include "DecodeListModel.h"\n#include "DecodiumLogging.hpp"\nvoid DecodiumLogging::diag(DiagCategory,const QString&) {}\n'+h[z:]
h=h.replace('band.rows={row("120000","old")};full.rows=band.rows;rx.rows=band.rows;', 'band.setEntries({row("120000","old")});full.setEntries({row("120000","old")});rx.setEntries({row("120000","old")});')
h=h.replace('if(!b.applyLegacyBandModelDelta({row("120015","new")})||band.count()!=3', 'if(!b.applyLegacyBandModelDelta({row("120015","new")}))return 20;pump(80);if(band.count()!=3')
h=h.replace('if(!b.applyLegacyRxModelDelta({row("120015","rx")})||rx.count()!=3', 'if(!b.applyLegacyRxModelDelta({row("120015","rx")}))return 21;pump(80);if(rx.count()!=3')
h=h.replace('band.rows={row("120030","old")};full.rows=band.rows;', 'band.setEntries({row("120030","old")});full.setEntries({row("120030","old")});')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
moc=run('qtpaths','--query','QT_INSTALL_LIBEXECS',stdout=subprocess.PIPE).stdout.strip()+'/moc'
with tempfile.TemporaryDirectory(prefix='decodium-realmodel-ab-') as t:
 d=Path(t);out=[];run(moc,str(ROOT/'src/models/DecodeListModel.h'),'-o',str(d/'moc_model.cpp'))
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);(d/'test.cpp').write_text(h)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(ROOT/'src/models'),'-I'+str(ROOT/'src/bridge'),str(d/'test.cpp'),str(ROOT/'src/models/DecodeListModel.cpp'),str(d/'moc_model.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('real model output differs')
 print('real DecodeListModel: '+out[1],end='')
