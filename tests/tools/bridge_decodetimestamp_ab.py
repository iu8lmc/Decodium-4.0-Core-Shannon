#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a):i=s.index(a);j=s.index('\n}',i);return s[i:j+2]+'\n'
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;t=(R/'src/bridge/DecodeUtcTime.cpp').read_text();names=['static QDateTime approxUtcDateTimeForDisplayToken(', 'static qint64 decodeEntryTimestampMsecs('];a='\n'.join(sec(s,n) for n in names);b='\n'.join(sec(t,n) for n in names)
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-decodetimestamp-ab-') as tmp:
 d=Path(tmp);h=(R/'tests/bridge_refactor_decodetimestamp_harness.cpp').read_text();out=[]
 for code in (a,b):
  (d/'test.cpp').write_text(h.replace('#include "DecodeUtcTime.h"','QString decodiumBridgeNormalizeRemoteActivityTime(QString);\ninline QString normalizeUtcDisplayToken(QString s){return decodiumBridgeNormalizeRemoteActivityTime(s); }'));(d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),str(R/'src/bridge/DecodeUtcTime.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 (d/'test.cpp').write_text(h);(d/'bridge_refactor_under_test.inc').write_text('');run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),str(R/'src/bridge/DecodeUtcTime.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if len(set(out))!=1 or out[0].splitlines()!=['valid|1|1|1','fallback|1|1']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
