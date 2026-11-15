#!/usr/bin/env python3
"""Compare mode-change cleanup and recovery after real file-based decodes."""
import argparse,json,os,secrets,subprocess,sys,tempfile,time
from pathlib import Path
try: import resource
except ImportError: resource=None
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');p.add_argument('--low-cpu',action='store_true');p.add_argument('--check-timing-bar',action='store_true');p.add_argument('--log',type=Path,help='save the application output for startup and mode profiling');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav1=ROOT/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav';wav2=ROOT/'tests/bridge_refactor_fixtures/ft8-9h1sr-1300.wav'
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{waitDecode.decodeStart=Date.now();bridge.openWavForDecode(__WAV1__);waitDecode.start()}}
 Timer {id:cpuProbe;property bool startedLowCpu:false;interval:1000;running:true;repeat:false;onTriggered:{startedLowCpu=bridge.lowCpuModeEnabled}}
 Timer {id:waitDecode;interval:250;running:false;repeat:true;property int ticks:0;property int phase:0;property bool cleared:false;property double decodeStart:0;property int firstDecodeMs:0;property int switchMs:0;onTriggered:{
  var rows=bridge.decodeList;var wanted=phase===0?"CQ K1ABC FN42":"CQ 9H1SR JM75";
  for(var i=0;i<rows.length;i++){if(String(rows[i].message).indexOf(wanted)>=0){
   if(phase===0){firstDecodeMs=Date.now()-decodeStart;var switchStart=Date.now();bridge.mode="FT4";cleared=(bridge.decodeList.length===0 && bridge.rxDecodeList.length===0);bridge.mode="FT8";switchMs=Date.now()-switchStart;decodeStart=Date.now();bridge.openWavForDecode(__WAV2__);phase=1;ticks=0;return;}
   console.log("[CONTEXT-PROBE] "+JSON.stringify({cleared:cleared,recovered:true,tx:bridge.transmitting,lowCpu:bridge.lowCpuModeEnabled,startupLowCpu:cpuProbe.startedLowCpu,firstDecodeMs:firstDecodeMs,secondDecodeMs:Date.now()-decodeStart,switchMs:switchMs}));stop();Qt.quit();return;
  }}if(++ticks>=200){console.log("[CONTEXT-PROBE] timeout");stop();Qt.quit()}
 }}
'''.replace('__WAV1__',json.dumps(str(wav1))).replace('__WAV2__',json.dumps(str(wav2)))
if a.check_timing_bar:
 probe=probe.replace('startupLowCpu:cpuProbe.startedLowCpu,','startupLowCpu:cpuProbe.startedLowCpu,timingBarInterval:timingBarRefreshTimer.interval,')
try:
 with tempfile.TemporaryDirectory(prefix='decodium-context-lab-') as tmp:
  rig='Context'+secrets.token_hex(5)
  settings=Path(tmp)/'Decodium'/'Decodium3.ini';settings.parent.mkdir(parents=True)
  low=str(a.low_cpu).lower()
  settings.write_text('[General]\nLowCpuMode=%s\nlowCpuModeEnabled=%s\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=%s\nlowCpuModeEnabled=%s\n' % (low,low,rig,low,low))
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','125000']
  before=resource.getrusage(resource.RUSAGE_CHILDREN) if resource else None
  started=time.monotonic()
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=155)
  if a.log:a.log.write_text(result.stdout)
  wall=time.monotonic()-started
  after=resource.getrusage(resource.RUSAGE_CHILDREN) if resource else None
  lines=[x.split('[CONTEXT-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[CONTEXT-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0]=='timeout' or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('context probe failed')
  v=json.loads(lines[0]);
  expected={'cleared':True,'recovered':True,'tx':False,'lowCpu':a.low_cpu,'startupLowCpu':a.low_cpu}
  if a.check_timing_bar:expected['timingBarInterval']=100 if a.low_cpu else 50
  if any(v.get(k)!=value for k,value in expected.items()) or any(not isinstance(v.get(k),int) or v[k]<0 for k in ('firstDecodeMs','secondDecodeMs','switchMs')):raise RuntimeError('unexpected context state '+str(v))
  v['wallSeconds']=round(wall,3)
  if after:
   v['cpuSeconds']=round((after.ru_utime+after.ru_stime)-(before.ru_utime+before.ru_stime),3)
   # Linux reports KiB; macOS reports bytes.  This script starts one child.
   v['peakRssBytes']=int(after.ru_maxrss * (1 if sys.platform=='darwin' else 1024))
  print(json.dumps(v,sort_keys=True))
finally:qml.write_text(original)
