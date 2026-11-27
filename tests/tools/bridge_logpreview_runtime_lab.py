#!/usr/bin/env python3
"""Exercise synthetic QSO previews without submitting or transmitting."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=R/'build/decodium');p.add_argument('--qml',type=Path,default=R/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='LogPreview'+secrets.token_hex(5)
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
  bridge.dxCall="K1ABC";bridge.dxGrid="FN42";bridge.setSetting("dBReportsToComments",false);bridge.setSetting("SpecOpInComments",false);bridge.setSetting("LogComments","default comment");var before=bridge.pendingLogQsoPreview();
  bridge.setNextLogComment("  synthetic comment  ");var after=bridge.pendingLogQsoPreview();
  console.log("[PREVIEW-PROBE] "+JSON.stringify({call:before.call==="K1ABC",grid:before.grid==="FN42",mode:before.mode==="FT8",timeValid:/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/.test(String(before.timeOn)),defaultComment:before.comment==="default comment",comment:after.comment==="synthetic comment",stored:bridge.getSetting("LogComments","")==="synthetic comment",tx:bridge.transmitting}));
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-logpreview-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','18000']
  res=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
  lines=[x.split('[PREVIEW-PROBE] ',1)[1] for x in res.stdout.splitlines() if '[PREVIEW-PROBE] ' in x]
  if res.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in res.stdout:print(res.stdout[-12000:]);raise RuntimeError('preview probe failed')
  v=json.loads(lines[0]);expected={'call':True,'grid':True,'mode':True,'timeValid':True,'defaultComment':True,'comment':True,'stored':True,'tx':False}
  if v!=expected:raise RuntimeError('unexpected state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
