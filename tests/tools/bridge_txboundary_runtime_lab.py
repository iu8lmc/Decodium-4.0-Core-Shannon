#!/usr/bin/env python3
"""Observe TX boundary and PCM drain checks in before/after app builds."""
import json,subprocess,sys
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeTxFinish.cpp';m=main.read_text();n=module.read_text();fixture=(r/'tests/bridge_refactor_fixtures/txboundary_before.inc').read_text();point='// BRIDGE_COMPLETE_TX_INSERTION_POINT\n';assert m.count(point)==1
start='void DecodiumBridge::scheduleSyncTxBoundaryStop(const QString& reason, quint64 txSerial)\n{';end='bool DecodiumBridge::delayTxFinishUntilPcmConsumed(const QString& context,\n                                                   const QString& reason,\n                                                   bool useCompletePlayback)\n{';assert fixture.count(start)==fixture.count(end)==n.count(start)==n.count(end)==1
def instrument(s):return s.replace(start,start+'\n    qInfo().noquote() << "[TX-BOUNDARY-LAB]";',1).replace(end,end+'\n    qInfo().noquote() << "[TX-DRAIN-LAB]";',1)
values=[]
try:
 for label in ('before','after'):
  if label=='before':main.write_text(m.replace(point,instrument(fixture)+point));module.write_text(n[:n.index(start)])
  else:main.write_text(m);module.write_text('#include <QDebug>\n'+instrument(n))
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  result=subprocess.run([sys.executable,str(r/'tests/tools/bridge_txboundary_playback_lab.py'),'--binary',str(r/'build/decodium'),'--mode','FT2'],cwd=r,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=100)
  if result.returncode:raise RuntimeError(result.stdout[-8000:])
  value=json.loads(result.stdout.strip());values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1],values
finally:
 main.write_text(m);module.write_text(n);subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
