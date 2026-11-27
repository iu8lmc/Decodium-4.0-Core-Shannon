#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b):i=s.index(a);return s[i:s.index(b,i)]
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
a=sec(s,'bool DecodiumBridge::registraQsoCw(', '\n}',)+'\n}\n\n'+sec(s,'bool DecodiumBridge::registraQsoRtty(', '\n}')+'\n}\n';b=(R/'src/bridge/DecodiumBridgeKeyboardLog.cpp').read_text();b=b[b.index('bool DecodiumBridge::registraQsoCw('):].replace('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-keyboardlog-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_keyboardlog_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or out[0].splitlines()!=['empty|0|1|0','rtty|1|K1ABC|FN42|599|599|Name, Town|1|0|1','cw|1|K2XYZ|579|589|CW|0|2']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
