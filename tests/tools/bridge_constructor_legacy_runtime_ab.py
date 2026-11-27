#!/usr/bin/env python3
"""Compare deferred macOS legacy startup under a deliberately slow QML boot."""
import json,os,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; module=r/'src/bridge/DecodiumBridgeConstructor.cpp'; legacy=r/'src/bridge/DecodiumBridgeLegacyInit.cpp'
m=main.read_text();n=module.read_text();original_legacy=legacy.read_text();fixture=(r/'tests/bridge_refactor_fixtures/bridge_constructor_before.inc').read_text()
point='// BRIDGE_CONSTRUCTOR_INSERTION_POINT';assert m.count(point)==1 and n.endswith(fixture)
macro='#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled';assert original_legacy.count(macro)==1
safe=original_legacy.replace(macro,'#define embeddedLegacyRigControlEnabledForBackend(...) false',1)
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
try:
 values=[]
 for label in ('before','after'):
  main.write_text(m.replace(point,fixture+point,1) if label=='before' else m);module.write_text('#include <QString>\n' if label=='before' else n);legacy.write_text(safe)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  proc=subprocess.run(['sandbox-exec','-p',policy,'python3','tests/tools/bridge_startup_legacy_order_lab.py','current'],cwd=r,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=70)
  if proc.returncode:raise RuntimeError(proc.stdout[-7000:])
  report=json.loads(proc.stdout.strip().splitlines()[-1]);assert report['main_before_legacy'] and report['exit']==0,report
  values.append(report['main_before_legacy']);print(label,json.dumps(report,sort_keys=True),flush=True)
 assert values==[True,True]
finally:
 main.write_text(m);module.write_text(n);legacy.write_text(original_legacy)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
