#!/usr/bin/env python3
"""Compare runtime audio rebind via virtual-device selection in isolated original/extracted apps."""
import json,subprocess,sys
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeAudioRuntimeChange.cpp';m=main.read_text();n=module.read_text();fixture=(r/'tests/bridge_refactor_fixtures/audioruntimechange_before.inc').read_text();mark='void DecodiumBridge::applyAudioInputRuntimeChange(';head=n.split(mark,1)[0];tail=n.split('#undef bridgeLog',1)[1];assert n==head+fixture+'#undef bridgeLog'+tail
outputs={}
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_PALETTE_INSERTION_POINT';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head+'#undef bridgeLog'+tail)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  outputs[label]=[]
  for variant in ('a','b'):
   result=subprocess.check_output([sys.executable,str(r/'tests/tools/bridge_audio_selection_virtual_runtime_lab.py'),'--variant',variant],cwd=r,text=True,timeout=130)
   value=json.loads(result.strip().splitlines()[-1]);outputs[label].append(value);print(label,variant,json.dumps(value,sort_keys=True),flush=True)
 assert outputs['before']==outputs['after'],outputs
finally:
 main.write_text(m);module.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
