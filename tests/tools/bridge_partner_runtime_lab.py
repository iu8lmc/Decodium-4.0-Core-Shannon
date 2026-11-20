#!/usr/bin/env python3
"""Compare partner memory settings in isolated non-transmitting sessions."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered: {
 bridge.resumeQsoOnReply=true;bridge.ft2PartnerMemoryEnabled=true;bridge.ft2Tx2ResendOnStall=true;
 var on=bridge.resumeQsoOnReply && bridge.ft2PartnerMemoryEnabled && bridge.ft2Tx2ResendOnStall;
 bridge.resumeQsoOnReply=false;bridge.ft2PartnerMemoryEnabled=false;bridge.ft2Tx2ResendOnStall=false;
 console.log("[PARTNER-PROBE] "+JSON.stringify({on:on,off:!bridge.resumeQsoOnReply&&!bridge.ft2PartnerMemoryEnabled&&!bridge.ft2Tx2ResendOnStall,tx:bridge.transmitting}));
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-partner-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','PartnerProbe'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','23000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  values=[json.loads(line.split('[PARTNER-PROBE] ',1)[1]) for line in result.stdout.splitlines() if '[PARTNER-PROBE] ' in line]
  if result.returncode or values!=[{'on':True,'off':True,'tx':False}] or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-12000:]);raise RuntimeError('partner settings probe failed')
  print(json.dumps(values[0],sort_keys=True))
finally:qml.write_text(original)
