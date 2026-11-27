#!/usr/bin/env python3
"""Switch away from an active two-thread FT8 deep file decode in an isolated profile."""
import argparse, json, os, re, secrets, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--ft8',type=Path,required=True);p.add_argument('--ft4',type=Path,required=True);p.add_argument('--log',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--return-ft8',action='store_true');p.add_argument('--test-followup',action='store_true');p.add_argument('--switch-delay-ms',type=int,default=1500);a=p.parse_args()
if not 100<=a.switch_delay_ms<=10000:raise ValueError('invalid switch delay')
qml=root/'build-baseline/qml/decodium/Main.qml';original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1 or not a.ft8.is_file() or not a.ft4.is_file():raise RuntimeError('unexpected input')
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{bridge.lowCpuModeEnabled=__START_LOW__;bridge.ftThreads=2;bridge.ft8ApEnabled=false;bridge.deepSearchEnabled=true;bridge.ndepth=3;waitStart.start()}}
 Timer {id:waitStart;interval:100;running:false;repeat:true;onTriggered:{if(Date.now()%15000>200)return;stop();bridge.openWavForDecode(__FT8__);watchStart.start()}}
 Timer {id:watchStart;interval:50;running:false;repeat:true;property int count:0;onTriggered:{if(bridge.decoding){stop();switchDeep.start()}else if(++count>200){console.log("[DEEP-SWITCH] no-start");stop();Qt.quit()}}}
 Timer {id:switchDeep;interval:__SWITCH_DELAY__;running:false;repeat:false;onTriggered:{
  recovery.wasActive=bridge.decoding;bridge.lowCpuModeEnabled=true;var t=Date.now();bridge.mode="FT4";recovery.switchMs=Date.now()-t;recovery.startMs=Date.now();bridge.openWavForDecode(__FT4__);recovery.start()
 }}
 Timer {id:recovery;interval:250;running:false;repeat:true;property int count:0;property bool wasActive:false;property int switchMs:0;property double startMs:0;
  onTriggered:{var rows=bridge.decodeList;for(var i=0;i<rows.length;i++)if(String(rows[i].message)==="CQ K1ABC FN42"){
   var result={active:wasActive,mode:bridge.mode,lowCpu:bridge.lowCpuModeEnabled,tx:bridge.transmitting,freq:String(rows[i].freq),switchMs:switchMs,recoveryMs:Date.now()-startMs};stop();__AFTER_FT4__ return}
   if(++count>180){console.log("[DEEP-SWITCH] timeout");stop();Qt.quit()}
  }
 }
 Timer {id:returnFt8;interval:250;running:false;repeat:true;property int count:0;property var result:({});property double startMs:0;
  onTriggered:{var rows=bridge.decodeList;for(var i=0;i<rows.length;i++)if(String(rows[i].message)==="CQ K1ABC FN42" && String(rows[i].freq)==="1500"){
   result.returnMode=bridge.mode;result.returnLowCpu=bridge.lowCpuModeEnabled;result.returnMs=Date.now()-startMs;console.log("[DEEP-SWITCH] "+JSON.stringify(result));stop();Qt.quit();return}
   if(++count>180){console.log("[DEEP-SWITCH] return-timeout");stop();Qt.quit()}
  }
 }
'''
action=('returnFt8.result=result;bridge.mode="FT8";bridge.deepSearchEnabled=false;returnFt8.startMs=Date.now();bridge.openWavForDecode(__RETURN_FT8__);returnFt8.start();' if a.return_ft8 else 'console.log("[DEEP-SWITCH] "+JSON.stringify(result));Qt.quit();')
probe=probe.replace('__SWITCH_DELAY__',str(a.switch_delay_ms)).replace('__START_LOW__','true' if a.test_followup else 'false').replace('__AFTER_FT4__',action).replace('__FT8__',json.dumps(str(a.ft8.resolve()))).replace('__FT4__',json.dumps(str(a.ft4.resolve()))).replace('__RETURN_FT8__',json.dumps(str(root/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav')))
try:
 qml.write_text(original.replace(marker,marker+probe,1))
 with tempfile.TemporaryDirectory(prefix='decodium-deep-switch-') as tmp:
  rig='DeepSwitch'+secrets.token_hex(5);ini=Path(tmp)/'Decodium'/'Decodium3.ini';ini.parent.mkdir(parents=True)
  ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n'%rig)
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_HASH_LIFECYCLE_DIAGNOSTICS='1')
  if a.test_followup:env['DECODIUM_LAB_LOWCPU_FT8_FOLLOWUP']='1'
  else:env.pop('DECODIUM_LAB_LOWCPU_FT8_FOLLOWUP',None)
  cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','85000']
  p=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=100);a.log.write_text(p.stdout)
  lines=[line.split('[DEEP-SWITCH] ',1)[1] for line in p.stdout.splitlines() if '[DEEP-SWITCH] ' in line]
  if p.returncode or len(lines)!=1 or lines[0] in ('no-start','timeout','return-timeout') or '[LAB] CAT auto-connect disabled' not in p.stdout:raise RuntimeError('switch failed; see '+str(a.log))
  data=json.loads(lines[0]);data['ft8Begin']=bool(re.search(r'\[HASHLIFE\] mode=FT8 decoder_begin',p.stdout))
  if (not a.test_followup and not data['active']) or not data['ft8Begin'] or data['mode']!='FT4' or not data['lowCpu'] or data['tx'] or data['freq']!='1000':raise RuntimeError('unexpected result '+repr(data))
  if a.test_followup:
   if not re.search(r'\[FT8DISPATCH\].*runFollowup=1',p.stdout) or not re.search(r'\[HASHLIFE\] mode=FT8 decoder_begin serial=\s*2',p.stdout) or not re.search(r'\[HASHLIFE\] mode=FT8 decoder_end serial=\s*2.*invalidated=\s*1',p.stdout):raise RuntimeError('experimental follow-up was not verified cancelled')
  elif not re.search(r'\[HASHLIFE\] mode=FT8 decoder_end.*invalidated=\s*1',p.stdout) or not re.search(r'\[FT8DISPATCH\].*effectiveDepth=4',p.stdout):raise RuntimeError('deep decode not verified cancelled')
  if a.return_ft8 and (data.get('returnMode')!='FT8' or not data.get('returnLowCpu')):raise RuntimeError('FT8 did not recover')
  a.output.write_text(json.dumps(data,indent=2)+'\n');print(json.dumps(data,sort_keys=True))
finally:qml.write_text(original)
