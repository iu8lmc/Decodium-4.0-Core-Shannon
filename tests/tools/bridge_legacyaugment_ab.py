#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a):i=s.index(a);j=s.index('\n}',i);return s[i:j+2]+'\n'
a=sec(run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout,'QVariantList DecodiumBridge::augmentLegacyMirrorWithAllTxt(');b=sec((R/'src/bridge/DecodiumBridgeLegacyAugment.cpp').read_text(),'QVariantList DecodiumBridge::augmentLegacyMirrorWithAllTxt(').replace('decodiumBridgeDecodeFlowLog(', 'bridgeLog(').replace('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey(').replace('decodiumBridgeNormalizeRemoteActivityTime(', 'normalizeUtcDisplayToken(')
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-legacyaugment-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_legacyaugment_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),str(R/'src/bridge/LegacyDecodeText.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 for name in ('DecodiumBridge.h','DecodeUserFilterConfig.h','DecodeMessageNormalization.h'):(d/name).write_text('#pragma once\n')
 (d/'production.cpp').write_text((R/'src/bridge/DecodiumBridgeLegacyAugment.cpp').read_text())
 actual=(R/'tests/bridge_refactor_legacyaugment_harness.cpp').read_text().replace('QString stripDecodeApAnnotation(QString s,QString* p){if(p)p->clear();return s;}','').replace('#include "bridge_refactor_under_test.inc"','namespace decodium::decode_message { inline QString stripDecodeApAnnotation(QString s,QString* p){if(p)p->clear();return s;} }\nvoid decodiumBridgeDecodeFlowLog(const QString& s){bridgeLog(s);} QString decodiumBridgeMirrorEntryKey(const QVariantMap& m){return decodeMirrorEntryKey(m);} QString decodiumBridgeNormalizeRemoteActivityTime(QString s){return normalizeUtcDisplayToken(s);}\n#include "production.cpp"')
 (d/'test.cpp').write_text(actual);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(d),'-I'+str(R/'src/bridge'),str(d/'test.cpp'),str(R/'src/bridge/LegacyDecodeText.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or out[0]!=out[2] or out[0].splitlines()!=['increment|0|1|1|CQ K1ABC FN42','truncate|1|1','rx|0','upgrade|1|120030','filtered|1|CQ K1ABC FN42','msk|1|120045','other-mode|1']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
