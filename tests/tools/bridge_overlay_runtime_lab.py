#!/usr/bin/env python3
"""Check the actual diagnostic overlay timer and counters in isolated sessions."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args()
qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='Overlay'+secrets.token_hex(5)
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
  bridge.devOverlayActive=true;
  for(var i=0;i<40;i++)bridge.recordFrameTimestamp();
  bridge.aggiungiRigaRtty("CQ OVERLAY TEST",0.7,1200);
  measure.start();
 }}
 Timer {id:measure;interval:40;repeat:true;property int ticks:0;onTriggered:{
  if(bridge.decodeRateCommittedHz<4 && ++ticks<50)return;stop();
  var samples=bridge.frameTimeSamples.length; var committed=bridge.totalDecodesCommitted>=1;
  var rate=bridge.decodeRateCommittedHz>=4; var enabled=bridge.devOverlayActive;
  bridge.devOverlayActive=false;var disabled=!bridge.devOverlayActive;
  bridge.devOverlayActive=true;var reset=bridge.frameTimeSamples.length===0 && bridge.decodeRateCommittedHz===0;
  bridge.devOverlayActive=false;
  console.log("[OVERLAY-PROBE] "+JSON.stringify({samples:samples===32,committed:committed,rate:rate,enabled:enabled,disabled:disabled,reset:reset,tx:bridge.transmitting}));
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-overlay-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','18000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
  lines=[x.split('[OVERLAY-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[OVERLAY-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:print(result.stdout[-12000:]);raise RuntimeError('overlay probe failed')
  v=json.loads(lines[0]);expected={'samples':True,'committed':True,'rate':True,'enabled':True,'disabled':True,'reset':True,'tx':False}
  if v!=expected:raise RuntimeError('unexpected state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
