#!/usr/bin/env python3
"""A/B audio capture and actual virtual-audio recording."""
import json,subprocess,sys
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeAudioCaptureStart.cpp';m=main.read_text();n=module.read_text();fixture=(r/'tests/bridge_refactor_fixtures/audiocapture_before.inc').read_text();mark='void DecodiumBridge::startAudioCapture(bool watchdogRecovery)';head=n.split(mark,1)[0];tail='';assert n==head+fixture+tail;values=[]
try:
 for label in ('before','after'):
  if label=='before':
   main.write_text(m+'\n#undef bridgeLog\n'+head+fixture+tail);module.write_text(head+tail)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  proc=subprocess.run([sys.executable,str(r/'tests/tools/bridge_rxcontrol_audio_lab.py'),'--binary','build/decodium'],cwd=r,text=True,timeout=110,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  if proc.returncode:print(proc.stdout,flush=True);raise RuntimeError('RX audio lab failed')
  value=json.loads(proc.stdout.strip().splitlines()[-1]);values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);module.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
