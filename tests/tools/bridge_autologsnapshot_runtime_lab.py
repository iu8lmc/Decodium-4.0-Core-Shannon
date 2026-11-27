#!/usr/bin/env python3
"""Observe automatic TX5 snapshot without committing or exporting a QSO."""
import argparse,json,os,re,secrets,shutil,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=R/'build/decodium');p.add_argument('--qml',type=Path,default=R/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='AutoSnapshot'+secrets.token_hex(5)
probe='''
 property bool snapshotTxStarted: false
 property bool snapshotPromptSeen: false
 property bool snapshotCommitted: false
 Connections {target:bridge
  function onQsoLogCompleted(call){snapshotCommitted=true;console.log("[SNAPSHOT-COMMIT] "+call)}
  function onLogQsoPromptRequested(){
   snapshotPromptSeen=true;var p=bridge.pendingLogQsoPreview();bridge.dxCall="K2DEF";var preserved=bridge.pendingLogQsoPreview();
   console.log("[SNAPSHOT-PROBE] "+JSON.stringify({started:snapshotTxStarted,call:p.call,grid:p.grid,mode:p.mode,received:p.rcvd,preserved:preserved.call===p.call,committed:snapshotCommitted}));
   bridge.notifyLogPromptShown();bridge.rejectPromptedLogQso();
  }
 }
 Timer {interval:200;running:true;repeat:true;onTriggered:{if(bridge.transmitting)snapshotTxStarted=true}}
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
  bridge.setSetting("PromptToLog",true);bridge.dxCall="K1ABC";bridge.dxGrid="FN42";bridge.reportReceived="-08";bridge.resetStandardTxMessages();
  var safe=bridge.getSetting("PromptToLog",false)===true && bridge.audioOutputDevice==="BlackHole 2ch" && !bridge.monitoring && !bridge.transmitting && !bridge.txEnabled;
  console.log("[SNAPSHOT-SAFETY] "+safe+" tx5="+bridge.tx5);
  if(safe)bridge.sendTx(5);
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-autosnapshot-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_FORCE_TX_IMMEDIATE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','9H1SR','--lab-grid','JM75','--lab-mode','FT8','--lab-audio-input','BlackHole 16ch','--lab-audio-output','BlackHole 2ch','--lab-quit-ms','45000']
  res=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=75)
  lines=[x.split('[SNAPSHOT-PROBE] ',1)[1] for x in res.stdout.splitlines() if '[SNAPSHOT-PROBE] ' in x]
  if res.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in res.stdout or '[SNAPSHOT-SAFETY] true' not in res.stdout or '[SNAPSHOT-COMMIT]' in res.stdout:
   print(res.stdout[-10000:]);raise RuntimeError('snapshot application probe failed')
  v=json.loads(lines[0]);expected={'started':True,'call':'K1ABC','grid':'FN42','mode':'FT8','received':'-08','preserved':True,'committed':False}
  if v!=expected:raise RuntimeError('unexpected state '+str(v))
  adif=Path.home()/'.qttest/Library/Application Support/IU8LMC'/('Decodium - '+rig+' - test')/'decodium_log.adi'
  if adif.exists() and re.search(r'<CALL(?::[^>]*)?>\s*K1ABC\b',adif.read_text(errors='replace'),re.IGNORECASE):raise RuntimeError('synthetic QSO unexpectedly committed')
  v['adif_clean']=True;print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
