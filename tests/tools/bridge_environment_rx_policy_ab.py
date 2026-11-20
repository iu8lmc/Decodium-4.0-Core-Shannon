#!/usr/bin/env python3
"""Compare CAT suppression and mode ownership with isolated environment changes."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2];old=(root/'tests/bridge_refactor_fixtures/environment_rx_policy_before.inc').read_text();new=(root/'src/bridge/DecodiumBridgeEnvironmentRxPolicy.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QString>
#include <QByteArray>
#include <QTextStream>
namespace {
#include "helpers.inc"
int main(){qunsetenv("DECODIUM_DISABLE_CAT");qunsetenv("DECODIUM_RX_RECORD_DISABLE_CAT");bool normal=!catSuppressedByEnvironment()&&!bridgeOwnsLegacyJtRxWhenCatSuppressed("JT65");qputenv("DECODIUM_DISABLE_CAT"," YeS ");bool jt=catSuppressedByEnvironment()&&bridgeOwnsLegacyJtRxWhenCatSuppressed(" jt65 ")&&bridgeOwnsLegacyJtRxWhenCatSuppressed("JT4")&&!bridgeOwnsLegacyJtRxWhenCatSuppressed("FT8");qputenv("DECODIUM_DISABLE_CAT","0");qputenv("DECODIUM_RX_RECORD_DISABLE_CAT","on");bool record=catSuppressedByEnvironment();bool fst=bridgeOwnsFst4Audio(" fst4w-120 ")&&bridgeOwnsFst4Audio("FST4")&&!bridgeOwnsFst4Audio("FT8");QTextStream(stdout)<<"normal="<<normal<<" jt="<<jt<<" record="<<record<<" fst="<<fst<<Qt::endl;return normal&&jt&&record&&fst?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-env-policy-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);out=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label;subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True);v=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,v.stdout.strip(),flush=True);out.append(v.stdout)
 assert out[0]==out[1]
