#!/usr/bin/env python3
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/decode_click_before.inc').read_text();sig='void DecodiumBridge::engageDxClusterSpot('
s=(R/'src/bridge/DecodiumBridgeDecodeClick.cpp').read_text();b=sig+s.split(sig,1)[1]
for new,old in [('decodiumBridgeDiagnosticOneLine(', 'bridgeDiagnosticOneLine('),('decodiumBridgeReplyTxPeriodForClickedDecode(', 'replyTxPeriodForClickedDecode('),('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')]:b=b.replace(new,old)
if a!=b:raise SystemExit('decode click differs')
print('Decode click and message generation match pre-extraction source modulo forwarding wrappers')
import os,shlex,subprocess,tempfile
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-genstd-ab-') as t:
 d=Path(t)
 for source in (a,b):
  code='void DecodiumBridge::genStdMsgs('+source.split('void DecodiumBridge::genStdMsgs(',1)[1];(d/'method.inc').write_text(code);exe=d/'test';files=[R/'tests/bridge_refactor_genstd_harness.cpp',R/'Sequencer/MessageTokenRules.cpp',R/'src/radio/Radio.cpp',R/'src/bridge/DecodeSignalReport.cpp',R/'src/bridge/FtxMessageHelpers.cpp'];subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(d),'-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),*[str(p) for p in files],*flags,'-o',str(exe)],check=True);out.append(subprocess.check_output([str(exe)],text=True))
if out[0]!=out[1]:raise SystemExit('generated messages differ')
print('Generated standard messages A/B: 12 mode/quick/RR73 cases and custom CQ match')
