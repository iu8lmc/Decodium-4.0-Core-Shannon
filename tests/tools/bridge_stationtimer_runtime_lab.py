#!/usr/bin/env python3
"""Exercise deferred station telemetry with CAT disabled and virtual input."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='Telem'+secrets.token_hex(5)
logpath=Path.home()/'Library/Application Support/IU8LMC/Decodium/decodium_diagnostic.log'
needle='sendStationTelemetry: locatore non valido, salto'
deferred='sendStationTelemetry: rimandata al prossimo slot TX'
old_deferred=logpath.read_text(errors='replace').count(deferred) if logpath.exists() else 0
old_count=logpath.read_text(errors='replace').count(needle) if logpath.exists() else 0
probe='''
 Timer { interval: 200; running: true; repeat: true; property bool done: false; onTriggered: {
   var t=Date.now()%15000; if (!done && bridge.monitoring && t>8000 && t<10000) { done=true; bridge.txPeriod=(Math.floor(Date.now()/15000)%2===0 ? 0 : 1); bridge.grid="INVALID"; bridge.sendStationTelemetry(); console.log("[TELEM-PROBE] "+JSON.stringify({grid:bridge.grid,tx:bridge.transmitting,log:bridge.diagnosticLogPath()})); stop(); }
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-stationtimer-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-audio-input','BlackHole 2ch','--lab-monitor-ms','1500','--lab-callsign','IU8LMC','--lab-mode','MSK144','--lab-quit-ms','65000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=100)
  lines=[x.split('[TELEM-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[TELEM-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout: raise RuntimeError('application failed '+result.stdout[-6000:])
  v=json.loads(lines[0]); log=Path(v['log']).read_text(errors='replace')
  if v['grid']!='INVALID' or v['tx'] or log.count(needle)<=old_count or log.count(deferred)<=old_deferred: raise RuntimeError('telemetry result missing: '+str(v)+' '+log[-5000:])
  print(json.dumps({'deferred':True,'invalid_grid_rejected':True,'tx_at_request':False},sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
