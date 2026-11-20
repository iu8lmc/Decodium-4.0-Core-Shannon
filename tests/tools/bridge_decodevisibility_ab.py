#!/usr/bin/env python3
"""Compare decode visibility using real worked filter policy."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'decodium::decode_ui::WorkedFilterOptions DecodiumBridge::workedFilterOptions() const','// 1.0.145: detection ghost decode')
new=sec((R/'src/bridge/DecodiumBridgeDecodeVisibility.cpp').read_text(),'decodium::decode_ui::WorkedFilterOptions DecodiumBridge::workedFilterOptions() const').replace('decodiumBridgeDecodeTelemetryOnly(', 'decodeListModel_isTelemetryOnlyMessage(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-decodevisibility-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_decodevisibility_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise RuntimeError('results differ')
 expected=['worked|0','cq|1','tx|1','bypass|1|0','ghost|0','telemetry|0','display|1','empty|0','yesterday|0']
 if results[0].splitlines()!=expected:raise RuntimeError('unexpected results '+results[0])
 print(results[0],end='')
