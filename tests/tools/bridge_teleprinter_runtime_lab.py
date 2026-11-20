#!/usr/bin/env python3
"""Exercise the real teleprinter bridge slots using an isolated test profile."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args()
qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1: raise RuntimeError('unexpected QML root')
rig='Teleprinter'+secrets.token_hex(5)
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
  var path=bridge.logAllTxtPath;
  if(path.indexOf("/.qttest/")<0 || path.indexOf("__RIG__")<0){console.log("[TELE-PROBE] unsafe path");return;}
  var dt=new Date("2024-05-06T10:34:56Z");
  bridge.aggiungiRigaRtty("CQ TEST RTTY",0.73,1234.6);
  bridge.aggiungiRigaJtty("CQ TEST JTTY",1450.3,dt);
  bridge.appendJttyAllTxt(false,1450,"CQ FILE JTTY",dt);
  bridge.appendJttyAllTxt(true,1500,"TX FILE JTTY",dt);
  var rows=bridge.decodeList, r=false,j=false;
  for(var i=0;i<rows.length;i++){var x=rows[i];if(x.message==="CQ TEST RTTY")r=x.mode==="RTTY" && x.freq===1235 && String(x.db)==="73";if(x.message==="CQ TEST JTTY")j=x.mode==="JTTY" && x.freq===1450 && String(x.time)==="103456";}
  var f=bridge.readTextFile(path,65536), txt=String(f.text);
  console.log("[TELE-PROBE] "+JSON.stringify({rtty:r,jtty:j,file:f.ok && txt.indexOf("CQ FILE JTTY")>=0 && txt.indexOf("TX FILE JTTY")>=0 && txt.indexOf("240506_103456")>=0,tx:bridge.transmitting}));
 }}
'''.replace('__RIG__',rig)
try:
 with tempfile.TemporaryDirectory(prefix='decodium-teleprinter-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','18000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
  lines=[x.split('[TELE-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[TELE-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-12000:]);raise RuntimeError('teleprinter app probe failed')
  v=json.loads(lines[0]); expected={'rtty':True,'jtty':True,'file':True,'tx':False}
  if v!=expected: raise RuntimeError('unexpected state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir(): shutil.rmtree(path)
