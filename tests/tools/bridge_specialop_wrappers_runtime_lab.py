#!/usr/bin/env python3
"""Compare activity transitions in an isolated app with CAT and network off."""
import argparse,json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);a=p.parse_args();qml=root/'build/qml/decodium/Main.qml';original=qml.read_text();rig='SpecialOp'+secrets.token_hex(5)
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT8";watch.start();}}\n Timer {id:watch;interval:500;repeat:true;property int i:0;property var out:[];onTriggered:{if(bridge.catConnected||bridge.transmitting){console.log('[SPECIALOP] unsafe');Qt.quit();return;}switch(i){case 0: bridge.foxMode=true;break;case 1:bridge.houndMode=false;break;case 2:bridge.houndMode=true;break;case 3:bridge.foxMode=false;break;case 4:bridge.houndMode=false;break;case 5:bridge.ncontest=1;break;case 6:bridge.ncontest=0;break;case 7:bridge.ncontest=-1;break;case 8:bridge.ncontest=99;break;case 9:bridge.ncontest=0;break;}out.push({activity:bridge.specialOperationActivity,fox:bridge.foxMode,hound:bridge.houndMode,tx:bridge.transmitting});if(++i===10){console.log('[SPECIALOP] '+JSON.stringify(out));stop();Qt.quit();}}}\n'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-specialop-') as tmp:
  sockets=[]
  for port in (2237,2239):s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',port));sockets.append(s)
  qml.write_text(original.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
  cmd=['sandbox-exec','-p',policy,str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','35000'];result=subprocess.run(cmd,cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=65)
  rows=[l.split('[SPECIALOP] ',1)[1] for l in result.stdout.splitlines() if '[SPECIALOP] ' in l]
  if result.returncode or len(rows)!=1 or not rows[0].startswith('['):raise RuntimeError(result.stdout[-8000:])
  values=json.loads(rows[0]);assert [x['activity'] for x in values]==[6,6,7,7,0,1,0,0,9,0] and values[0]['fox'] and values[2]['hound'] and not any(x['tx'] for x in values),values
  print(json.dumps(values,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  p=parent/('Decodium - '+rig+' - test')
  if p.is_dir():shutil.rmtree(p)
