#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/cat_split_before.inc').read_text();sig='bool DecodiumBridge::catSplitOperationActiveForMode() const'
s=(R/'src/bridge/DecodiumBridgeCatSplitMath.cpp').read_text();b=sig+s.split(sig,1)[1]
for new,old in [('decodiumBridgeIsHamlibFamilyBackend(', 'isHamlibFamilyBackend('),('decodiumBridgeNormalizedCatSplitMode(', 'normalizedCatSplitMode('),('decodiumBridgeCanonicalApplicationDecodeMode(', 'canonicalApplicationDecodeMode(')]:b=b.replace(new,old)
if a!=b:raise SystemExit('CAT split differs')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-catsplit-ab-') as t:
 d=Path(t)
 for code in (a,b):
  (d/'method.inc').write_text(code);exe=d/'test';subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(d),str(R/'tests/bridge_refactor_cat_split_harness.cpp'),*flags,'-o',str(exe)],check=True);outputs.append(subprocess.check_output([str(exe)],text=True))
if outputs[0]!=outputs[1]:raise SystemExit('CAT split A/B mismatch')
print('CAT split A/B: '+outputs[0],end='')
