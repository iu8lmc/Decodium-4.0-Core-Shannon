#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a):i=s.index(a);j=s.index('\n}',i);return s[i:j+2]+'\n'
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;t=(R/'src/bridge/DecodiumQsoSnapshot.cpp').read_text();names=['static QVariantMap qsoMapFromAdifRecord(', 'static QVariantMap qsoStatsFromRows(', 'static QString exportRecordFingerprint(', 'static QsoLogSnapshot buildQsoLogSnapshot('];a='\n'.join(sec(s,n).replace('static ', '',1) if i else sec(s,n) for i,n in enumerate(names));b='\n'.join(sec(t,n if i==0 else n.replace('static ', '',1)) for i,n in enumerate(names))
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-qsosnapshot-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_qsosnapshot_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),str(R/'src/bridge/AdifDocumentInternal.cpp'),str(R/'src/bridge/AdifExportSanitizer.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or out[0].splitlines()!=['rows|2|K2XYZ|K1ABC','stats|2|2|7000|K1ABC','export|1|1','missing|0|0']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
