#!/usr/bin/env python3
"""Verify decoded callsign and grid in an isolated application session."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--variant',choices=('a','b'),default='a');p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav=ROOT/('tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav' if a.variant=='a' else 'tests/bridge_refactor_fixtures/ft8-9h1sr-1300.wav')
wanted='CQ K1ABC FN42' if a.variant=='a' else 'CQ 9H1SR JM75'
rig='Clear'+secrets.token_hex(5)
probe = r'''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
  if(bridge.catConnected || bridge.audioOutputDevice!=="BlackHole 2ch" || bridge.monitoring){console.log("[CLEAR-PROBE] unsafe");return;}
  bridge.filtersBypassed=true;console.log("[WAIT-BEGIN] __RIG__");bridge.openWavForDecode(__WAV__);watch.start();
 }}
 Timer {id:watch;interval:100;repeat:true;property int ticks:0;onTriggered:{
  if(String(bridge.dxCall)==__CALL__){var v={call:bridge.dxCall,grid:bridge.dxGrid,tx1:bridge.tx1,tx2:bridge.tx2,output:bridge.audioOutputDevice,connected:bridge.catConnected};bridge.halt();v.txAfter=bridge.transmitting;v.enabledAfter=bridge.txEnabled;console.log("[CLEAR-PROBE] "+JSON.stringify(v));stop();return;}
  if(++ticks>=160){console.log("[CLEAR-PROBE] timeout");stop();}
 }}
'''.replace('__WAV__',json.dumps(str(wav))).replace('__CALL__',json.dumps('K1ABC' if a.variant=='a' else '9H1SR')).replace('__RIG__',rig)

try:
 with tempfile.TemporaryDirectory(prefix='decodium-clear-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-audio-output','BlackHole 2ch','--lab-standard-wait-pounce-ms','6000','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','32000']
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)']+cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  lines=[x.split('[CLEAR-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[CLEAR-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0]=='timeout' or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('clear probe failed')
  v=json.loads(lines[0]);call='K1ABC' if a.variant=='a' else '9H1SR'
  if v['call']!=call or v['connected'] or v['output']!='BlackHole 2ch' or v['txAfter'] or v['enabledAfter'] or not v['tx1'] or not v['tx2']:raise RuntimeError('unexpected positive wait-pounce '+str(v))
  logdir=Path.home()/'Library/Application Support/IU8LMC/Decodium';diagnostic=''.join(p.read_text() for p in (logdir/'decodium_diagnostic.1.log',logdir/'decodium_diagnostic.log') if p.exists());segment=diagnostic.rsplit('[WAIT-BEGIN] '+rig,1)[-1].split('[CLEAR-PROBE]',1)[0]
  if 'Wait & Pounce trigger' not in segment:raise RuntimeError('positive guard not observed')
  print(json.dumps(v,sort_keys=True))

finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
