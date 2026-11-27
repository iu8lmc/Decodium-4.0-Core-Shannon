#!/usr/bin/env python3
"""Exercise the actual QML TX selection, without CAT, monitoring or TX."""
import argparse, json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(); p.add_argument('--binary',type=Path,required=True); a=p.parse_args()
qml=root/'build/qml/decodium/Main.qml'; original=qml.read_text(); marker='GlassApplicationWindow {'
assert original.count(marker)==1
rig='MamEnqueue'+secrets.token_hex(5)
probe='''
 Timer { interval:10000; running:true; repeat:false; onTriggered:{
  if(bridge.catConnected || bridge.monitoring || bridge.transmitting || bridge.audioOutputDevice!=="BlackHole 2ch"){console.log("[TXSEL] unsafe");return;}
  bridge.mode="FT8";bridge.callsign="9H1SR";bridge.grid="JM75";bridge.mamMultiStream=true;bridge.mamMaxStreams=2;bridge.multiAnswerMode=true;bridge.txEnabled=true;
  bridge.mamEnqueueClickedStation("9H1SR",1000);bridge.mamEnqueueClickedStation("K1ABC",1000);bridge.mamEnqueueClickedStation("K1ABC",1100);bridge.mamEnqueueClickedStation("W1AW",1020);bridge.mamEnqueueClickedStation("K2AAA",5000);
  var queue=bridge.callerQueue.slice();var slots=JSON.parse(JSON.stringify(bridge.mamActiveSlots));var enabled=bridge.txEnabled;bridge.halt();
  console.log("[TXSEL] "+JSON.stringify({queue:queue,slots:slots,enabled:enabled,cat:bridge.catConnected,tx:bridge.transmitting}));
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-txselection-lab-') as tmp:
  sockets=[]
  for port in (2237,2239):
   s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',port));sockets.append(s)
  guard=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);guard.bind(('127.0.0.1',2238));guard.close()
  qml.write_text(original.replace(marker,marker+probe,1))
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  command=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-audio-output','BlackHole 2ch','--lab-quit-ms','16000']
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))']+command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=70)
  lines=[s.split('[TXSEL] ',1)[1] for s in result.stdout.splitlines() if '[TXSEL] ' in s]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout: raise RuntimeError(result.stdout[-5000:])
  v=json.loads(lines[0]);assert v['enabled'] and v['queue']==['K2AAA 4000 -10'] and [slot['call'] for slot in v['slots']]==['K1ABC','W1AW'] and v['slots'][0]['freq']==1000 and v['slots'][1]['freq']>1020 and not v['cat'] and not v['tx'],v
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
