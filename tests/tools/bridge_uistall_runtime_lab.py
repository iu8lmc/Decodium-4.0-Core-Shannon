#!/usr/bin/env python3
"""Observe an artificial UI event-loop stall without CAT, monitoring or TX."""
import argparse,os,re,secrets,shutil,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=R/'build/decodium');p.add_argument('--qml',type=Path,default=R/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='UiStall'+secrets.token_hex(5)
log=Path.home()/'Library/Application Support/IU8LMC/Decodium/decodium_diagnostic.log'
offset=log.stat().st_size if log.exists() else 0
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{console.log("[STALL-PATH] "+bridge.diagnosticLogPath());bridge.setMainWindowRenderActive(true);var end=Date.now()+1200;while(Date.now()<end){}console.log("[STALL-PROBE] finished tx="+bridge.transmitting)}}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-uistall-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-mode','FT8','--lab-quit-ms','19000']
  res=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
  diag=''
  if log.exists() and log.stat().st_size>=offset:
   with log.open('rb') as f:f.seek(offset);diag=f.read().decode('utf-8',errors='replace')
  gaps=[int(x) for x in re.findall(r'\[UIDBG\] main-thread stall gap_ms=(\d+)',diag)]
  if res.returncode or '[LAB] CAT auto-connect disabled' not in res.stdout or '[STALL-PROBE] finished tx=false' not in res.stdout or not any(x>=1100 for x in gaps):
   print('new diagnostic contains stall=', '[UIDBG] main-thread stall gap_ms=' in diag);raise RuntimeError('UI stall was not observed')
  print('synthetic UI stall detected; CAT and monitoring disabled; TX false: OK')
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
