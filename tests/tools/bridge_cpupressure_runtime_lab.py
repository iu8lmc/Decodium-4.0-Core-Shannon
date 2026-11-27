#!/usr/bin/env python3
"""Exercise CPU-profile and render-visibility controls without radio TX."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{
  bridge.ftThreads=24;bridge.lowCpuModeEnabled=false;var normal=bridge.effectiveFtThreadLimitForDecode();
  bridge.lowCpuModeEnabled=true;var low=bridge.effectiveFtThreadLimitForDecode();bridge.setMainWindowRenderActive(false);bridge.setMainWindowRenderActive(true);bridge.lowCpuModeEnabled=false;
  console.log("[PRESSURE-PROBE] "+JSON.stringify({normal:normal,low:low,tx:bridge.transmitting}));
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-pressure-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','Pressure'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','21000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  lines=[x.split('[PRESSURE-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[PRESSURE-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('pressure probe failed')
  v=json.loads(lines[0]);
  if not (1<=v['low']<=2 and v['normal']>=v['low'] and not v['tx']):raise RuntimeError('unexpected pressure state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:qml.write_text(original)
