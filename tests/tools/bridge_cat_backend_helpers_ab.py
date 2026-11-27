#!/usr/bin/env python3
"""Compare CAT backend, split and rigctl classification without a radio."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/cat_backend_helpers_before.inc').read_text()
new=(root/'src/bridge/DecodiumBridgeCatBackendHelpers.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QObject>
#include <QString>
#include <QVariant>
#include <QTextStream>
#include "helpers.inc"
int main(){QObject m;m.setProperty("portType"," NETWORK ");m.setProperty("rigName","Net rigctl");bool split=normalizedCatSplitMode(" fake it ")=="emulate"&&normalizedCatSplitMode("RIG")=="rig"&&normalizedCatSplitMode("other")=="none";bool backend=isHamlibFamilyBackend(" TCI ")&&!isHamlibFamilyBackend("omnirig")&&isNetworkRigctlCat("HAMLIB",&m)&&!isNetworkRigctlCat("tci",&m)&&!isNetworkRigctlCat("hamlib",nullptr);bool matched=catSignalMatchesBackend("tci","hamlib")&&!catSignalMatchesBackend("hamlib","tci")&&catSignalMatchesBackend("Hamlib","hamlib");QTextStream(stdout)<<"split="<<split<<" backend="<<backend<<" signals="<<matched<<Qt::endl;return split&&backend&&matched?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-cat-helpers-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outs=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label;subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True);result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outs.append(result.stdout)
 assert outs[0]==outs[1]
