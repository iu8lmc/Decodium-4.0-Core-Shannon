#!/usr/bin/env python3
"""Compare isolated target-call settings without starting a call or TX."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--variant',choices=('a','b'),required=True);p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered: {
  bridge.targetCallSign="K1ABC";bridge.targetCallPeriod=__PERIOD__;bridge.armedWatchEnabled=true;bridge.armedReArm=true;bridge.startTargetCall();
  let started={active:bridge.targetCallActive,waiting:bridge.targetCallArmedWaiting,txEnabled:bridge.txEnabled,dx:bridge.dxCall};bridge.stopTargetCall();
  console.log("[TARGET-LIFECYCLE] "+JSON.stringify({started:started,stopped:!bridge.targetCallActive,txEnabled:bridge.txEnabled}));
 }}
'''.replace('__PERIOD__','0' if a.variant=='a' else '2')

try:
 with tempfile.TemporaryDirectory(prefix='decodium-targetcall-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','TargetSettings'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','26000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  matches=[line.split('[TARGET-LIFECYCLE] ',1)[1] for line in result.stdout.splitlines() if '[TARGET-LIFECYCLE] ' in line]
  if result.returncode or len(matches)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-12000:]);raise RuntimeError('target lifecycle probe failed')
  expected={'started':{'active':True,'waiting':True,'txEnabled':False,'dx':'K1ABC'},'stopped':True,'txEnabled':False}
  if json.loads(matches[0])!=expected:raise RuntimeError('unexpected target lifecycle: '+matches[0])
  print(a.variant+': '+matches[0])
finally:qml.write_text(original)
