#!/usr/bin/env python3
"""Exercise the actual QML TX selection, without CAT, monitoring or TX."""
import argparse, json, os, secrets, shutil, socket, subprocess, tempfile
import sqlite3,time
import numpy as np
import sounddevice as sd
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(); p.add_argument('--binary',type=Path,required=True); p.add_argument('--mode',choices=['FT8','FT4'],required=True); a=p.parse_args()
qml=root/'build/qml/decodium/Main.qml'; original=qml.read_text(); marker='GlassApplicationWindow {'
assert original.count(marker)==1
rig='MamModes'+secrets.token_hex(5);rxrig=rig+'RX';rx=None
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{
  if(bridge.callsign!=="9H1SR")return;
  if(bridge.catConnected || bridge.transmitting || bridge.audioOutputDevice!=="BlackHole 2ch" || bridge.audioInputDevice!=="BlackHole 16ch"){console.log("[TXSEL] unsafe");return;}
  bridge.mode="__MODE__";bridge.callsign="9H1SR";bridge.grid="JM75";bridge.mamMultiStream=true;bridge.mamMaxStreams=1;bridge.multiAnswerMode=true;bridge.monitoring=true;bridge.txEnabled=true;bridge.mamEnqueueClickedStation("W1AW",1500);watch.start();
 }}
 Timer {id:watch;interval:50;repeat:true;property int ticks:0;property bool seenTx:false;property var streams:[];onTriggered:{
  if(bridge.catConnected || bridge.audioOutputDevice!=="BlackHole 2ch" || bridge.audioInputDevice!=="BlackHole 16ch"){bridge.halt();bridge.monitoring=false;console.log("[TXSEL] unsafe");stop();return;}
  if(bridge.transmitting){seenTx=true;var arr=bridge.mamTxStreams;if(arr.length)streams=JSON.parse(JSON.stringify(arr));}
  if(seenTx && !bridge.transmitting){var p=streams.map(function(x){return {message:x.message,freq:x.freq};});bridge.halt();bridge.monitoring=false;console.log("[TXSEL] "+JSON.stringify({streams:p,cat:bridge.catConnected,tx:bridge.transmitting}));stop();Qt.quit();return;}
  if(++ticks>=1200){bridge.halt();bridge.monitoring=false;console.log("[TXSEL] timeout");stop();}
 }}
'''.replace('__MODE__',a.mode)
try:
 with tempfile.TemporaryDirectory(prefix='decodium-mamrecovery-lab-') as tmp:
  inject=Path(tmp)/'rx-inject.txt';inject.write_text('');probe=probe.replace('__INJECT__',json.dumps(str(inject)))
  sockets=[]
  for port in (2237,2239):
   s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',port));sockets.append(s)
  guard=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);guard.bind(('127.0.0.1',2238));guard.close()
  qml.write_text(original.replace(marker,marker+probe,1))
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  command=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','9H1SR','--lab-rx-inject',str(inject),'--lab-audio-input','BlackHole 16ch','--lab-audio-output','BlackHole 2ch','--lab-quit-ms','235000']
  policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
  rxenv=env.copy();rxenv['DECODIUM_LAB_SETTINGS_DIR']=str(Path(tmp)/'receiver');Path(rxenv['DECODIUM_LAB_SETTINGS_DIR']).mkdir();rxenv['DECODIUM_FORCE_STANDALONE_UDP']='1'
  rxlog=open(Path(tmp)/'receiver.log','w');rx=subprocess.Popen(['sandbox-exec','-p',policy,str(a.binary.resolve()),'--test-mode','--rig-name',rxrig,'--lab-no-cat','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 16ch','--lab-monitor-ms','2500','--lab-quit-ms','120000'],env=rxenv,stdout=rxlog,stderr=subprocess.STDOUT)
  time.sleep(5)
  device=next(i for i,d in enumerate(sd.query_devices()) if d['name']=='BlackHole 2ch' and d['max_input_channels']>=2)
  audio=sd.rec(80*48000,samplerate=48000,channels=2,device=device,dtype='float32')
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))']+command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=260)
  sd.stop()
  step=4800; rms=np.sqrt(np.mean(np.square(audio[:len(audio)//step*step].reshape(-1,step,2)),axis=(1,2)))
  lines=[s.split('[TXSEL] ',1)[1] for s in result.stdout.splitlines() if '[TXSEL] ' in s]
  if result.returncode or len(lines)!=1 or '[LAB-PERIODIC-MAM]' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout: raise RuntimeError(result.stdout[-5000:])
  v=json.loads(lines[0]);assert len(v['streams'])==1 and v['streams'][0]['freq']==1500 and 'W1AW' in v['streams'][0]['message'] and not v['cat'] and not v['tx'],v
  peak=float(np.max(np.abs(audio)));windows=int((rms>0.003).sum());assert peak>0.01 and windows>=100,(peak,windows);v['audioPeak']=round(peak,3);v['audioWindows']=windows
  db=Path.home()/'.qttest/Library/Application Support/IU8LMC'/('Decodium - '+rxrig+' - test')/'db.sqlite';messages=[]
  for attempt in range(80):
   if db.is_file():
    try:
     with sqlite3.connect('file:'+str(db)+'?mode=ro',uri=True) as conn:messages=[row[0] for row in conn.execute("SELECT message FROM decodes WHERE message LIKE '%9H1SR%'")]
    except sqlite3.Error:pass
   if any('W1AW' in msg and '9H1SR' in msg for msg in messages):break
   time.sleep(.25)
  assert any('W1AW' in msg and '9H1SR' in msg for msg in messages),messages;v['received']=sorted(set(messages));print(json.dumps(v,sort_keys=True))
finally:
 if rx is not None and rx.poll() is None:rx.terminate();rx.wait(timeout=10)
 if 'rxlog' in locals():rxlog.close()
 sd.stop()
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  for name in (rig,rxrig):
   path=parent/('Decodium - '+name+' - test')
   if path.is_dir():shutil.rmtree(path)
