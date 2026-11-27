#!/usr/bin/env python3
"""Compare MAM slot lifecycle with Multi Answer before and after extraction."""
from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridge.cpp'; module=r/'src/bridge/DecodiumBridgeMamState.cpp'
m=main.read_text(); n=module.read_text(); fixture=(r/'tests/bridge_refactor_fixtures/multianswer_mode_before.inc').read_text(); marker='void DecodiumBridge::setMultiAnswerMode(bool v)'; head=n.split(marker,1)[0]
assert n==head+fixture.replace('bridgeLog(', 'decodiumBridgeMamQueueLog(')
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_UTC_TIMER_INSERTION_POINT'; assert m.count(point)==1; main.write_text(m.replace(point,fixture+point)); module.write_text(head)
  else: main.write_text(m); module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  out=subprocess.run(['python3','tests/tools/bridge_mamstate_runtime_lab.py','--binary','build/decodium'],cwd=r,check=True,text=True,stdout=subprocess.PIPE)
  print(label,out.stdout.strip(),flush=True)
finally:
 main.write_text(m);module.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
