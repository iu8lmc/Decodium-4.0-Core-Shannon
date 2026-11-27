#!/usr/bin/env python3
"""Compare special-operation public setters in the two source placements."""
import subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeSpecialOperation.cpp';a=main.read_text();b=module.read_text()
s=b.index('int DecodiumBridge::ncontest() const');e=b.index('bool DecodiumBridge::specialOperationRequiresLegacyTx()',s);body=b[s:e];results=[]
try:
 for label in ('before','after'):
  if label=='before':main.write_text(a+'\n#include "SpecialOperationBounds.h"\n'+body);module.write_text(b[:s]+b[e:])
  else:main.write_text(a);module.write_text(b)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  p=subprocess.run(['python3','tests/tools/bridge_specialop_wrappers_runtime_lab.py','--binary',str(r/'build/decodium')],cwd=r,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=80)
  print(label,p.stdout,flush=True)
  if p.returncode:raise RuntimeError(f'{label} failed')
  results.append(p.stdout)
 assert results[0]==results[1]
finally:
 main.write_text(a);module.write_text(b)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
