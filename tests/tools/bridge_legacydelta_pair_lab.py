#!/usr/bin/env python3
"""Compare legacy RX model with two isolated instances over BlackHole audio."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:20000;running:true;repeat:false;onTriggered:{if(bridge.callsign==="IU8LMC")modelProbe.start()}}
 Timer {id:modelProbe;interval:1000;running:false;repeat:true;property int ticks:0;onTriggered:{
  var model=bridge.bandActivityModel;for(var i=0;i<model.count();i++){var e=model.entry(i);if(String(e.message).indexOf("9H1SR")>=0){console.log("[LEGACY-DELTA] "+JSON.stringify({found:true,legacy:bridge.legacyBackendActive,tx:bridge.transmitting}));stop();return;}}
  if(++ticks>=63){console.log("[LEGACY-DELTA] timeout legacy="+bridge.legacyBackendActive+" rows="+model.count());stop()}
 }}
'''
suffix=secrets.token_hex(5)
try:
 with tempfile.TemporaryDirectory(prefix='decodium-legacy-pair-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1))
  def env(role):
   e=os.environ.copy();p=Path(tmp)/role;p.mkdir();e.update(QT_QPA_PLATFORM='cocoa',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=str(p),DECODIUM_DISABLE_QML_CACHE='1')
   if role=='tx':e['DECODIUM_FORCE_STANDALONE_UDP']='1';e['DECODIUM_LAB_FTX_SLOT_TIMING']='1'
   else:e.pop('DECODIUM_FORCE_STANDALONE_UDP',None)
   return e
  common=[str(a.binary.resolve()),'--test-mode','--lab-no-cat','--lab-mode','FT8','--lab-grid','JM75']
  rxargs=common+['--rig-name','LegacyRX'+suffix,'--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 16ch','--lab-callsign','IU8LMC','--lab-monitor-ms','2500','--lab-quit-ms','87000']
  txargs=common+['--rig-name','LegacyTX'+suffix,'--lab-audio-input','BlackHole 16ch','--lab-audio-output','BlackHole 2ch','--lab-callsign','9H1SR','--lab-tx-output-level','70','--lab-monitor-ms','2500','--lab-tx-period','0','--lab-sendtx-ms','10000','--lab-sendtx-slot','6','--lab-quit-ms','68000']
  with (Path(tmp)/'rx.log').open('w') as rxlog,(Path(tmp)/'tx.log').open('w') as txlog:
   rx=subprocess.Popen(rxargs,env=env('rx'),stdout=rxlog,stderr=subprocess.STDOUT)
   try:
    import time;time.sleep(7);tx=subprocess.Popen(txargs,env=env('tx'),stdout=txlog,stderr=subprocess.STDOUT)
    try:txcode=tx.wait(timeout=100);rxcode=rx.wait(timeout=35)
    finally:
     if tx.poll() is None:tx.terminate();tx.wait(timeout=10)
   finally:
    if rx.poll() is None:rx.terminate();rx.wait(timeout=10)
  r=(Path(tmp)/'rx.log').read_text(errors='replace');t=(Path(tmp)/'tx.log').read_text(errors='replace')
  lines=[x.split('[LEGACY-DELTA] ',1)[1] for x in r.splitlines() if '[LEGACY-DELTA] ' in x]
  if rxcode or txcode or len(lines)!=1 or 'timeout' in lines[0] or '[LAB] CAT auto-connect disabled' not in r or '[LAB] CAT auto-connect disabled' not in t:
   print('probe=',lines,'rx_tail=',r[-6000:],'tx_tail=',t[-3000:]);raise RuntimeError('legacy virtual audio probe failed')
  v=json.loads(lines[0]);
  if v!={'found':True,'legacy':True,'tx':False}:raise RuntimeError('unexpected legacy state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  for role in ('LegacyRX','LegacyTX'):
   path=parent/('Decodium - '+role+suffix+' - test')
   if path.is_dir():shutil.rmtree(path)

