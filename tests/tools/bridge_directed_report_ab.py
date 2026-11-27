#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/directed_report_guard_before.inc').read_text();sig='bool DecodiumBridge::shouldSuppressConflictingDirectedReportDecode('
s=(R/'src/bridge/DecodiumBridgeDirectedReportGuard.cpp').read_text();b=sig+s.split(sig,1)[1];b=b.replace('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')
if a!=b:raise SystemExit('directed report guard differs')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-reportguard-ab-') as t:
 d=Path(t)
 for code in (a,b):
  (d/'method.inc').write_text(code);exe=d/'test';files=[R/'tests/bridge_refactor_directed_report_harness.cpp',R/'Sequencer/MessageTokenRules.cpp',R/'src/radio/Radio.cpp',R/'src/bridge/DecodeMessageNormalization.cpp',R/'src/bridge/DecodeValidationRules.cpp'];subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(d),'-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),*[str(p) for p in files],*flags,'-o',str(exe)],check=True);outputs.append(subprocess.check_output([str(exe)],text=True))
if outputs[0]!=outputs[1]:raise SystemExit('directed report guard A/B mismatch')
print('Directed report guard A/B: '+outputs[0],end='')
