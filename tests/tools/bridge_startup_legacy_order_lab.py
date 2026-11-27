#!/usr/bin/env python3
"""Exercise startup ordering with a deliberate QML bootstrap stall, CAT disabled."""
import os,subprocess,time,tempfile,secrets,shutil,json,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2];parent=Path.home()/'.qttest/Library'
version=sys.argv[1];binary=Path('/tmp/decodium-color-baseline/Decodium4.app/Contents/MacOS/Decodium4') if version=='baseline' else root/'build/decodium';qml=Path('/tmp/decodium-color-baseline/Decodium4.app/Contents/Resources/qml/decodium/BootLoader.qml') if version=='baseline' else root/'build/qml/decodium/BootLoader.qml';original=qml.read_text();marker='    Component.onCompleted: {';assert original.count(marker)==1
rig='Pressure'+secrets.token_hex(5)
try:
 qml.write_text(original.replace(marker,marker+'\n        var stallUntil=Date.now()+4000; while(Date.now()<stallUntil) {}\n',1))
 with tempfile.TemporaryDirectory(prefix='decodium-startup-pressure-') as tmp:
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1');env.pop('DECODIUM_FORCE_STANDALONE_UDP',None)
  cmd=[str(binary),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','22000'];keys=['bridge constructing','bridge OK','window visible at','Configuration Settings','WorkedBefore::reload','starting Main.qml load at','Main.qml created as top-level','waterfall visual stage ready','live map visual stage ready'];start=time.monotonic();ev={};lines=[];p=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1,env=env)
  for line in p.stdout:
   lines.append(line)
   for key in keys:
    if key in line and key not in ev:ev[key]=round(time.monotonic()-start,3)
  rc=p.wait(timeout=40)
  if rc or 'Main.qml created as top-level' not in ev or 'Configuration Settings' not in ev:raise RuntimeError('incomplete '+''.join(lines[-60:]))
  main_first=ev['Main.qml created as top-level']<ev['Configuration Settings']
  if main_first != (version=='current'):raise RuntimeError('legacy startup order incorrect '+str(ev))
  print(json.dumps({'case':version,'main_before_legacy':main_first,'events':ev,'exit':rc},sort_keys=True),flush=True)
finally:
 qml.write_text(original)
 for base in ('Application Support','Caches'):
  path=parent/base/'IU8LMC'/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
