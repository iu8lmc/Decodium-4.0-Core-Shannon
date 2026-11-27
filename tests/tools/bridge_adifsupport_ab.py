#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a):i=s.index(a);j=s.index('\n}',i);return s[i:j+2]+'\n'
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;t=(R/'src/bridge/DecodiumAdifSupport.cpp').read_text();names=['static void rebuildWorkedCallsFromDocument(', 'static int countAdifRecordsLightweight(', 'static void setAdifModeFields('];a='\n'.join(sec(s,n).replace('static ', '',1) if i!=1 else sec(s,n) for i,n in enumerate(names));b='\n'.join(sec(t,n.replace('static ', '',1) if i!=1 else n) for i,n in enumerate(names))
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-adifsupport-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_adifsupport_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or out[0].splitlines()!=['count|3|0','worked|1|1|0','mode|ft4|MFSK|FT4','mode|q65-30|MFSK|Q65-30','mode|cw|CW|']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
