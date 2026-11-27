#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
a=sec(s,'static QStringList readTextLinesFromOffset(', 'struct HashResolveAllTxtCandidate');b=sec((R/'src/bridge/LegacyDecodeText.cpp').read_text(),'QStringList readTextLinesFromOffset(');a=a.replace('static QStringList readTextLinesFromOffset(', 'QStringList readTextLinesFromOffset(').replace('static QStringList readTextTailLines(', 'QStringList readTextTailLines(')
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-legacytext-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_legacytext_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 (d/'test.cpp').write_text((R/'tests/bridge_refactor_legacytext_harness.cpp').read_text().replace('#include "bridge_refactor_under_test.inc"', '#include "LegacyDecodeText.h"'));run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),str(R/'src/bridge/LegacyDecodeText.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or out[0]!=out[2] or out[0].splitlines()!=['all|first,second,third,fourth','last2|third,fourth','offset|second,third,fourth','end|','tail|third,fourth','tail1|fourth','missing|','invalid|']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
