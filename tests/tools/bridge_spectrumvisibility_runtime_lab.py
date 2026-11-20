#!/usr/bin/env python3
"""Exercise spectrum visibility and QSO visual rearm in an isolated app."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='SpecVis'+secrets.token_hex(5)
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{bridge.spectrumVisible=false;var hidden=!bridge.spectrumVisible;bridge.spectrumVisible=true;bridge.rearmSpectrumAfterQsoLog();console.log("[SPECVIS-PROBE] "+JSON.stringify({hidden:hidden,shown:bridge.spectrumVisible,monitor:bridge.monitoring,legacy:bridge.legacyBackendActive,tx:bridge.transmitting}));}}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-specvis-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1');env.pop('DECODIUM_FORCE_STANDALONE_UDP',None)
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 16ch','--lab-monitor-ms','2500','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','30000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=70)
  lines=[x.split('[SPECVIS-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[SPECVIS-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:raise RuntimeError('probe failed '+result.stdout[-4000:])
  v=json.loads(lines[0]);expected={'hidden':True,'shown':True,'monitor':True,'legacy':True,'tx':False}
  if v!=expected:raise RuntimeError('unexpected '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
