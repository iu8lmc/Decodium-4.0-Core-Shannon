#!/usr/bin/env python3
"""Compare the extracted full-export implementation in two placements."""
import subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'
module=r/'src/bridge/DecodiumBridgeAdifEditing.cpp'
a,b=main.read_text(),module.read_text()
legacy=r/'src/bridge/DecodiumBridgeLegacyInit.cpp'
legacy_original=legacy.read_text()
legacy_macro='#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled'
assert legacy_original.count(legacy_macro)==1
start=b.index('bool DecodiumBridge::exportToAdif(')
end=b.index('bool DecodiumBridge::deleteQso(',start)
body=b[start:end]
results=[]
try:
 legacy.write_text(legacy_original.replace(legacy_macro,'#define embeddedLegacyRigControlEnabledForBackend(...) false',1))
 for label in ('before','after'):
  if label=='before':
   main.write_text(a+'\n#include "AdifExportSanitizer.h"\n#include <QSaveFile>\n'+body)
   module.write_text(b[:start]+b[end:])
  else:
   main.write_text(a);module.write_text(b)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  for variant in ('a','b'):
   p=subprocess.run(['python3','tests/tools/bridge_adif_full_export_runtime_lab.py','--variant',variant],cwd=r,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
   print(label,variant,p.stdout,flush=True)
   if p.returncode: raise RuntimeError(f'{label} {variant} failed')
   results.append((label,variant,p.stdout))
finally:
 legacy.write_text(legacy_original)
 main.write_text(a);module.write_text(b)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
