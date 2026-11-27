#!/usr/bin/env python3
"""Compare explicit FT2 TX requests made after the safe start deadline."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=R/'build/decodium');p.add_argument('--qml',type=Path,default=R/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='Ft2Late'+secrets.token_hex(5)
probe='''
 property bool lateRequested: false
 property bool lateDeferred: false
 property bool lateTransmitted: false
 Connections {target:bridge;function onStatusMessage(message){if(String(message).indexOf("FT2 rimandato")>=0)lateDeferred=true}}
 Timer {interval:25;running:true;repeat:true;property int ticks:0;onTriggered:{
  if(++ticks<480)return;
  if(!lateRequested && Date.now()%3750>=1800 && Date.now()%3750<2200){lateRequested=true;bridge.sendTx(6);finish.start();}
  if(lateRequested && bridge.transmitting)lateTransmitted=true;
 }}
 Timer {id:finish;interval:5000;repeat:false;onTriggered:{console.log("[FT2-LATE] "+JSON.stringify({requested:lateRequested,deferred:lateDeferred,transmitted:lateTransmitted,enabled:bridge.txEnabled}));}}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-ft2late-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_FTX_SLOT_TIMING='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-mode','FT2','--lab-grid','JM75','--lab-callsign','9H1SR','--lab-audio-input','BlackHole 16ch','--lab-audio-output','BlackHole 2ch','--lab-monitor-ms','2500','--lab-quit-ms','24000']
  res=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
  lines=[x.split('[FT2-LATE] ',1)[1] for x in res.stdout.splitlines() if '[FT2-LATE] ' in x]
  if res.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in res.stdout:print(res.stdout[-8000:]);raise RuntimeError('late request probe failed')
  v=json.loads(lines[0]);print(json.dumps(v,sort_keys=True))
  if v!={'requested':True,'deferred':True,'transmitted':False,'enabled':False}:raise RuntimeError('unexpected state '+str(v))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
