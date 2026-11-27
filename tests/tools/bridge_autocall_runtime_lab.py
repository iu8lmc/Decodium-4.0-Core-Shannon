#!/usr/bin/env python3
"""Compare isolated target-call settings without starting a call or TX."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--variant',choices=('a','b'),required=True);p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered: {
  bridge.mode=__MODE__;bridge.dxCall="";bridge.autoCallMaxQsos=__MAX__;bridge.autoCallPriority=__PRIORITY__;bridge.autoCallEnabled=true;
  let started={mode:bridge.mode,enabled:bridge.autoCallEnabled,tx:bridge.txEnabled,max:bridge.autoCallMaxQsos,priority:bridge.autoCallPriority};bridge.autoCallEnabled=false;
  console.log("[AUTOCALL-STATE] "+JSON.stringify({started:started,stopped:!bridge.autoCallEnabled,tx:bridge.txEnabled}));
 }}
'''
for key,value in zip(('__MODE__','__MAX__','__PRIORITY__'),('"FT8"','1500','8') if a.variant=='a' else ('"WSPR"','-3','-2')):probe=probe.replace(key,value)

try:
 with tempfile.TemporaryDirectory(prefix='decodium-targetcall-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','TargetSettings'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','26000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  matches=[line.split('[AUTOCALL-STATE] ',1)[1] for line in result.stdout.splitlines() if '[AUTOCALL-STATE] ' in line]
  if result.returncode or len(matches)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-12000:]);raise RuntimeError('Auto Call probe failed')
  expected=({'started':{'mode':'FT8','enabled':True,'tx':False,'max':999,'priority':2},'stopped':True,'tx':False} if a.variant=='a' else {'started':{'mode':'WSPR','enabled':False,'tx':False,'max':0,'priority':0},'stopped':True,'tx':False})
  if json.loads(matches[0])!=expected:raise RuntimeError('unexpected Auto Call state: '+matches[0])
  print(a.variant+': '+matches[0])
finally:qml.write_text(original)
