#!/usr/bin/env python3
"""Compare active virtual-audio clear before and after extraction."""
import json,subprocess,sys
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeTxMessages.cpp';m=main.read_text();n=module.read_text();fixture=(r/'tests/bridge_refactor_fixtures/cleartxmessages_before.inc').read_text();mark='void DecodiumBridge::clearTxMessages()';head=n.split(mark,1)[0];tail='#undef bridgeLog\n';assert n==head+fixture+tail;values=[]
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_TX_OUTPUT_DEVICE_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head+tail)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  raw=subprocess.check_output([sys.executable,str(r/'tests/tools/bridge_cleartx_active_audio_lab.py'),'--binary','build/decodium','--kind','cw'],cwd=r,text=True,timeout=260)
  value=json.loads(raw.strip().splitlines()[-1]);assert value['stopped'] and value['seen'] and not value['cat'] and value['rmsBefore']>0.005 and value['rmsAfter']<0.002;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
finally:
 main.write_text(m);module.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
