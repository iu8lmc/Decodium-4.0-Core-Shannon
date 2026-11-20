#!/usr/bin/env python3
"""Decode a WAV, then clear both decode lists in isolated app sessions."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav=ROOT/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'
rig='Clear'+secrets.token_hex(5)
probe='''
 property int stage: 0
 Timer {interval:12000;running:true;repeat:false;onTriggered:{console.log("[FILTER-LOG] "+bridge.diagnosticLogPath());bridge.setSetting("Blacklisted",false);bridge.setSetting("Blacklist1","K1ABC");bridge.filtersBypassed=false;bridge.openWavForDecode(__WAV__);check.start()}}
 Timer {id:check;interval:500;running:false;repeat:true;property int ticks:0;onTriggered:{var rows=bridge.decodeList,found=false;for(var i=0;i<rows.length;i++)if(String(rows[i].message).indexOf("CQ K1ABC FN42")>=0)found=true;
 if(stage===0 && found){stage=1;ticks=0;bridge.clearDecodeList();bridge.setSetting("Blacklisted",true);bridge.qmlDebugLog("FILTER_TEST___RIG___BEGIN");bridge.openWavForDecode(__WAV__);return;}
 if(stage===1 && ++ticks>=25){if(found){console.log("[CLEAR-PROBE] rejected-filter");stop();return;}stage=2;ticks=0;bridge.qmlDebugLog("FILTER_TEST___RIG___END");bridge.clearDecodeList();bridge.filtersBypassed=true;bridge.openWavForDecode(__WAV__);return;}
 if(stage===2 && found){console.log("[CLEAR-PROBE] "+JSON.stringify({baseline:true,filtered:true,bypass:true,tx:bridge.transmitting || bridge.txEnabled}));stop();return;}
 if(stage!==1 && ++ticks>=90){console.log("[CLEAR-PROBE] timeout");stop();}
 }}
'''.replace('__WAV__',json.dumps(str(wav))).replace('__RIG__',rig)
try:
 with tempfile.TemporaryDirectory(prefix='decodium-clear-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','90000']
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)']+cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=130)
  lines=[x.split('[CLEAR-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[CLEAR-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0]=='timeout' or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('clear probe failed')
  paths=[x.split('[FILTER-LOG] ',1)[1].strip() for x in result.stdout.splitlines() if '[FILTER-LOG] ' in x]
  if len(paths)!=1:raise RuntimeError('missing diagnostic log path')
  diagnostics=Path(paths[0]).read_text(errors='replace');begin=diagnostics.find('FILTER_TEST_'+rig+'_BEGIN');end=diagnostics.find('FILTER_TEST_'+rig+'_END',begin)
  section=diagnostics[begin:end] if begin>=0 and end>begin else ''
  if not any('user decode filter:' in line and 'CQ K1ABC FN42' in line for line in section.splitlines()):
   print('FILTER-DIAGNOSTICS:',section[-10000:]);raise RuntimeError('no confirmed blacklist rejection in diagnostic log')
  v=json.loads(lines[0]);expected={'baseline':True,'filtered':True,'bypass':True,'tx':False}
  if v!=expected:raise RuntimeError('unexpected state '+str(v))
  print(json.dumps(v,sort_keys=True)+"; diagnostic confirms blacklist rejection")
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
