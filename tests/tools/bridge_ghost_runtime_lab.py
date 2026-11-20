#!/usr/bin/env python3
"""Exercise decode filter controls in an isolated, offscreen app session."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav=ROOT/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'
rig='Clear'+secrets.token_hex(5)
probe='''
 Timer { interval:12000; running:true; repeat:false; onTriggered:{
 bridge.setHideGhostDecodes(true);
 function row(msg,db,time,mode){return {message:msg,displayMessage:msg,db:db,time:time,mode:mode};}
 var weak1=bridge.entryLooksLikeGhost(row("CQ 9H1SR JM75","-26","120030","FT8"));
 var weak2=!bridge.entryLooksLikeGhost(row("CQ 9H1SR JM75","-26","120045","FT8"));
 var malformed=bridge.entryLooksLikeGhost(row("CQ K1ABC R FN42","-10","120115","FT8"));
 var telemetry=bridge.entryLooksLikeGhost(row("A1B2C3D4E5","-18","120145","FT2"));
 var text=!bridge.entryLooksLikeGhost(row("random text","-25","120130","RTTY"));
 bridge.setHideGhostDecodes(false);var disabled=!bridge.entryLooksLikeGhost(row("A1B2C3D4E5","-18","120145","FT2"));
 console.log("[CLEAR-PROBE] "+JSON.stringify({weak1:weak1,weak2:weak2,malformed:malformed,telemetry:telemetry,text:text,disabled:disabled,tx:bridge.transmitting || bridge.txEnabled}));
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-clear-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','17000']
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)']+cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  lines=[x.split('[CLEAR-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[CLEAR-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0]=='timeout' or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('clear probe failed')
  v=json.loads(lines[0]);expected={'weak1':True,'weak2':True,'malformed':True,'telemetry':True,'text':True,'disabled':True,'tx':False}
  if v!=expected:raise RuntimeError('unexpected state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
