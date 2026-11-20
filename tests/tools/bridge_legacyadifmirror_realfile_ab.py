#!/usr/bin/env python3
"""Compare mirror with real ADIF parser and isolated temporary files."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::mirrorLegacyLoggedAdif(','using AdifImportProgressCallback =')
new=sec((R/'src/bridge/DecodiumBridgeLegacyAdifMirror.cpp').read_text(),'void DecodiumBridge::mirrorLegacyLoggedAdif(').replace('decodiumBridgeLogbookLog(','bridgeLog(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-legacyadif-real-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_legacyadifmirror_realfile_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),str(R/'src/bridge/AdifDocumentInternal.cpp'),str(R/'src/bridge/AdifExportSanitizer.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 if results[0].splitlines()!=['import|2|K1ABC|FT4|1|3|1','duplicate|2|3','failure|1|3']:raise RuntimeError('unexpected '+results[0])
 print(results[0],end='')
