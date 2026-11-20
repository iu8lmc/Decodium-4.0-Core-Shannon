#!/usr/bin/env python3
"""Compare station callsign and base-call normalization."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/worldmap_call_helpers_before.inc').read_text()
new=(root/'src/bridge/DecodiumBridgeWorldMapCalls.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QString>
#include <QTextStream>
#include "MessageTokenRules.hpp"
#include "Radio.hpp"
using decodium::seq::normalizedUsableCallToken;
#include "helpers.inc"
int main(){bool a=normalizeWorldMapCall(" k1abc ")=="K1ABC";bool b=worldMapCallKey("k1abc/p")=="K1ABC";bool c=worldMapCallKey("").isEmpty();bool d=normalizeWorldMapCall("<...>").isEmpty();QTextStream(stdout)<<"normal="<<a<<" portable="<<b<<" empty="<<c<<" placeholder="<<d<<Qt::endl;return a&&b&&c&&d?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-map-calls-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);out=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label
  files=[d/'main.cpp',root/'Sequencer/MessageTokenRules.cpp',root/'src/radio/Radio.cpp']
  subprocess.run(['c++','-std=c++17','-I'+str(d),'-I'+str(root/'Sequencer'),'-I'+str(root/'src/radio'),'-I'+str(root/'src/core'),'-I'+str(root),*[str(f) for f in files],*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);out.append(result.stdout)
 assert out[0]==out[1]
