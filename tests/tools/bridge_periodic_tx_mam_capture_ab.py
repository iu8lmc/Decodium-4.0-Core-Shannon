#!/usr/bin/env python3
"""Compare MAM slot lifecycle with Multi Answer before and after extraction."""
from pathlib import Path
import json,subprocess
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridge.cpp'; module=r/'src/bridge/DecodiumBridgePeriodicTx.cpp'
m=main.read_text(); n=module.read_text(); fixture=(r/'tests/bridge_refactor_fixtures/periodic_tx_before.inc').read_text(); marker='void DecodiumBridge::checkAndStartPeriodicTx()'; head=n.split(marker,1)[0]
assert n==head+fixture
fixture=fixture.replace('        mamDispatchPeriod(elasticGo);','        qWarning("[LAB-PERIODIC-MAM]"); mamDispatchPeriod(elasticGo);',1);instrumented=head+fixture
values=[]
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n'; assert m.count(point)==1; main.write_text(m.replace(point,fixture+point)); module.write_text(head)
  else: main.write_text(m); module.write_text(instrumented)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  out=subprocess.run(['python3','tests/tools/bridge_periodic_tx_mam_capture_runner.py','--binary','build/decodium','--mode','FT8'],cwd=r,check=True,text=True,stdout=subprocess.PIPE)
  value=json.loads(out.stdout.strip().splitlines()[-1]);values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]['streams']==values[1]['streams'] and abs(values[0]['audioPeak']-values[1]['audioPeak'])<0.01 and abs(values[0]['audioWindows']-values[1]['audioWindows'])<=2,values
finally:
 main.write_text(m);module.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
