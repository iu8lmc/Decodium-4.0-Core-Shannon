#!/usr/bin/env python3
"""Compare SSTV helper placement through real isolated app controls."""
import os,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
main=root/'src/bridge/DecodiumBridgeSstv.cpp'; mod=root/'src/bridge/DecodiumBridgeSstvHelpers.cpp'
m=main.read_text(); n=mod.read_text(); fixture=(root/'tests/bridge_refactor_fixtures/sstv_helpers_before.inc').read_text()
assert n.endswith(fixture)
a=m.index('#if DECODIUM_HAS_SSTV\nusing decodium::sstv::SstvAudioSourceKind;'); b=m.index('void DecodiumBridge::initialiseSstvRuntime()',a)
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software',DECODIUM_FORCE_STANDALONE_UDP='1')
try:
 for label in ('before','after'):
  main.write_text(m[:a]+fixture+m[b:] if label=='before' else m)
  mod.write_text('#include "DecodiumBridge.h"\n' if label=='before' else n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=root,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  for case in ('a','b'):
   proc=subprocess.run(['sandbox-exec','-p',policy,sys.executable,'tests/tools/bridge_sstv_controls_runtime_lab.py','--variant',case],cwd=root,env=env,text=True,capture_output=True,timeout=60)
   if proc.returncode: raise RuntimeError(proc.stdout[-5000:]+proc.stderr[-3000:])
   print(label,proc.stdout.strip(),flush=True)
finally:
 main.write_text(m);mod.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=root,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
