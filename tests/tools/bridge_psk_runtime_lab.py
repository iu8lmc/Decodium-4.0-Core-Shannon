#!/usr/bin/env python3
"""Compare PSK Reporter time-window controls without enabling reporting."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{
  if(bridge.pskReporterEnabled){console.log("[PSK-PROBE] reporting-enabled");return;}
  bridge.pskReporterTimeSpanMinutes=17;var a=bridge.pskReporterTimeSpanMinutes;bridge.pskReporterTimeSpanMinutes=1;var b=bridge.pskReporterTimeSpanMinutes;
  bridge.pskMapSpotWindowMinutes=58;var c=bridge.pskMapSpotWindowMinutes;bridge.pskMapSpotWindowMinutes=6;var d=bridge.pskMapSpotWindowMinutes;
  console.log("[PSK-PROBE] "+JSON.stringify({rounded:a,min:b,max:c,mapMin:d,enabled:bridge.pskReporterEnabled,tx:bridge.transmitting}));
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-psk-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','PskProbe'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','21000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  lines=[x.split('[PSK-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[PSK-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0]=='reporting-enabled' or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('PSK probe failed')
  v=json.loads(lines[0]);
  if v!={'rounded':15,'min':5,'max':60,'mapMin':5,'enabled':False,'tx':False}:raise RuntimeError('unexpected PSK state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:qml.write_text(original)
