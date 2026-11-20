#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,n):i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
new=(R/'src/bridge/DecodiumBridgeExternalAdif.cpp').read_text();names=['QVariantMap DecodiumBridge::uploadExternalAdif(', 'QVariantMap DecodiumBridge::uploadExternalAdifForOutbox(', 'QVariantMap DecodiumBridge::uploadExternalAdifInternal(', 'void DecodiumBridge::noteExternalAdifUploadBackend(', 'void DecodiumBridge::finishExternalAdifUploadIfReady('];a='\n'.join(sec(old,n) for n in names);b='\n'.join(sec(new,n).replace('decodiumBridgeExternalAdifLog(', 'bridgeLog(') for n in names)
if a!=b:raise RuntimeError('external ADIF implementation differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core','Qt6Network',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-externaladif-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_externaladif_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_externaladif.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or 'status:42:Failed' not in out[0] or 'raw-failure|ok=0' not in out[0] or 'missing|ok=0' not in out[0]:raise RuntimeError('unexpected output '+repr(out))
 print(f'External ADIF A/B: {len(out[0].splitlines())} result and callback cases match; destinations simulated')
