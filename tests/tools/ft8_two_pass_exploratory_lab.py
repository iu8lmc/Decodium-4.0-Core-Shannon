#!/usr/bin/env python3
"""Explore two sequential file decodes; the second temporarily leaves LOW CPU.

This is not the proposed live scheduler: audio monitoring and CAT are off,
outputs are captured separately, and the second pass changes application
settings in an isolated test profile.
"""
import argparse, collections, json, os, re, secrets, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--wav',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--log',type=Path,required=True);p.add_argument('--preserve-first',action='store_true');a=p.parse_args()
qml=root/'build-baseline/qml/decodium/Main.qml';original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer { interval:12000;running:true;repeat:false;onTriggered:twoPass.start() }
 Connections {target:bridge;function onDecodingChanged(){if(bridge.decoding && (twoPass.phase===1 || twoPass.phase===4))twoPass.seen=true}}
 Timer {id:twoPass;interval:150;running:false;repeat:true;property int phase:0;property bool seen:false;property int ticks:0;property double startMs:0;property var fastRows:[];property int fastMs:0;property var beforeRows:[];
  function collect(){var out=[];var rows=bridge.decodeList;for(var i=0;i<rows.length;i++)out.push({message:String(rows[i].message),freq:String(rows[i].freq)});return out}
  onTriggered:{
   if(phase===0){if(Date.now()%15000>300)return;bridge.ftThreads=2;bridge.deepSearchEnabled=false;bridge.lowCpuModeEnabled=true;bridge.clearDecodeList();startMs=Date.now();phase=1;seen=false;bridge.openWavForDecode(__WAV__);if(bridge.decoding)seen=true;return}
   if(phase===1){if(bridge.decoding)seen=true;else if(seen){ticks=0;phase=2};return}
   if(phase===2){if(++ticks<6)return;fastRows=collect();fastMs=Date.now()-startMs;__CLEAR_SECOND__ bridge.lowCpuModeEnabled=false;bridge.ftThreads=2;bridge.ft8ApEnabled=false;bridge.deepSearchEnabled=true;bridge.ndepth=3;ticks=0;phase=3;return}
   if(phase===3){if(++ticks<4)return;startMs=Date.now();phase=4;seen=false;bridge.openWavForDecode(__WAV__);if(bridge.decoding)seen=true;return}
   if(phase===4){if(bridge.decoding)seen=true;else if(seen){ticks=0;phase=5};return}
   if(++ticks<6)return;var second=collect();var result={fast:fastRows,second:second,fastMs:fastMs,secondMs:Date.now()-startMs,tx:bridge.transmitting,threads:bridge.ftThreads,secondLowCpu:bridge.lowCpuModeEnabled};bridge.lowCpuModeEnabled=true;console.log("[TWO-PASS] "+JSON.stringify(result));stop();Qt.quit()
  }
 }
'''.replace('__WAV__',json.dumps(str(a.wav.resolve()))).replace('__CLEAR_SECOND__','' if a.preserve_first else 'bridge.clearDecodeList();')
def counter(rows):return collections.Counter((str(r['freq']).strip(),' '.join(r['message'].upper().split())) for r in rows)
try:
 qml.write_text(original.replace(marker,marker+probe,1))
 with tempfile.TemporaryDirectory(prefix='decodium-two-pass-') as tmp:
  rig='TwoPass'+secrets.token_hex(5);ini=Path(tmp)/'Decodium'/'Decodium3.ini';ini.parent.mkdir(parents=True)
  ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n'%rig)
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','85000']
  p=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=100);a.log.write_text(p.stdout)
  lines=[line.split('[TWO-PASS] ',1)[1] for line in p.stdout.splitlines() if '[TWO-PASS] ' in line]
  if p.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in p.stdout:raise RuntimeError('probe failed; see '+str(a.log))
  data=json.loads(lines[0]);dispatches=re.findall(r'\[FT8DISPATCH\].*',p.stdout)
  if data['tx'] or data['secondLowCpu'] or data['threads']!=2 or len(dispatches)<2 or not data['fast']:raise RuntimeError('unexpected state '+repr(data))
  fast=counter(data['fast']);second=counter(data['second']);data['added']=[{'freq':f,'message':m,'count':n} for (f,m),n in sorted((second-fast).items())];data['onlyFast']=[{'freq':f,'message':m,'count':n} for (f,m),n in sorted((fast-second).items())];data['preserveFirst']=a.preserve_first;data['dispatches']=dispatches
  if a.preserve_first and data['onlyFast']:raise RuntimeError('previous rows lost: '+repr(data['onlyFast']))
  a.output.write_text(json.dumps(data,indent=2)+'\n');print(json.dumps(data,sort_keys=True))
finally:qml.write_text(original)
