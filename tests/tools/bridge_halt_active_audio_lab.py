#!/usr/bin/env python3
"""Exercise the actual QML TX selection, without CAT, monitoring or TX."""
import argparse, json, os, secrets, shutil, socket, subprocess, tempfile, time
import numpy as np
import sounddevice as sd
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(); p.add_argument('--binary',type=Path,required=True); p.add_argument('--kind',choices=['cw','ft2link','telemetry'],required=True); a=p.parse_args()
qml=root/'build/qml/decodium/Main.qml'; original=qml.read_text(); marker='GlassApplicationWindow {'
assert original.count(marker)==1
rig='SpecialTx'+secrets.token_hex(5)
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{
  if(bridge.catConnected || bridge.monitoring || bridge.transmitting || bridge.audioOutputDevice!=="BlackHole 2ch"){console.log("[TXSEL] unsafe");return;}
  bridge.mode="FT2";bridge.callsign="9H1SR";bridge.grid="JM75";watch.start();
  if("__KIND__"==="cw")bridge.sendCwAudio("TEST TEST TEST TEST TEST TEST",0,20);
  else if("__KIND__"==="ft2link"){var w=[];for(var i=0;i<192000;i++)w.push(0.25*Math.sin(i*0.2));watch.accepted=bridge.transmitFt2LinkAudio("TEST",w,{});}
  else {bridge.setSetting("WeatherApiEnable",false);bridge.currentTx=5;bridge.sendStationTelemetry();}
 }}
 Timer {id:watch;interval:50;repeat:true;property int ticks:0;property bool seen:false;property bool accepted:true;property double haltAt:0;onTriggered:{
  if(bridge.catConnected || bridge.audioOutputDevice!=="BlackHole 2ch"){bridge.halt();console.log("[TXSEL] unsafe");stop();Qt.quit();return;}
  if(bridge.transmitting){seen=true;stop();haltTimer.start();return;}
  if(++ticks>=1000){bridge.halt();console.log("[TXSEL] timeout");stop();Qt.quit();}
 }}
 Timer {id:haltTimer;interval:1500;repeat:false;onTriggered:{watch.haltAt=Date.now();bridge.halt();settle.start();}}
 Timer {id:settle;interval:2000;repeat:false;onTriggered:{console.log("[TXSEL] "+JSON.stringify({seen:watch.seen,accepted:watch.accepted,cat:bridge.catConnected,stopped:!bridge.transmitting&&!bridge.txEnabled,tx:bridge.transmitting,enabled:bridge.txEnabled,haltAt:watch.haltAt}));Qt.quit();}}
'''.replace('__KIND__',a.kind)
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
  device=next(i for i,d in enumerate(sd.query_devices()) if d['name']=='BlackHole 2ch' and d['max_input_channels']>=2)
  captureStart=time.time()
  audio=sd.rec(22*48000,samplerate=48000,channels=2,device=device,dtype='float32')
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))']+command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=260)
  sd.wait()
  lines=[s.split('[TXSEL] ',1)[1] for s in result.stdout.splitlines() if '[TXSEL] ' in s]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout: raise RuntimeError(result.stdout[-5000:])
  v=json.loads(lines[0]);assert v['seen'] and v['accepted'] and not v['cat'],v
  at=int((v['haltAt']/1000-captureStart)*48000)
  pre=audio[max(0,at-24000):max(0,at-4800)]
  post=audio[at+24000:at+72000]
  v['rmsBefore']=float(np.sqrt(np.mean(pre*pre))) if len(pre) else 0.0
  v['rmsAfter']=float(np.sqrt(np.mean(post*post))) if len(post) else 1.0
  assert v['stopped'] and v['rmsBefore']>0.005 and v['rmsAfter']<0.002,v
  logs=Path.home()/'Library/Application Support/IU8LMC/Decodium'
  diagnostic=''.join(p.read_text() for p in (logs/'decodium_diagnostic.1.log',logs/'decodium_diagnostic.log') if p.exists())
  segment=diagnostic.rsplit(rig,1)[-1]
  expected={'cw':'CW','ft2link':'FT2LINK','telemetry':'FT2'}[a.kind]
  v['audioPrepared']=('[TX-TL] ensure_tx_audio mode='+expected) in segment
  if not v['audioPrepared']: raise RuntimeError('special audio preparation not observed: '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
