#!/usr/bin/env python3
"""Exercise audio level controls in an isolated, non-transmitting application."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--legacy',action='store_true');p.add_argument('--binary',type=Path,default=R/'build/decodium');p.add_argument('--qml',type=Path,default=R/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='AudioLevels'+secrets.token_hex(5)
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
  bridge.rxInputLevel=30;bridge.autoRxInputLevel=true;var auto=bridge.autoRxInputLevel;
  bridge.rxInputLevel=110;var high=bridge.rxInputLevel===100;var manual=!bridge.autoRxInputLevel;
  bridge.rxInputLevel=-10;var low=bridge.rxInputLevel===0;
  bridge.txOutputLevel=500;var txhigh=bridge.txOutputLevel===450;
  bridge.txOutputLevel=-1;var txlow=bridge.txOutputLevel===0;
  console.log("[AUDIOLEVELS-PROBE] "+JSON.stringify({auto:auto,manual:manual,high:high,low:low,txhigh:txhigh,txlow:txlow,transmitting:bridge.transmitting,legacy:bridge.legacyBackendActive}));
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-audiolevels-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  if a.legacy:env.pop('DECODIUM_FORCE_STANDALONE_UDP',None)
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','18000']
  res=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
  lines=[x.split('[AUDIOLEVELS-PROBE] ',1)[1] for x in res.stdout.splitlines() if '[AUDIOLEVELS-PROBE] ' in x]
  if res.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in res.stdout:print(res.stdout[-12000:]);raise RuntimeError('audio controls probe failed')
  v=json.loads(lines[0]);expected={'auto':True,'manual':True,'high':True,'low':True,'txhigh':True,'txlow':True,'transmitting':False,'legacy':a.legacy}
  if v!=expected:raise RuntimeError('unexpected state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
