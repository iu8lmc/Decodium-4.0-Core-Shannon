#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,n):i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;new=(R/'src/bridge/DecodeTokenClassifier.cpp').read_text();names=['bool isGridToken(', 'bool isTelemetryHexToken(', 'bool looksLikeCallsignToken('];a='\n'.join(sec(old,n) for n in names);b='\n'.join(sec(new,n) for n in names)
if a!=b:raise RuntimeError('token classifier source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-tokenclass-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_tokenclass_harness.cpp').read_text());out=[]
 for baseline in (True,False):
  (d/'bridge_refactor_tokenclass.inc').write_text(a if baseline else '#include "DecodeTokenClassifier.h"\nusing namespace decodium::decode_tokens;\n')
  files=[str(d/'test.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp')]
  if not baseline:files.append(str(R/'src/bridge/DecodeTokenClassifier.cpp'))
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),*files,*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or 'FN42|1|0|0' not in out[0] or 'RR73|0|0|0' not in out[0] or '1234ABCD|0|1|0' not in out[0]:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
