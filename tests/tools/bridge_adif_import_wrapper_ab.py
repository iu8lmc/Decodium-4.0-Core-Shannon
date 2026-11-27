#!/usr/bin/env python3
"""Compare ADIF compatibility wrapper and progress setters in two placements."""
import subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; module=r/'src/bridge/DecodiumBridgeAdifImport.cpp'
a,b=main.read_text(),module.read_text()
s1=b.index('void DecodiumBridge::setAdifImportProgress(');e1=b.index('bool DecodiumBridge::importFromAdifAsync(',s1)
s2=b.index('int DecodiumBridge::importFromAdif(',e1); e2=b.index('\n}',s2)+2
methods=b[s1:e1]+'\n'+b[s2:e2]+'\n'
try:
 for label in ('before','after'):
  if label=='before':main.write_text(a+'\n'+methods);module.write_text(b[:s1]+b[e1:s2]+b[e2:])
  else:main.write_text(a);module.write_text(b)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  for variant in ('a','b'):
   p=subprocess.run(['python3','tests/tools/bridge_adif_import_wrapper_runtime_lab.py','--variant',variant],cwd=r,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
   print(label,variant,p.stdout,flush=True)
   if p.returncode:raise RuntimeError(f'{label} {variant} failed')
finally:
 main.write_text(a);module.write_text(b)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
