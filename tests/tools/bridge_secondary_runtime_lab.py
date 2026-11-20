#!/usr/bin/env python3
"""Check secondary CQ station routing after an actual FT8 WAV decode."""
import argparse,json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav=ROOT/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'
rig='Secondary'+secrets.token_hex(5)
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{bridge.filtersBypassed=true;bridge.openWavForDecode(__WAV__);waitDecode.start()}}
 Timer {id:waitDecode;interval:250;running:false;repeat:true;property int ticks:0;onTriggered:{
  var rows=bridge.decodeList;var decoded=false;for(var i=0;i<rows.length;i++)if(String(rows[i].message).indexOf("CQ K1ABC FN42")>=0)decoded=true;
  var found=false;for(var j=0;j<bridge.activeStations.count;j++)if(bridge.activeStations.callsignAt(j)==="K1ABC")found=true;
  if(decoded && found){console.log("[SECONDARY-PROBE] "+JSON.stringify({decoded:decoded,station:found,tx:bridge.transmitting}));stop();return;}
  if(++ticks>=100){console.log("[SECONDARY-PROBE] "+JSON.stringify({decoded:decoded,station:found,timeout:true}));stop()}
 }}
'''.replace('__WAV__',json.dumps(str(wav)))
try:
 with tempfile.TemporaryDirectory(prefix='decodium-secondary-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software')
  sockets=[]
  for port in (2237,2239):
   sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sockets.append(sock)
  policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
  cmd=['sandbox-exec','-p',policy,str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','50000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=100)
  lines=[x.split('[SECONDARY-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[SECONDARY-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('secondary probe failed')
  v=json.loads(lines[0]);
  if v!={'decoded':True,'station':True,'tx':False}:raise RuntimeError('unexpected secondary state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
