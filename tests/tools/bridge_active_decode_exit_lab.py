#!/usr/bin/env python3
"""Compare an FT8-to-FT4 mode change while WAV decoding is active."""
import argparse,json,os,secrets,shutil,subprocess,tempfile,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav=ROOT/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'
rig='ActiveExit'+secrets.token_hex(5)
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{bridge.openWavForDecode(__WAV__);watchDecode.start()}}
 Timer {id:watchDecode;interval:5;running:false;repeat:true;property int ticks:0;onTriggered:{
  if(bridge.decoding){console.log("[ACTIVE-EXIT] active");stop();Qt.quit();}
  else if(++ticks>=2000){console.log("[ACTIVE-EXIT] no-active-decode");stop()}
 }}
'''.replace('__WAV__',json.dumps(str(wav)))
try:
 with tempfile.TemporaryDirectory(prefix='decodium-active-switch-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','48000']
  start=time.monotonic()
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  elapsed=time.monotonic()-start
  if result.returncode or elapsed>=35 or '[ACTIVE-EXIT] active' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('active decode exit not observed')
  print(json.dumps({'active':True,'exit_code':result.returncode,'exited_before_lab_timeout':elapsed<35},sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
