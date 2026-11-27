#!/usr/bin/env python3
"""Compare enrichment fields in an isolated application session."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--variant',choices=('a','b'),default='a');p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav=ROOT/('tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav' if a.variant=='a' else 'tests/bridge_refactor_fixtures/ft8-9h1sr-1300.wav')
wanted='CQ K1ABC FN42' if a.variant=='a' else 'CQ 9H1SR JM75'
rig='Clear'+secrets.token_hex(5)
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{bridge.filtersBypassed=true;bridge.openWavForDecode(__WAV__);waitDecode.start()}}
 Timer {id:waitDecode;interval:250;running:false;repeat:true;property int ticks:0;onTriggered:{
  var rows=bridge.decodeList;for(var i=0;i<rows.length;i++){if(String(rows[i].message).indexOf(__WANTED__)>=0){
   console.log("[CLEAR-PROBE] "+JSON.stringify({call:String(rows[i].fromCall),grid:String(rows[i].dxGrid),tx:bridge.transmitting,dxcall:String(rows[i].dxCallsign),country:String(rows[i].dxCountry),continent:String(rows[i].dxContinent),prefix:String(rows[i].dxPrefix),zone:rows[i].dxCqZone,itu:rows[i].dxItuZone,cq:rows[i].isCQ,display:String(rows[i].displayMessage),source:String(rows[i].source),provenance:String(rows[i].provenance),b4:rows[i].isB4,newgrid:rows[i].dxIsNewGrid,highlight:rows[i].isHighlighted,snrcolor:String(rows[i].snrColor),bgcolor:String(rows[i].bgColorHex)}));stop();return;
  }}if(++ticks>=90){console.log("[CLEAR-PROBE] timeout");stop()}
 }}
'''.replace('__WAV__',json.dumps(str(wav))).replace('__WANTED__',json.dumps(wanted))
try:
 with tempfile.TemporaryDirectory(prefix='decodium-clear-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','45000']
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)']+cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  lines=[x.split('[CLEAR-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[CLEAR-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0]=='timeout' or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('clear probe failed')
  v=json.loads(lines[0]);expected={'call':'K1ABC' if a.variant=='a' else '9H1SR','grid':'FN42' if a.variant=='a' else 'JM75','tx':False}
  if any(v[k]!=expected[k] for k in expected):raise RuntimeError('unexpected state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
