#!/usr/bin/env python3
"""Compare legacy rig ownership decisions without operating a radio."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2];old=(root/'tests/bridge_refactor_fixtures/legacy_rig_policy_before.inc').read_text();new=(root/'src/bridge/DecodiumBridgeLegacyRigPolicy.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QString>
#include <QTextStream>
class DecodiumLegacyBackend{public: bool avail=false,control=false;bool available()const{return avail;}bool rigControlEnabled()const{return control;}};
#include "helpers.inc"
int main(){DecodiumLegacyBackend b;bool fallback=!useLegacyRigControlFallback(&b,"hamlib")&&!useLegacyRigControlFallback(nullptr,"tci");bool policy=embeddedLegacyRigControlEnabledForBackend("hamlib",false)&&!embeddedLegacyRigControlEnabledForBackend(" OmniRig ",false)&&!embeddedLegacyRigControlEnabledForBackend("CAT4OM",false)&&!embeddedLegacyRigControlEnabledForBackend("hamlib",true);bool owner=!legacyOwnsRigControl(nullptr)&&!legacyOwnsRigControl(&b);b.avail=true;owner=owner&&!legacyOwnsRigControl(&b);b.control=true;owner=owner&&legacyOwnsRigControl(&b);QTextStream(stdout)<<"fallback="<<fallback<<" policy="<<policy<<" owner="<<owner<<Qt::endl;return fallback&&policy&&owner?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-legacy-rig-policy-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);out=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label;subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True);v=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,v.stdout.strip(),flush=True);out.append(v.stdout)
 assert out[0]==out[1]
