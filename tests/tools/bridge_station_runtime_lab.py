#!/usr/bin/env python3
"""Compare generated station messages without enabling TX or CAT."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{
  bridge.mode="FT8";bridge.callsign="9H1SR";bridge.grid="JM75";bridge.dxCall="K1ABC";
  var cq=bridge.tx6;var tx1=bridge.tx1;var tx2=bridge.tx2;bridge.setTxMessage(6,"CQ WWA 9H1SR JM75");bridge.grid="JN70";var custom=bridge.tx6;bridge.mode="WSPR";bridge.wsprPowerDbm=37;
  console.log("[STATION-PROBE] "+JSON.stringify({cq:cq,tx1:tx1,tx2:tx2,custom:custom,wspr:bridge.tx6,empty:bridge.tx1==="",tx:bridge.transmitting}));
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-station-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','Station'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','22000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  lines=[x.split('[STATION-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[STATION-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('station probe failed')
  v=json.loads(lines[0]);
  if not ('9H1SR' in v['cq'] and 'K1ABC' in v['tx1'] and 'K1ABC' in v['tx2'] and 'CQ WWA' in v['custom'] and '37' in v['wspr'] and v['empty'] and not v['tx']):raise RuntimeError('unexpected station state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:qml.write_text(original)
