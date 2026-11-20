#!/usr/bin/env python3
"""Decode a WAV, then clear both decode lists in isolated app sessions."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav=ROOT/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'
rig='Clear'+secrets.token_hex(5)
probe='''
 property bool mapContactSeen: false
 property int resetCount: 0
 Connections {target:bridge;function onWorldMapResetRequested(){resetCount++} function onWorldMapContactAdded(call,sourceGrid,destinationGrid,role){if(String(call).toUpperCase()==="K1ABC")mapContactSeen=true}}
 Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.setWorldMapConsumerReady(watchDecode,true);bridge.openWavForDecode(__WAV__);watchDecode.start()}}
 Timer {id:watchDecode;interval:250;running:false;repeat:true;property int ticks:0;onTriggered:{
  if(mapContactSeen){var old=resetCount;bridge.clearDecodeList();console.log("[CLEAR-PROBE] "+JSON.stringify({before:true,band:resetCount>old,rx:bridge.decodeList.length===0,tx:bridge.transmitting}));stop();return;}
  if(++ticks>=100){console.log("[CLEAR-PROBE] timeout");stop()}
 }}
'''.replace('__WAV__',json.dumps(str(wav)))
try:
 with tempfile.TemporaryDirectory(prefix='decodium-clear-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JN70','--lab-mode','FT8','--lab-quit-ms','45000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  lines=[x.split('[CLEAR-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[CLEAR-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0]=='timeout' or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('clear probe failed')
  v=json.loads(lines[0]);expected={'before':True,'band':True,'rx':True,'tx':False}
  if v!=expected:raise RuntimeError('unexpected state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
