#!/usr/bin/env python3
"""Compare isolated target-call settings without starting a call or TX."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--variant',choices=('a','b'),required=True);p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered: {
   bridge.targetCallSign=__CALL__;bridge.targetCallMaxRetries=__RETRIES__;bridge.targetCallTimeoutS=__TIMEOUT__;bridge.targetCallPeriod=__PERIOD__;bridge.targetCallPauseS=__PAUSE__;bridge.armedWatchEnabled=__ARM__;bridge.armedReArm=__REARM__;
   console.log("[TARGET-SETTINGS] "+JSON.stringify({call:bridge.targetCallSign,retries:bridge.targetCallMaxRetries,timeout:bridge.targetCallTimeoutS,period:bridge.targetCallPeriod,pause:bridge.targetCallPauseS,armed:bridge.armedWatchEnabled,rearm:bridge.armedReArm,active:bridge.targetCallActive}));
 }}
'''
values=('" k1abc "','1500','1','9','800','true','true') if a.variant=='a' else ('" iu8lmc "','-1','900','-2','-8','false','false')
for key,value in zip(('__CALL__','__RETRIES__','__TIMEOUT__','__PERIOD__','__PAUSE__','__ARM__','__REARM__'),values):probe=probe.replace(key,value)
try:
 with tempfile.TemporaryDirectory(prefix='decodium-targetcall-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','TargetSettings'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','26000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  matches=[line.split('[TARGET-SETTINGS] ',1)[1] for line in result.stdout.splitlines() if '[TARGET-SETTINGS] ' in line]
  if result.returncode or len(matches)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-12000:]);raise RuntimeError('target settings probe failed')
  expected=({'call':'K1ABC','retries':999,'timeout':10,'period':2,'pause':300,'armed':True,'rearm':True,'active':False} if a.variant=='a' else {'call':'IU8LMC','retries':0,'timeout':600,'period':0,'pause':0,'armed':False,'rearm':False,'active':False})
  if json.loads(matches[0])!=expected:raise RuntimeError('unexpected target settings: '+matches[0])
  print(a.variant+': '+matches[0])
finally:qml.write_text(original)
