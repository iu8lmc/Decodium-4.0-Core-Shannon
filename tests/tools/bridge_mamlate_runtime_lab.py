#!/usr/bin/env python3
"""Exercise the actual QML TX selection, without CAT, monitoring or TX."""
import argparse, json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(); p.add_argument('--binary',type=Path,required=True); a=p.parse_args()
qml=root/'build/qml/decodium/Main.qml'; original=qml.read_text(); marker='GlassApplicationWindow {'
assert original.count(marker)==1
rig='MamLate'+secrets.token_hex(5)
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{
  if(bridge.catConnected || bridge.transmitting || bridge.audioOutputDevice!=="BlackHole 2ch" || bridge.audioInputDevice!=="BlackHole 16ch"){console.log("[TXSEL] unsafe");return;}
  bridge.mode="FT2";bridge.callsign="9H1SR";bridge.grid="JM75";bridge.mamMultiStream=true;bridge.mamMaxStreams=1;bridge.maxCallerRetries=1;bridge.multiAnswerMode=true;bridge.monitoring=true;bridge.txEnabled=true;bridge.mamEnqueueClickedStation("W1AW",1500);
  var d=new Date(),pad=function(n){return(n<10?"0":"")+n};var stamp=pad(d.getUTCHours())+pad(d.getUTCMinutes())+pad(d.getUTCSeconds());watch.first=stamp+" -12 0.1 1500 ~  9H1SR W1AW -08\n";var ret=bridge.writeTextFile(__INJECT__,watch.first);if(!ret.ok){bridge.halt();console.log("[TXSEL] write-error");return;}watch.start();
 }}
 Timer {id:watch;interval:250;repeat:true;property int ticks:0;property int stage:0;property string first:"";onTriggered:{
  if(bridge.catConnected || bridge.audioOutputDevice!=="BlackHole 2ch" || bridge.audioInputDevice!=="BlackHole 16ch"){bridge.halt();bridge.monitoring=false;console.log("[TXSEL] unsafe");stop();return;}
  var slots=JSON.parse(JSON.stringify(bridge.mamActiveSlots));var q=bridge.callerQueue.slice();
  if(stage===0 && slots.length===1 && slots[0].call==="W1AW" && slots[0].tx===3){stage=1;}
  if(stage===1 && slots.length===0){bridge.mamEnqueueClickedStation("K1ABC",1000);bridge.mamEnqueueClickedStation("K2AAA",2000);var d=new Date(),pad=function(n){return(n<10?"0":"")+n};var stamp=pad(d.getUTCHours())+pad(d.getUTCMinutes())+pad(d.getUTCSeconds());var ret=bridge.writeTextFile(__INJECT__,first+stamp+" -12 0.1 1500 ~  9H1SR W1AW R-08\n");if(!ret.ok){bridge.halt();bridge.monitoring=false;console.log("[TXSEL] write-error");stop();return;}stage=2;}
  if(stage===2 && q.some(function(x){return x.indexOf("W1AW ")===0;})){bridge.mamDropSlot("K1ABC");bridge.mamPromoteQueuedCaller("K2AAA");slots=JSON.parse(JSON.stringify(bridge.mamActiveSlots));q=bridge.callerQueue.slice();var v={calls:slots.map(function(x){return x.call;}),slotTx:slots.map(function(x){return x.tx;}),report:slots.map(function(x){return x.rptRcvd;}),queue:q,cat:bridge.catConnected};bridge.halt();bridge.monitoring=false;v.tx=bridge.transmitting;console.log("[TXSEL] "+JSON.stringify(v));stop();Qt.quit();return;}
  if(++ticks>=840){bridge.halt();bridge.monitoring=false;console.log("[TXSEL] timeout-stage-"+stage);stop();}
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-mamrecovery-lab-') as tmp:
  inject=Path(tmp)/'rx-inject.txt';inject.write_text('');probe=probe.replace('__INJECT__',json.dumps(str(inject)))
  sockets=[]
  for port in (2237,2239):
   s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',port));sockets.append(s)
  guard=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);guard.bind(('127.0.0.1',2238));guard.close()
  qml.write_text(original.replace(marker,marker+probe,1))
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  command=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-rx-inject',str(inject),'--lab-audio-input','BlackHole 16ch','--lab-audio-output','BlackHole 2ch','--lab-quit-ms','235000']
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))']+command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=260)
  lines=[s.split('[TXSEL] ',1)[1] for s in result.stdout.splitlines() if '[TXSEL] ' in s]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout: raise RuntimeError(result.stdout[-5000:])
  v=json.loads(lines[0]);assert v['calls']==['W1AW'] and v['slotTx']==[4] and v['report']==['-08'] and any(x.startswith('K2AAA ') for x in v['queue']) and not v['cat'] and not v['tx'],v
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
