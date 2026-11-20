#!/usr/bin/env python3
"""Exercise the actual QML TX selection, without CAT, monitoring or TX."""
import argparse, json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(); p.add_argument('--binary',type=Path,required=True); a=p.parse_args()
qml=root/'build/qml/decodium/Main.qml'; original=qml.read_text(); marker='GlassApplicationWindow {'
assert original.count(marker)==1
rig='MamRecovery'+secrets.token_hex(5)
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{
  if(bridge.catConnected || bridge.monitoring || bridge.transmitting || bridge.audioOutputDevice!=="BlackHole 2ch"){console.log("[TXSEL] unsafe");return;}
  bridge.mode="FT2";bridge.callsign="9H1SR";bridge.grid="JM75";bridge.mamMultiStream=true;bridge.mamMaxStreams=1;bridge.multiAnswerMode=true;bridge.txEnabled=true;
  bridge.mamEnqueueClickedStation("K1ABC",1000);bridge.mamEnqueueClickedStation("K2AAA",2000);
  var d=new Date(),pad=function(n){return(n<10?"0":"")+n};var stamp=pad(d.getUTCHours())+pad(d.getUTCMinutes())+pad(d.getUTCSeconds());
  var ret=bridge.writeTextFile(__INJECT__,stamp+" -12 0.1 1500 ~  9H1SR W1AW -08\n");if(!ret.ok){console.log("[TXSEL] write-error");bridge.halt();return;}watch.start();
 }}
 Timer {id:watch;interval:200;repeat:true;property int ticks:0;onTriggered:{
  var q=bridge.callerQueue.slice();if(q.some(function(x){return x.indexOf("W1AW ")===0;})){
   var before=q;bridge.mamDropSlot("K1ABC");bridge.mamPromoteQueuedCaller("K2AAA");var slots=JSON.parse(JSON.stringify(bridge.mamActiveSlots));var after=bridge.callerQueue.slice();var active=bridge.txEnabled;bridge.halt();console.log("[TXSEL] "+JSON.stringify({before:before,after:after,slots:slots,enabled:active,cat:bridge.catConnected,tx:bridge.transmitting}));stop();return;
  } if(++ticks>=50){bridge.halt();console.log("[TXSEL] timeout");stop();}
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
  command=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-rx-inject',str(inject),'--lab-audio-output','BlackHole 2ch','--lab-quit-ms','25000']
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))']+command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=70)
  lines=[s.split('[TXSEL] ',1)[1] for s in result.stdout.splitlines() if '[TXSEL] ' in s]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout: raise RuntimeError(result.stdout[-5000:])
  v=json.loads(lines[0]);assert v['enabled'] and any(x.startswith('W1AW ') for x in v['before']) and [slot['call'] for slot in v['slots']]==['W1AW'] and v['slots'][0]['tx']==3 and any(x.startswith('K2AAA ') for x in v['after']) and not v['cat'] and not v['tx'],v
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
