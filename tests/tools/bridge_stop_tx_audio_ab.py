#!/usr/bin/env python3
"""Compare shutdown and persisted settings in isolated Decodium processes."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridge.cpp'; mod=r/'src/bridge/DecodiumBridgeStopTx.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,n,s,q=(p.read_text() for p in (main,mod,sstv,qml)); fixture=(r/'tests/bridge_refactor_fixtures/stop_tx_before.inc').read_text(); head=n.split('void DecodiumBridge::stopTx()',1)[0]; assert n==head+fixture

rigs=[]; results=[]
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_STOP_TX_INSERTION_POINT\n'; assert m.count(point)==1; main.write_text(m.replace(point,fixture.replace('void DecodiumBridge::stopTx()\n{','void DecodiumBridge::stopTx()\n{\n qWarning(\"[LAB-STOP-TX]\");',1)+'\n'+point));mod.write_text(head)
  else:main.write_text(m);mod.write_text(n.replace('void DecodiumBridge::stopTx()\n{','void DecodiumBridge::stopTx()\n{\n qWarning("[LAB-STOP-TX]");',1))
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  result=subprocess.run(['python3','tests/tools/bridge_stop_tx_audio_runner.py','--binary','build/decodium','--kind','ft2link'],cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=100)
  if result.returncode:raise RuntimeError(result.stdout[-8000:])
  val=json.loads(result.stdout.strip().splitlines()[-1]);assert val['seen'] and val['accepted'] and val['stopped'] and not val['cat'] and val['audioPeak']>0.003 and val['stopSeen'],val;results.append(val);print(label,json.dumps(val,sort_keys=True),flush=True)

 assert {k:v for k,v in results[0].items() if k not in ("audioPeak","tailPeak")}=={k:v for k,v in results[1].items() if k not in ("audioPeak","tailPeak")}
finally:
 for p,content in ((main,m),(mod,n),(sstv,s),(qml,q)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
