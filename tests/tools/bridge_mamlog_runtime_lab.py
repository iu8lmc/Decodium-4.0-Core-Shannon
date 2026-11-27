#!/usr/bin/env python3
"""Exercise the actual QML TX selection, without CAT, monitoring or TX."""
import argparse, json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(); p.add_argument('--binary',type=Path,required=True); a=p.parse_args()
qml=root/'build/qml/decodium/Main.qml'; original=qml.read_text(); marker='GlassApplicationWindow {'
assert original.count(marker)==1
rig='MamLog'+secrets.token_hex(5)
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{
  var ok=bridge.createLogbook("SyntheticMamLog__RIG__",false);if(!ok || String(bridge.activeLogbookPath).indexOf("/.qttest/")<0 || bridge.catConnected || bridge.transmitting || bridge.audioOutputDevice!=="BlackHole 2ch"){console.log("[TXSEL] unsafe");return;}
  bridge.setSetting("PromptToLog",false);bridge.mode="FT8";bridge.callsign="9H1SR";bridge.grid="JM75";bridge.mamMultiStream=true;bridge.mamMaxStreams=2;bridge.multiAnswerMode=true;bridge.txEnabled=true;
  bridge.mamEnqueueClickedStation("K1ABC",1000);bridge.mamEnqueueClickedStation("W1AW",2000);bridge.mamLogSlotNow("K1ABC");var slots=JSON.parse(JSON.stringify(bridge.mamActiveSlots));var path=bridge.activeLogbookPath;bridge.halt();check.path=path;check.slots=slots;check.start();
 }}
 Timer{id:check;interval:250;repeat:true;property int ticks:0;property string path:"";property var slots:[];onTriggered:{var f=bridge.readTextFile(path,100000);var text=f.ok?String(f.text):"";if(/<CALL:5>K1ABC/i.test(text)){console.log("[TXSEL] "+JSON.stringify({call:/<CALL:5>K1ABC/i.test(text),count:bridge.qsoCount,slots:slots.map(function(x){return x.call;}),cat:bridge.catConnected,tx:bridge.transmitting}));stop();return;}if(++ticks>=20){console.log("[TXSEL] timeout");stop();}}}
'''.replace('__RIG__',rig)
try:
 with tempfile.TemporaryDirectory(prefix='decodium-txselection-lab-') as tmp:
  sockets=[]
  for port in (2237,2239):
   s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',port));sockets.append(s)
  guard=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);guard.bind(('127.0.0.1',2238));guard.close()
  qml.write_text(original.replace(marker,marker+probe,1))
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  command=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-audio-output','BlackHole 2ch','--lab-quit-ms','20000']
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))']+command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=70)
  lines=[s.split('[TXSEL] ',1)[1] for s in result.stdout.splitlines() if '[TXSEL] ' in s]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout: raise RuntimeError(result.stdout[-5000:])
  v=json.loads(lines[0]);assert v['call'] and v['count']>=1 and v['slots']==['W1AW'] and not v['cat'] and not v['tx'],v
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
