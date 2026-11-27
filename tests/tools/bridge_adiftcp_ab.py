#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,n):i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
new=(R/'src/bridge/DecodiumBridgeAdifTcp.cpp').read_text();name='void DecodiumBridge::tcpSendLoggedAdifQso(';a=sec(old,name);b=sec(new,name).replace('decodiumBridgeAdifTcpLog(', 'bridgeLog(')
if a!=b:raise RuntimeError('ADIF TCP source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core','Qt6Network',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-adiftcp-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_adiftcp_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_adiftcp.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or 'disabled=0\nempty=0\npayload=<CALL:5>K1ABC <BAND:3>20M <EOR>' not in out[0]:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
