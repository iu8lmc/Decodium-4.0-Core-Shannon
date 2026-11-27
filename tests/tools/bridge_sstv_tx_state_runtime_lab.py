#!/usr/bin/env python3
"""Compare read-only SSTV TX state in an isolated app; never start TX."""
import argparse, os, secrets, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--variant',choices=('a','b'),required=True);p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args()
qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered: {
   bridge.mode=__MODE__;
   let d=bridge.sstvTxDiagnostics;
   console.log("[SSTV-TX-STATE] "+JSON.stringify({ready:bridge.sstvTxCanStart,reason:bridge.sstvTxUnavailableReason,active:bridge.sstvTxActive,state:bridge.sstvTxState,progress:bridge.sstvTxProgress,error:bridge.sstvTxError,preflight:d.preflightReady,blockers:d.preflightBlockers}));
 }}
'''.replace('__MODE__','"FT8"' if a.variant=='a' else '"SSTV"')
try:
 with tempfile.TemporaryDirectory(prefix='decodium-sstv-state-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','SstvState'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','26000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  matches=[line.split('[SSTV-TX-STATE] ',1)[1] for line in result.stdout.splitlines() if '[SSTV-TX-STATE] ' in line]
  if result.returncode or len(matches)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-12000:]);raise RuntimeError('SSTV state probe failed')
  print(a.variant+': '+matches[0])
finally:qml.write_text(original)
