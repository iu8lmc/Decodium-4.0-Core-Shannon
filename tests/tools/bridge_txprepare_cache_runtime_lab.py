#!/usr/bin/env python3
"""Exercise the actual QML TX selection, without CAT, monitoring or TX."""
import argparse, json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(); p.add_argument('--binary',type=Path,required=True); p.add_argument('--mode',choices=['FT8','FT4','FT2'],required=True); a=p.parse_args()
qml=root/'build/qml/decodium/Main.qml'; original=qml.read_text(); marker='GlassApplicationWindow {'
assert original.count(marker)==1
rig='TxCache'+secrets.token_hex(5)
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{
  if(bridge.catConnected || bridge.transmitting || bridge.audioOutputDevice!=="BlackHole 2ch" || bridge.audioInputDevice!=="BlackHole 16ch"){console.log("[TXSEL] unsafe");return;}
  bridge.mode="FT2";bridge.callsign="9H1SR";bridge.grid="JM75";bridge.mamMultiStream=false;bridge.currentTx=6;bridge.monitoring=true;watch.start();bridge.autoCqRepeat=true;
 }}
 Timer {id:watch;interval:50;repeat:true;property int ticks:0;property int starts:0;property bool previous:false;onTriggered:{
  if(bridge.catConnected || bridge.audioOutputDevice!=="BlackHole 2ch" || bridge.audioInputDevice!=="BlackHole 16ch"){bridge.halt();bridge.monitoring=false;console.log("[TXSEL] unsafe");stop();Qt.quit();return;}
  if(bridge.transmitting && !previous)starts++;previous=bridge.transmitting;
  if(starts>=2 && !bridge.transmitting){bridge.halt();bridge.monitoring=false;console.log("[TXSEL] "+JSON.stringify({starts:starts,cat:bridge.catConnected,tx:bridge.transmitting}));stop();Qt.quit();return;}
  if(++ticks>=1500){bridge.halt();bridge.monitoring=false;console.log("[TXSEL] timeout");stop();Qt.quit();}
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
  v=json.loads(lines[0]);assert v['starts']>=2 and not v['cat'] and not v['tx'],v
  logs=Path.home()/'Library/Application Support/IU8LMC/Decodium'
  diagnostic=''.join(p.read_text() for p in (logs/'decodium_diagnostic.1.log',logs/'decodium_diagnostic.log') if p.exists())
  segment=diagnostic.rsplit(rig,1)[-1]
  v['cacheHit']=any('ensure_tx_audio mode=FT2' in line and 'cache=1' in line for line in segment.splitlines())
  if not v['cacheHit']: raise RuntimeError('TX cache hit not observed: '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
