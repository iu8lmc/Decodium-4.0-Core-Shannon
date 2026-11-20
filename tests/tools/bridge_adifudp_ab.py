#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,n):i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
new=(R/'src/bridge/DecodiumBridgeAdifUdp.cpp').read_text();names=['bool DecodiumBridge::udpSendRawAdifDatagram(', 'void DecodiumBridge::udpSendN1mmLoggedQso('];a='\n'.join(sec(old,n) for n in names);b='\n'.join(sec(new,n).replace('decodiumBridgeAdifUdpLog(', 'bridgeLog(') for n in names)
if a!=b:raise RuntimeError('ADIF UDP source differs')
helpers=(R/'src/bridge/UdpTargetHelpers.h').read_text()
for n in ('quint16 udpPortFromSettingValue(', 'QHostAddress resolveUdpHostAddress('):
 if sec(old,'static '+n)!=sec(helpers,'inline '+n).replace('inline ','static ',1):raise RuntimeError('UDP target helper differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core','Qt6Network',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-adifudp-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_adifudp_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_adifudp.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/bridge'),str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or 'payload=<CALL:5>K1ABC <BAND:3>20m <eor>' not in out[0] or 'disabled=0' not in out[0]:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
