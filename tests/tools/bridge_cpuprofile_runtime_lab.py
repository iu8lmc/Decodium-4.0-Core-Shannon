#!/usr/bin/env python3
"""Exercise CPU profile controls in an isolated Decodium session."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered: {
 bridge.ftThreads=1000;var max=bridge.ftThreads;bridge.ftThreads=-1;var min=bridge.ftThreads;
 bridge.ftThreadsAuto=true;var automatic=bridge.ftThreadsAuto;var autoCount=bridge.ftThreads;
 bridge.ftThreadsAuto=false;bridge.lowCpuModeEnabled=true;var lowOn=bridge.lowCpuModeEnabled;
 bridge.lowCpuModeEnabled=false;console.log("[CPU-PROBE] "+JSON.stringify({max:max,min:min,automatic:automatic,autoCount:autoCount,manual:!bridge.ftThreadsAuto,lowOn:lowOn,lowOff:!bridge.lowCpuModeEnabled}));
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-cpu-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','CpuProbe'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','23000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  values=[json.loads(line.split('[CPU-PROBE] ',1)[1]) for line in result.stdout.splitlines() if '[CPU-PROBE] ' in line]
  if result.returncode or len(values)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-12000:]);raise RuntimeError('CPU profile probe failed')
  v=values[0]
  if not (v['max']==24 and v['min']==1 and v['automatic'] and 1<=v['autoCount']<=24 and v['manual'] and v['lowOn'] and v['lowOff']):raise RuntimeError('unexpected CPU profile: '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:qml.write_text(original)
