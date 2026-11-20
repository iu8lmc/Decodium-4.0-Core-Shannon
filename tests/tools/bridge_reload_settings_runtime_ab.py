#!/usr/bin/env python3
"""Compare CAT profile reload and missing-profile behavior in both source placements."""
import subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeLoadSettings.cpp';a=main.read_text();b=module.read_text();pos=b.index('void DecodiumBridge::reloadBridgeSettingsFromPersistentStore()');head=b[:pos];body=b[pos:];preamble=b[:b.index('void DecodiumBridge::loadSettings()')]
try:
 for label in ('before','after'):
  if label=='before':main.write_text(a+'\n'+preamble+body);module.write_text(head)
  else:main.write_text(a);module.write_text(b)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  for variant in ('a','b'):
   p=subprocess.run(['python3','tests/tools/bridge_reload_settings_runtime_lab.py','--variant',variant],cwd=r,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=150)
   print(label,variant,p.stdout,flush=True)
   if p.returncode:raise RuntimeError(f'{label} {variant} failed')
finally:
 main.write_text(a);module.write_text(b)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
