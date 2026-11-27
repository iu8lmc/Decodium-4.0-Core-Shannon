#!/usr/bin/env python3
"""Measure isolated baseline/current startup with CAT and monitoring disabled."""
import os,subprocess,time,tempfile,secrets,shutil,json
from pathlib import Path
root=Path(__file__).resolve().parents[2]; parent=Path.home()/'.qttest/Library';baseline='/tmp/decodium-color-baseline/Decodium4.app/Contents/MacOS/Decodium4';current=str(root/'build/decodium')
keys=['main() START','bridge constructing','bridge OK','window visible at','Configuration Settings','WorkedBefore::reload','starting Main.qml load at','Main.qml created as top-level','waterfall visual stage ready','live map visual stage ready']
for label,binary,standalone in [('baseline-legacy',baseline,False),('current-legacy',current,False)]:
 rig='StartAB'+secrets.token_hex(5)
 with tempfile.TemporaryDirectory(prefix='decodium-startup-ab-') as tmp:
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1');env.pop('DECODIUM_FORCE_STANDALONE_UDP',None)
  if standalone:env['DECODIUM_FORCE_STANDALONE_UDP']='1'
  cmd=[binary,'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','20000']
  start=time.monotonic(); result={}; metrics=[];p=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1,env=env)
  try:
   for line in p.stdout:
    if '[STARTUP-TIMING]' in line:metrics.append(line.strip())
    for key in keys:
     if key in line and key not in result:result[key]=round(time.monotonic()-start,3)
   rc=p.wait(timeout=40)
  finally:
   if p.poll() is None:p.terminate();p.wait(timeout=10)
   for base in ('Application Support','Caches'):
    path=parent/base/'IU8LMC'/('Decodium - '+rig+' - test')
    if path.is_dir():shutil.rmtree(path)
  print(json.dumps({'case':label,'exit':rc,'events':result,'metrics':metrics},sort_keys=True),flush=True)
