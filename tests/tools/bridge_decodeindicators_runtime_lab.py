#!/usr/bin/env python3
"""Exercise decoder indicator slots on isolated application bridges."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='Ind'+secrets.token_hex(5)
probe='''
 Timer { interval:12000;running:true;repeat:false;onTriggered:{bridge.resetLedStatus();bridge.notifyCoherentAveraging(3);bridge.notifyNeuralSyncHit(1.2);bridge.notifyTurboIterations(9);var before={a:bridge.ledCoherentAveraging,b:bridge.ledNeuralSync,c:bridge.ledTurboFeedback,count:bridge.coherentCount,score:bridge.neuralScore,iters:bridge.turboIterations};bridge.resetLedStatus();console.log("[IND-PROBE] "+JSON.stringify({before:before,after:{a:bridge.ledCoherentAveraging,b:bridge.ledNeuralSync,c:bridge.ledTurboFeedback,count:bridge.coherentCount,score:bridge.neuralScore,iters:bridge.turboIterations},tx:bridge.transmitting}));}}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-indicators-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','30000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
  lines=[x.split('[IND-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[IND-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:raise RuntimeError('probe failed '+result.stdout[-4000:])
  v=json.loads(lines[0]);on={'a':True,'b':True,'c':True,'count':3,'score':1,'iters':9};off={'a':False,'b':False,'c':False,'count':0,'score':0,'iters':0}
  if v!={'before':on,'after':off,'tx':False}:raise RuntimeError('unexpected '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
