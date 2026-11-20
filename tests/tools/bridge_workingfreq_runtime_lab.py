#!/usr/bin/env python3
"""Compare all read-only working-frequency rows in an isolated app."""
import argparse, hashlib, json, os, secrets, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--variant',choices=('a','b'),required=True);p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args()
qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered: {
   console.log("[WORKING-FREQUENCIES] "+JSON.stringify({rows:bridge.workingFrequencyRows(),modes:bridge.frequencyModeOptions(),regions:bridge.frequencyRegionOptions(),bands:bridge.frequencyBandOptions()}));
 }}
'''

try:
 with tempfile.TemporaryDirectory(prefix='decodium-sstv-state-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','SstvState'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','26000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  matches=[line.split('[WORKING-FREQUENCIES] ',1)[1] for line in result.stdout.splitlines() if '[WORKING-FREQUENCIES] ' in line]
  if result.returncode or len(matches)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-12000:]);raise RuntimeError('working frequencies read probe failed')
  payload=json.loads(matches[0]);canonical=json.dumps(payload,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode('utf-8')
  print(a.variant+': '+json.dumps({'rows':len(payload['rows']),'modes':len(payload['modes']),'regions':len(payload['regions']),'bands':len(payload['bands']),'sha256':hashlib.sha256(canonical).hexdigest()},sort_keys=True))
finally:qml.write_text(original)
