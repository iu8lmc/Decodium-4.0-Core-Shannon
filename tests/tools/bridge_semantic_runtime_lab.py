#!/usr/bin/env python3
"""Verify semantic acceptance and rejection in an isolated legacy session."""
import argparse,json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='AllTxt'+secrets.token_hex(5);wav=ROOT/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'
probe='''
 property string targetLog: ""
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
  console.log("[SEMANTIC-LOG] "+bridge.diagnosticLogPath());targetLog=bridge.logAllTxtPath; if(!bridge.legacyBackendActive || targetLog.indexOf("/.qttest/")<0 || targetLog.indexOf("__RIG__")<0) {console.log("[ALLTXT-PROBE] unsafe-path-or-legacy");return;}
  bridge.filtersBypassed=true;bridge.setHideGhostDecodes(false);var result=bridge.writeTextFile(targetLog,"seed\\n");if(!result.ok){console.log("[ALLTXT-PROBE] write-error");return;} appendTimer.start();
 }}
 Timer {id:appendTimer;interval:4000;repeat:false;onTriggered:{bridge.qmlDebugLog("SEMANTIC___RIG___BEGIN");var result=bridge.writeTextFile(targetLog,"seed\\n261107_120030 14.074 Rx FT8 -20 0.1 1501 CQ K1ABC FN42\\n261107_120045 14.074 Rx FT8 -10 0.1 1604 CQ X9 FN42\\n");if(!result.ok){console.log("[ALLTXT-PROBE] write-error");return;}waitLog.start();}}
 Timer {id:waitLog;interval:300;running:false;repeat:true;property int ticks:0;property bool found:false;onTriggered:{var rows=bridge.decodeList;for(var i=0;i<rows.length;i++)if(String(rows[i].message).indexOf("CQ K1ABC FN42")>=0)found=true;if(found && ++ticks>=12){bridge.qmlDebugLog("SEMANTIC___RIG___END");console.log("[ALLTXT-PROBE] "+JSON.stringify({valid:true,tx:bridge.transmitting}));stop();return;}if(!found && ++ticks>=70){console.log("[ALLTXT-PROBE] timeout");stop();}}}
'''.replace('__RIG__',rig)
try:
 with tempfile.TemporaryDirectory(prefix='decodium-alltxt-lab-') as tmp:
  sinks=[]
  for port in (2237,2239):
   sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sinks.append(sock)
  guard=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);guard.bind(('127.0.0.1',2238));guard.close()
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','50000']
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))']+cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=100)
  lines=[x.split('[ALLTXT-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[ALLTXT-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0] in ('unsafe-path-or-legacy','write-error','timeout') or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('ALL.TXT probe failed')
  v=json.loads(lines[0]);
  if v!={'valid':True,'tx':False}:raise RuntimeError('unexpected state '+str(v))
  paths=[x.split('[SEMANTIC-LOG] ',1)[1].strip() for x in result.stdout.splitlines() if '[SEMANTIC-LOG] ' in x]
  if len(paths)!=1:raise RuntimeError('missing log path')
  log=Path(paths[0]).read_text(errors='replace');a=log.find('SEMANTIC_'+rig+'_BEGIN');b=log.find('SEMANTIC_'+rig+'_END',a);segment=log[a:b] if a>=0 and b>a else ''
  if not any('semantic decode filter' in line and 'X9' in line for line in segment.splitlines()):raise RuntimeError('no confirmed semantic rejection')
  print(json.dumps(v,sort_keys=True)+"; diagnostic confirms invalid CQ rejection")
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
