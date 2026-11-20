#!/usr/bin/env python3
"""Compare the update-check invocation in isolated application sessions."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Connections { target: bridge; function onStatusMessage(msg) { if (msg.indexOf("Update checks are") === 0) console.log("[UPDATE-PROBE] "+JSON.stringify({message:msg,offline:bridge.offlineMode,available:bridge.updateAvailable,latest:bridge.latestVersion})); } }
 Timer {interval:10000;running:true;repeat:false;onTriggered:bridge.checkForUpdates()}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-update-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','UpdateProbe'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','23000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  matches=[json.loads(line.split('[UPDATE-PROBE] ',1)[1]) for line in result.stdout.splitlines() if '[UPDATE-PROBE] ' in line]
  if result.returncode or len(matches)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-12000:]);raise RuntimeError('update probe failed')
  print(json.dumps(matches[0],sort_keys=True))
finally:qml.write_text(original)
