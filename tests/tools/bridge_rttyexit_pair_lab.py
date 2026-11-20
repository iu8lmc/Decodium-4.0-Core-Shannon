#!/usr/bin/env python3
"""Exercise RTTY-exit RX recovery using two CAT-disabled apps over BlackHole."""
import argparse,json,os,secrets,shutil,subprocess,tempfile,time
from pathlib import Path
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--legacy-rx',action='store_true');p.add_argument('--binary',type=Path,default=R/'build/decodium');p.add_argument('--qml',type=Path,default=R/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
suffix=secrets.token_hex(5);rxname='AutoRxRX'+suffix;txname='AutoRxTX'+suffix
probe='''
 property bool switchToRttyWorked: false
 Timer {interval:9000;running:true;repeat:false;onTriggered:{if(bridge.callsign==="IU8LMC"){bridge.mode="RTTY";switchToRttyWorked=(bridge.mode==="RTTY");}}}
 Timer {interval:11000;running:true;repeat:false;onTriggered:{if(bridge.callsign==="IU8LMC")bridge.mode="FT8"}}
 Timer {interval:7000;running:true;repeat:false;onTriggered:{if(bridge.callsign==="IU8LMC"){bridge.rxInputLevel=90;bridge.autoRxInputLevel=true;}}}
 Timer {interval:250;running:true;repeat:true;onTriggered:{if(bridge.callsign==="9H1SR" && bridge.transmitting){console.log("[AUTORX-SENDER] transmitting");stop();}}}
 Timer {interval:51000;running:true;repeat:false;onTriggered:{if(bridge.callsign==="IU8LMC")console.log("[AUTORX-PROBE] "+JSON.stringify({level:bridge.rxInputLevel,auto:bridge.autoRxInputLevel,tx:bridge.transmitting,monitor:bridge.monitoring,mode:bridge.mode,rtty:switchToRttyWorked,legacy:bridge.legacyBackendActive}));}}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-autorx-pair-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1))
  def env(role):
   d=Path(tmp)/role;d.mkdir();e=os.environ.copy();e.update(QT_QPA_PLATFORM='cocoa',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=str(d),DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_FTX_SLOT_TIMING='1');
   if role=='rx' and a.legacy_rx:e.pop('DECODIUM_FORCE_STANDALONE_UDP',None)
   return e
  common=[str(a.binary.resolve()),'--test-mode','--lab-no-cat','--lab-mode','FT8','--lab-grid','JM75']
  rxargs=common+['--rig-name',rxname,'--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 16ch','--lab-callsign','IU8LMC','--lab-monitor-ms','2500','--lab-quit-ms','59000']
  txargs=common+['--rig-name',txname,'--lab-audio-input','BlackHole 16ch','--lab-audio-output','BlackHole 2ch','--lab-callsign','9H1SR','--lab-tx-output-level','0','--lab-monitor-ms','2500','--lab-tx-period','0','--lab-sendtx-ms','10000','--lab-sendtx-slot','6','--lab-quit-ms','49000']
  with (Path(tmp)/'rx.log').open('w') as rf,(Path(tmp)/'tx.log').open('w') as tf:
   rx=subprocess.Popen(rxargs,env=env('rx'),stdout=rf,stderr=subprocess.STDOUT)
   try:
    time.sleep(7);tx=subprocess.Popen(txargs,env=env('tx'),stdout=tf,stderr=subprocess.STDOUT)
    try:tc=tx.wait(timeout=75);rc=rx.wait(timeout=30)
    finally:
     if tx.poll() is None:tx.terminate();tx.wait(timeout=10)
   finally:
    if rx.poll() is None:rx.terminate();rx.wait(timeout=10)
  r=(Path(tmp)/'rx.log').read_text(errors='replace');t=(Path(tmp)/'tx.log').read_text(errors='replace');lines=[v.split('[AUTORX-PROBE] ',1)[1] for v in r.splitlines() if '[AUTORX-PROBE] ' in v]
  if rc or tc or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in r or '[LAB] CAT auto-connect disabled' not in t or '[AUTORX-SENDER] transmitting' not in t:print('rx:',r[-6000:],'tx:',t[-4000:]);raise RuntimeError('virtual pair failed')
  v=json.loads(lines[0]);
  if v['legacy']!=a.legacy_rx or not v['auto'] or v['tx'] or not v['monitor'] or v['mode']!='FT8' or not v['rtty'] or not 0<=v['level']<89.5:raise RuntimeError('automatic gain did not reduce: '+str(v))
  print(json.dumps({'gain_reduced':True,'auto':v['auto'],'tx':v['tx'],'monitor':v['monitor'],'mode':v['mode'],'rtty':v['rtty'],'legacy':v['legacy']},sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  for rig in (rxname,txname):
   path=parent/('Decodium - '+rig+' - test')
   if path.is_dir():shutil.rmtree(path)
