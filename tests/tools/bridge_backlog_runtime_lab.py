#!/usr/bin/env python3
"""Observe decoder reset transitions while changing modes in an isolated app."""
import argparse,json,os,re,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
tag='WORKERPROBE'+secrets.token_hex(8)
probe='''
 property var probeModes: ["FT8","FT2","FT4","Q65-60A","MSK144","WSPR","JT65","FST4-120"]
 property int probeIndex: 0
 property var probeActual: []
 Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.qmlDebugLog("__TAG__ BEGIN");console.log("[WORKER-LOG] "+bridge.diagnosticLogPath());modeProbe.start()}}
 Timer {id:modeProbe;interval:800;running:false;repeat:true;onTriggered:{
  if(probeIndex<probeModes.length){bridge.mode=probeModes[probeIndex++];probeActual.push(bridge.mode);}
  else {bridge.qmlDebugLog("__TAG__ END");console.log("[WORKER-MODES] "+JSON.stringify(probeActual));stop()}
 }}
'''.replace('__TAG__',tag)
try:
 with tempfile.TemporaryDirectory(prefix='decodium-workers-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','Workers'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','26000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  modes=[json.loads(line.split('[WORKER-MODES] ',1)[1]) for line in result.stdout.splitlines() if '[WORKER-MODES] ' in line]
  paths=[line.split('[WORKER-LOG] ',1)[1].strip() for line in result.stdout.splitlines() if '[WORKER-LOG] ' in line]
  if result.returncode or len(modes)!=1 or len(paths)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-12000:]);raise RuntimeError('worker modes probe failed')
  log=Path(paths[0]).read_text(errors='replace');begin=log.rfind(tag+' BEGIN');end=log.find(tag+' END',begin)
  if begin<0 or end<0:raise RuntimeError('worker diagnostic markers missing')
  workers=re.findall(r'decoder worker initialized lazily: requested=.*? worker=([^ ]+)',log[begin:end])
  resets=re.findall(r'mode-change decoder worker reset: ([^\n]+)',log[begin:end])
  if not resets:raise RuntimeError('no decoder reset observed')
  print(json.dumps({'modes':modes[0],'created':sorted(workers),'resets':resets},sort_keys=True))
finally:qml.write_text(original)
