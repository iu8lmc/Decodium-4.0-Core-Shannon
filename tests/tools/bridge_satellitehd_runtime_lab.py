#!/usr/bin/env python3
"""Inspect satellite half-duplex status in isolated app without CAT or TX."""
import argparse,json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);a=p.parse_args();qml=root/'build/qml/decodium/Main.qml';original=qml.read_text();rig='SatelliteHd'+secrets.token_hex(5)
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{var rows=[];bridge.setSetting("Ft2LinkSatelliteHalfDuplexEnabled",false);rows.push(JSON.parse(JSON.stringify(bridge.ft2LinkSatelliteHalfDuplexStatus)));bridge.setSetting("Ft2LinkSatelliteHalfDuplexEnabled",true);rows.push(JSON.parse(JSON.stringify(bridge.ft2LinkSatelliteHalfDuplexStatus)));bridge.setSetting("Ft2LinkSatelliteRxDialHz",145900000);bridge.setSetting("Ft2LinkSatelliteTxDialHz",435100000);bridge.setSetting("Ft2LinkSatelliteCatSettleMs",1);rows.push(JSON.parse(JSON.stringify(bridge.ft2LinkSatelliteHalfDuplexStatus)));console.log('[SAT-HD] '+JSON.stringify(rows));Qt.quit();}}\n'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-satellitehd-') as tmp:
  socks=[]
  for port in (2237,2239):s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',port));socks.append(s)
  qml.write_text(original.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];res=subprocess.run(cmd,cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
  rows=[l.split('[SAT-HD] ',1)[1] for l in res.stdout.splitlines() if '[SAT-HD] ' in l]
  if res.returncode or len(rows)!=1:raise RuntimeError(res.stdout[-6000:])
  values=json.loads(rows[0]);assert not values[0]['enabled'] and values[1]['configurationError'] and not values[2]['configurationError'] and values[2]['settleMs']==250 and not any(x['catConnected'] or x['busy'] for x in values),values
  print(json.dumps(values,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  p=parent/('Decodium - '+rig+' - test')
  if p.is_dir():shutil.rmtree(p)
