#!/usr/bin/env python3
"""Compare an FT8-to-FT4 mode change while WAV decoding is active."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');p.add_argument('--low-cpu',action='store_true');p.add_argument('--mode',choices=('FT8','FT4','FT2'),default='FT8');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav=ROOT/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'
recovery_wav=ROOT/'tests/bridge_refactor_fixtures/ft8-9h1sr-1300.wav'
next_mode={'FT8':'FT4','FT4':'FT2','FT2':'FT8'}[a.mode]
if a.mode!='FT8':
 wav=ROOT/f'build-baseline/performance/wavs-ft2-ft4/{a.mode.lower()}-0.wav';recovery_wav=wav
probe='''
 property bool sawActive: false
 property bool switchedClean: false
 property bool switchedMode: false
 property bool idleBeforeReturn: false
 Timer {interval:12000;running:true;repeat:false;onTriggered:{bridge.openWavForDecode(__WAV__);watchDecode.start()}}
 Timer {id:watchDecode;interval:5;running:false;repeat:true;property int ticks:0;onTriggered:{
   if(bridge.decoding){sawActive=true;bridge.mode="FT4";switchedMode=(bridge.mode==="FT4");switchedClean=(switchedMode && !bridge.decoding);recoverDecode.start();stop();}
   else if(++ticks>=2000){console.log("[ACTIVE-SWITCH] no-active-decode");stop()}
 }}
 Timer {id:recoverDecode;interval:1800;running:false;repeat:false;onTriggered:{idleBeforeReturn=!bridge.decoding;bridge.mode="FT8";bridge.clearDecodeList();bridge.openWavForDecode(__RECOVERY__);watchRecovery.start()}}
 Timer {id:watchRecovery;interval:250;running:false;repeat:true;property int ticks:0;onTriggered:{
  var rows=bridge.decodeList;for(var i=0;i<rows.length;i++){if(String(rows[i].message).indexOf("CQ 9H1SR JM75")>=0){console.log("[ACTIVE-SWITCH] "+JSON.stringify({active:sawActive,switchedMode:switchedMode,switchedClean:switchedClean,idleBeforeReturn:idleBeforeReturn,recovered:true,mode:bridge.mode}));stop();return;}}
  if(++ticks>=90){console.log("[ACTIVE-SWITCH] recovery-timeout");stop()}
 }}
'''.replace('__WAV__',json.dumps(str(wav))).replace('__RECOVERY__',json.dumps(str(recovery_wav)))
if a.mode!='FT8':
 probe=probe.replace('bridge.mode="FT8"','bridge.mode=__RETURN_MODE__').replace('bridge.mode="FT4"','bridge.mode=__NEXT_MODE__').replace('bridge.mode==="FT4"','bridge.mode===__NEXT_MODE__').replace('__RETURN_MODE__',json.dumps(a.mode)).replace('__NEXT_MODE__',json.dumps(next_mode)).replace('CQ 9H1SR JM75','CQ K1ABC FN42')
try:
 with tempfile.TemporaryDirectory(prefix='decodium-active-switch-') as tmp:
  rig='ActiveSwitch'+secrets.token_hex(5)
  if a.low_cpu:
   ini=Path(tmp)/'Decodium/Decodium3.ini';ini.parent.mkdir(parents=True);ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n'%rig)
   probe=probe.replace('active:sawActive,switchedMode:', 'low:bridge.lowCpuModeEnabled,active:sawActive,switchedMode:')
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode',a.mode,'--lab-quit-ms','48000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  lines=[x.split('[ACTIVE-SWITCH] ',1)[1] for x in result.stdout.splitlines() if '[ACTIVE-SWITCH] ' in x]
  if result.returncode or len(lines)!=1 or lines[0] in ('no-active-decode','recovery-timeout') or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('active decode switch not observed')
  v=json.loads(lines[0]);
  if not v['active'] or not v['switchedMode'] or not v['idleBeforeReturn'] or not v['recovered'] or v['mode']!=a.mode or (a.low_cpu and not v.get('low')):raise RuntimeError('unexpected state: '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:qml.write_text(original)
