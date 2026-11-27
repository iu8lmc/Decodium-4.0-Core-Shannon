#!/usr/bin/env python3
"""Compare original/extracted legacy sync using the isolated ALL.TXT UI lab."""
from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeLegacySync.cpp'
m=main.read_text();n=module.read_text();start='void DecodiumBridge::syncLegacyBackendDecodeList()';old=(r/'tests/bridge_refactor_fixtures/legacy_sync_before.inc').read_text()
assert n[n.index(start):]==old
try:
 for label in ('before','after'):
  if label=='before':
   anchor='// === STATION ===';assert m.count(anchor)==1;main.write_text(m.replace(anchor,old+anchor,1));module.write_text(n[:n.index(start)])
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  out=subprocess.run(['python3','tests/tools/bridge_legacyaugment_runtime_lab.py'],cwd=r,check=True,text=True,stdout=subprocess.PIPE)
  print(label,out.stdout.strip(),flush=True)
finally:
 main.write_text(m);module.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
