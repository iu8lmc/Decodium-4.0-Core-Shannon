#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a):i=s.index(a);j=s.index('\n}',i);return s[i:j+2]+'\n'
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;t=(R/'src/bridge/DecodeChronology.cpp').read_text();names=['static qint64 decodeDisplaySortMsecsFromTime(', 'static qint64 decodeDisplaySortMsecs(', 'static void sortDecodeEntriesChronologically('];a='\n'.join(sec(s,n) for n in names);b='\n'.join(sec(t,n.replace('static void sort','void sort')) for n in names).replace('void sortDecodeEntriesChronologically(', 'static void sortDecodeEntriesChronologically(')
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-chronology-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_chronology_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),str(R/'src/bridge/DecodeUtcTime.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 (d/'bridge_refactor_under_test.inc').write_text('#include "DecodeChronology.h"\n#define decodeDisplaySortMsecs decodiumBridgeDecodeDisplaySortMsecs\n');run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),str(R/'src/bridge/DecodeUtcTime.cpp'),str(R/'src/bridge/DecodeChronology.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or out[0]!=out[2] or out[0].splitlines()!=['midnight|1|1|1','sort|AABZ|1500']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
