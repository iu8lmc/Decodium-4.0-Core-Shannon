#!/usr/bin/env python3
"""Log a synthetic local QSO in an isolated test profile without CAT or network."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='QsoDone'+secrets.token_hex(5)
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{var ok=bridge.createLogbook("Synthetic__RIG__",false);var safe=ok && String(bridge.activeLogbookPath).indexOf("/.qttest/")>=0 && !bridge.transmitting && !bridge.txEnabled && !bridge.catConnected;if(!safe){console.log("[QSODONE-PROBE] unsafe");return;}bridge.setSetting("PromptToLog",false);bridge.dxCall="K1ABC";bridge.dxGrid="FN42";bridge.resetStandardTxMessages();bridge.logQso();checkDone.start();}}
 Timer {id:checkDone;interval:500;repeat:true;property int n:0;onTriggered:{if(bridge.qsoCount>0){console.log("[QSODONE-PROBE] "+JSON.stringify({count:bridge.qsoCount,clear:bridge.dxCall==="",tx:bridge.txEnabled || bridge.transmitting}));stop();}else if(++n>35){console.log("[QSODONE-PROBE] timeout");stop();}}}
'''.replace('__RIG__',rig)
try:
 with tempfile.TemporaryDirectory(prefix='decodium-qsodone-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  app=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','45000'];cmd=['sandbox-exec','-p','(version 1)(allow default)(deny network*)']+app
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=85)
  lines=[x.split('[QSODONE-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[QSODONE-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0] in ('unsafe','timeout') or '[LAB] CAT auto-connect disabled' not in result.stdout:raise RuntimeError('probe failed '+result.stdout[-5000:])
  v=json.loads(lines[0]);expected={'count':1,'clear':True,'tx':False}
  if v!=expected:raise RuntimeError('unexpected '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
