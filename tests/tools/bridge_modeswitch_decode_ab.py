#!/usr/bin/env python3
"""A/B mode-switch cleanup with actual isolated WAV decodes."""
import json,subprocess,sys
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeMode.cpp';m=main.read_text();n=module.read_text();fixture=(r/'tests/bridge_refactor_fixtures/setmode_before.inc').read_text();mark='void DecodiumBridge::setMode(const QString& v)';head=n.split(mark,1)[0];tail='#undef canonicalApplicationDecodeMode\n#undef isFt2LinkApplicationMode\n#undef isStreamingKeyboardMode\n#undef isNativePcmKeyboardMode\n#undef isRadioOnlyModeLabel\n#undef rxInputGainFromLevel\n#undef bridgeLog\n';tail+='\n'+(r/'tests/bridge_refactor_fixtures/mode_period_before.inc').read_text();assert n==head+fixture+tail;values=[]
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_RX_MONITORING_INSERTION_POINT';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head+tail)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  proc=subprocess.run([sys.executable,str(r/'tests/tools/bridge_modeswitch_decode_runtime_lab.py'),'--binary','build/decodium'],cwd=r,text=True,timeout=110,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);raw=proc.stdout
  if proc.returncode: print(raw,flush=True);raise RuntimeError('decode lab failed')
  value=json.loads(raw.strip().splitlines()[-1]);assert value=={'cleared':True,'recovered':True,'tx':False};values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);module.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
