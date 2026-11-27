#!/usr/bin/env python3
"""Decode a fixture and verify its asynchronous SQLite history entry."""
import argparse,json,os,secrets,sqlite3,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav=ROOT/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav';rig='Persist'+secrets.token_hex(5)
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
  if(bridge.queryDecodeHistory({callsign:"K1ABC"}).length!==0){console.log("[PERSIST-PROBE] nonempty-history");return;}
  bridge.openWavForDecode(__WAV__);waitHistory.start()
 }}
 Timer {id:waitHistory;interval:250;running:false;repeat:true;property int ticks:0;onTriggered:{
  var rows=bridge.queryDecodeHistory({callsign:"K1ABC"});var found=false;for(var i=0;i<rows.length;i++)if(String(rows[i].message).indexOf("CQ K1ABC FN42")>=0)found=true;
  if(found){console.log("[PERSIST-PROBE] "+JSON.stringify({persisted:true,tx:bridge.transmitting}));stop();return;}if(++ticks>=100){console.log("[PERSIST-PROBE] timeout");stop()}
 }}
'''.replace('__WAV__',json.dumps(str(wav)))
try:
 with tempfile.TemporaryDirectory(prefix='decodium-persist-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','50000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=100)
  lines=[x.split('[PERSIST-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[PERSIST-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0] in ('timeout','nonempty-history') or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('persistence probe failed')
  v=json.loads(lines[0]);
  if v!={'persisted':True,'tx':False}:raise RuntimeError('unexpected persistence state '+str(v))
  db=Path.home()/'.qttest/Library/Application Support/IU8LMC'/('Decodium - '+rig+' - test')/'db.sqlite'
  if not db.is_file():raise RuntimeError('test database missing')
  with sqlite3.connect('file:'+str(db)+'?mode=ro',uri=True) as conn:
   count,closed=conn.execute("select count(*),sum(s.ended_utc is not null) from decodes d join sessions s on s.id=d.session_id where d.message like '%CQ K1ABC FN42%'").fetchone()
  if count!=1 or closed!=1:raise RuntimeError('unexpected stored rows/session state')
  v['sessionClosed']=True
  print(json.dumps(v,sort_keys=True))
finally:qml.write_text(original)
