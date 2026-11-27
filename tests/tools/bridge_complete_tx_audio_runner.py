#!/usr/bin/env python3
"""Exercise the actual QML TX selection, without CAT, monitoring or TX."""
import argparse, json, os, secrets, shutil, socket, subprocess, tempfile
import numpy as np
import sounddevice as sd
import time
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(); p.add_argument('--binary',type=Path,required=True); p.add_argument('--kind',choices=['cw','ft2link','telemetry'],required=True); p.add_argument('--expect-modulator',action='store_true'); p.add_argument('--two-cw',action='store_true'); a=p.parse_args()
qml=root/'build/qml/decodium/Main.qml'; original=qml.read_text(); marker='GlassApplicationWindow {'
assert original.count(marker)==1
rig='SpecialTx'+secrets.token_hex(5)
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{
  if(bridge.catConnected || bridge.monitoring || bridge.transmitting || bridge.audioOutputDevice!=="BlackHole 2ch"){console.log("[TXSEL] unsafe");return;}
  bridge.mode="FT2";bridge.callsign="9H1SR";bridge.grid="JM75";watch.start();
  if("__KIND__"==="cw")bridge.sendCwAudio("TEST",0,30);
  else if("__KIND__"==="ft2link"){var w=[];for(var i=0;i<192000;i++)w.push(0.25*Math.sin(i*0.2));watch.accepted=bridge.transmitFt2LinkAudio("TEST",w,{});}
  else {bridge.setSetting("WeatherApiEnable",false);bridge.currentTx=5;bridge.sendStationTelemetry();}
 }}
 Timer {id:watch;interval:50;repeat:true;property int ticks:0;property bool seen:false;property bool accepted:true;property int activeTicks:0;property real haltAt:0;onTriggered:{
  if(bridge.catConnected || bridge.audioOutputDevice!=="BlackHole 2ch"){bridge.halt();console.log("[TXSEL] unsafe");stop();Qt.quit();return;}
  if(bridge.transmitting){seen=true;++activeTicks;return;}
  if(seen){haltAt=Date.now();stop();settle.start();return;}
  if(++ticks>=1000){bridge.halt();console.log("[TXSEL] timeout");stop();Qt.quit();}
 }}
 Timer {id:settle;interval:2000;repeat:false;property int completed:0;property real secondStart:0;onTriggered:{
  if (__TWO_CW__ && completed===0 && !bridge.transmitting && !bridge.txEnabled) {completed=1;watch.seen=false;watch.activeTicks=0;watch.ticks=0;watch.start();secondStart=Date.now();bridge.sendCwAudio("EE",0,25);return;}
  console.log("[TXSEL] "+JSON.stringify({seen:watch.seen,accepted:watch.accepted,haltAt:watch.haltAt,secondStart:secondStart,cat:bridge.catConnected,stopped:!bridge.transmitting&&!bridge.txEnabled,tx:bridge.transmitting,enabled:bridge.txEnabled,completed:completed+1}));Qt.quit();}}
'''.replace('__KIND__',a.kind).replace('__TWO_CW__','true' if a.two_cw else 'false')
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
  samples=sd.rec((35 if a.two_cw else 25)*48000,samplerate=48000,channels=2,device=device,dtype='float32')
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))']+command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=260)
  sd.wait();peak=float(np.max(np.abs(samples)));assert peak>0.003, 'no virtual audio captured'
  lines=[s.split('[TXSEL] ',1)[1] for s in result.stdout.splitlines() if '[TXSEL] ' in s]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout: raise RuntimeError(result.stdout[-5000:])
  v=json.loads(lines[0]);assert v['seen'] and v['accepted'] and not v['cat'],v
  if a.two_cw and v['completed'] != 2: raise RuntimeError('second CW transmission did not complete: '+str(v))
  secondStart=v.pop('secondStart')
  if a.two_cw:
   start=max(0,int((secondStart/1000.0-captureStart)*48000));end=min(len(samples),int((v['haltAt']/1000.0-captureStart)*48000))
   if end<=start: raise RuntimeError('invalid second CW capture window')
   v['secondPeak']=float(np.max(np.abs(samples[start:end])))
   if v['secondPeak']<=0.003: raise RuntimeError('second CW request produced no captured audio')
   # Two E characters should contain two tone bursts separated by a letter
   # gap. Use 10 ms RMS windows and discard isolated sub-20 ms fluctuations.
   mono=samples[start:end].mean(axis=1); blocks=len(mono)//480
   envelope=np.sqrt(np.mean(np.square(mono[:blocks*480].reshape(blocks,480)),axis=1))
   active=envelope>0.02; runs=[]; begin=None
   for i,on in enumerate(np.append(active,False)):
    if on and begin is None: begin=i
    elif not on and begin is not None:
     if i-begin>=2:runs.append((begin,i))
     begin=None
   v['secondBursts']=len(runs)
   if len(runs)!=2 or runs[1][0]-runs[0][1]<2: raise RuntimeError('second CW waveform does not contain two separated tone bursts: '+str(runs))
  logs=Path.home()/'Library/Application Support/IU8LMC/Decodium'
  diagnostic=''.join(p.read_text() for p in (logs/'decodium_diagnostic.1.log',logs/'decodium_diagnostic.log') if p.exists())
  segment=diagnostic.rsplit(rig,1)[-1]
  expected={'cw':'CW','ft2link':'FT2LINK','telemetry':'FT2'}[a.kind]
  haltOffset=v.pop('haltAt')/1000.0-captureStart;tail=samples[int((haltOffset+0.8)*48000):int((haltOffset+2.0)*48000)];assert len(tail)>0,'missing post-completion audio';v['tailPeak']=float(np.max(np.abs(tail)));v['audioPeak']=peak;assert v['tailPeak']<0.003, f'audio persists after completion: {v["tailPeak"]}';v['completeSeen']='[LAB-COMPLETE-TX]' in result.stdout
  v['modulatorSeen']='[LAB-MODULATOR-FINISH]' in result.stdout
  if not (v['modulatorSeen'] if a.expect_modulator else v['completeSeen']):
   relevant=[line for line in result.stdout.splitlines() if any(token in line.lower() for token in ('[tx-tl]', 'ft2-link', 'completetx', 'stoptx', 'soundoutput', 'sound output', '[lab-modulator-finish]'))]
   raise RuntimeError('completeTxPlayback not reached; relevant app output:\n'+'\n'.join(relevant[-50:]))
  v['audioPrepared']=('[TX-TL] ensure_tx_audio mode='+expected) in segment
  if not v['audioPrepared']: raise RuntimeError('special audio preparation not observed: '+str(v))
  if not v['stopped']:
   relevant=[line for line in result.stdout.splitlines() if any(token in line.lower() for token in ('[tx-tl]', ' cw', 'sendcwaudio', 'modulator', 'transmitting', 'starttx', 'stoptx'))]
   raise RuntimeError('TX active after completion: '+str(v)+'\n'+'\n'.join(relevant[-70:]))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
