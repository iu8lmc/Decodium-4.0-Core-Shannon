#!/usr/bin/env python3
"""Observe actual playback completion in original and moved placements."""
import json,subprocess,sys
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeTxFinish.cpp';m=main.read_text();n=module.read_text();fixture=(r/'tests/bridge_refactor_fixtures/txfinish_before.inc').read_text();point='// BRIDGE_CONSTRUCTOR_INSERTION_POINT';assert m.count(point)==1
marker='bool DecodiumBridge::noteTxPlaybackFinished(const QString& reason, bool error)\n{';probe='\n    qInfo().noquote() << "[TX-FINISH-LAB]" << m_activeTxNumber;';assert fixture.count(marker)==1 and n.count(marker)==1
observed=[]
try:
 for label in ('before','after'):
  if label=='before':main.write_text(m.replace(point,fixture.replace(marker,marker+probe,1)+point));module.write_text(n[:n.index(marker)]+n[n.index('void DecodiumBridge::scheduleSyncTxBoundaryStop('):])
  else:main.write_text(m);module.write_text('#include <QDebug>\n'+n.replace(marker,marker+probe,1))
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  result=subprocess.run([sys.executable,str(r/'tests/tools/bridge_txfinish_playback_lab.py'),'--binary',str(r/'build/decodium'),'--mode','FT2'],cwd=r,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=100)
  if result.returncode:raise RuntimeError(result.stdout[-8000:])
  value=json.loads(result.stdout.strip());observed.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert observed[0]==observed[1],observed
finally:
 main.write_text(m);module.write_text(n);subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
