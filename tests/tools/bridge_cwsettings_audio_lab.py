#!/usr/bin/env python3
"""Check enabled/disabled CW sidetone over virtual BlackHole audio."""
import argparse,json,os,secrets,shutil,subprocess,tempfile,time
from pathlib import Path
import numpy as np
import sounddevice as sd
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='CWAud'+secrets.token_hex(5)
probe='''
 Timer { interval:12000;running:true;repeat:false;onTriggered:{var out=bridge.cwMonitorOutputs();var id="";for(var i=0;i<out.length;i++){if(String(out[i].label).indexOf("BlackHole 2ch")>=0)id=out[i].id;} bridge.cwMonitorDevice=id;bridge.cwMonitorVolume=80;bridge.cwMonitorEnabled=true;console.log("[CWAUD-ON] "+JSON.stringify({found:id!=="",ms:Date.now(),tx:bridge.transmitting}));bridge.testCwMonitor();}}
 Timer { interval:20000;running:true;repeat:false;onTriggered:{bridge.cwMonitorEnabled=false;console.log("[CWAUD-OFF] "+JSON.stringify({ms:Date.now(),tx:bridge.transmitting}));bridge.testCwMonitor();}}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-cwaudio-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','30000']
  device=next(i for i,d in enumerate(sd.query_devices()) if d['name']=='BlackHole 2ch' and d['max_input_channels']>=2)
  output=Path(tmp)/'application.log'; start=time.time(); recording=sd.rec(34*48000,samplerate=48000,channels=2,device=device,dtype='float32',blocking=False)
  with output.open('w') as stream:
   process=subprocess.Popen(cmd,env=env,stdout=stream,stderr=subprocess.STDOUT,text=True)
   try:code=process.wait(timeout=60);sd.wait()
   finally:
    sd.stop()
    if process.poll() is None:process.terminate();process.wait(timeout=10)
  log=output.read_text(errors='replace')
  def event(tag):
   lines=[line.split(tag,1)[1] for line in log.splitlines() if tag in line]
   if len(lines)!=1:raise RuntimeError('missing '+tag+' '+log[-3500:])
   return json.loads(lines[0])
  on=event('[CWAUD-ON] ');off=event('[CWAUD-OFF] ')
  if code or '[LAB] CAT auto-connect disabled' not in log or not on['found'] or on['tx'] or off['tx']:raise RuntimeError('bad app state')
  def peak(v):
   idx=int((v['ms']/1000-start)*48000);window=recording[max(0,idx):min(len(recording),idx+int(4*48000))];return float(np.max(np.abs(window))) if len(window) else 0
  pon=peak(on);poff=peak(off)
  if pon<0.01 or poff>0.002:raise RuntimeError(f'unexpected signal enabled={pon} disabled={poff}')
  print(json.dumps({'enabled_audible':True,'disabled_silent':True,'tx':False},sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
