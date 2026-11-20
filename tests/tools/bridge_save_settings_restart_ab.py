#!/usr/bin/env python3
"""Compare persisted settings across app restarts in both source placements."""
import subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeSaveSettings.cpp';a=main.read_text();b=module.read_text();pos=b.index('void DecodiumBridge::saveSettings()');head=b[:pos];body=b[pos:]
try:
 for label in ('before','after'):
  if label=='before':main.write_text(a+'\n'+head+body);module.write_text(head)
  else:main.write_text(a);module.write_text(b)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  for variant in ('a','b'):
   p=subprocess.run(['python3','tests/tools/bridge_save_settings_restart_runtime_lab.py','--variant',variant],cwd=r,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=150)
   print(label,variant,p.stdout,flush=True)
   if p.returncode:raise RuntimeError(f'{label} {variant} failed')
finally:
 main.write_text(a);module.write_text(b)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
