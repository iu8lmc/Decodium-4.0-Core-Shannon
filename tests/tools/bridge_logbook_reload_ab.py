#!/usr/bin/env python3
"""Compare logbook reload with real ADIF parsing and isolated collaborators."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def section(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
old=section(run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout,'void DecodiumBridge::reloadActiveLogbookState(','\n}\n')+'\n}\n'
new=section((R/'src/bridge/DecodiumBridgeLogbookProfiles.cpp').read_text(),'void DecodiumBridge::reloadActiveLogbookState(').replace('decodiumBridgeLogbookLog(', 'bridgeLog(')
if old.strip()!=new.strip():raise RuntimeError('implementation differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-logbook-reload-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_logbook_reload_harness.cpp').read_text());out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),str(R/'src/bridge/AdifDocumentInternal.cpp'),str(R/'src/bridge/AdifExportSanitizer.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or out[0].splitlines()!=['loaded|2|1|1|2|1|1|4|1|1','missing|0|0|0|8']:raise RuntimeError('unexpected result '+repr(out))
 print(out[0],end='')
