#!/usr/bin/env python3
"""Compare HALT active-CW state with original and extracted methods."""
import argparse,json,subprocess,sys
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeHalt.cpp';m=main.read_text();n=module.read_text();fixture=(r/'tests/bridge_refactor_fixtures/halt_before.inc').read_text();mark='void DecodiumBridge::halt()';head=n.split(mark,1)[0];tail='#undef bridgeLog\n';assert n.startswith(head+mark) and n.endswith(tail)
parser=argparse.ArgumentParser();parser.add_argument("--kind",choices=("cw","ft2link","telemetry"),default="cw");args=parser.parse_args()
out=[]
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_SHUTDOWN_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head+tail)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  raw=subprocess.check_output([sys.executable,str(r/'tests/tools/bridge_halt_active_runtime_lab.py'),'--binary','build/decodium','--kind',args.kind],cwd=r,text=True,timeout=260)
  result=json.loads(raw.strip().splitlines()[-1]);out.append(result);print(label,json.dumps(result,sort_keys=True),flush=True)
 assert not out[0]['enabled'] and not out[0]['cat'],out
 if args.kind != 'telemetry': assert out[0]['tx'],out
 assert out[1]['stopped'] and not out[1]['cat'],out
finally:
 main.write_text(m);module.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
