#!/usr/bin/env python3
"""Exercise changing the FT8 thread request across repeated decodes in one process."""
import json, os, re, secrets, subprocess, tempfile, sys, time
from pathlib import Path

root=Path(__file__).resolve().parents[2]
qml=root/'build-baseline/qml/decodium/Main.qml'; original=qml.read_text()
marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav=root/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'
low_toggle='--low-toggle' in sys.argv[1:]
idle_window='--idle-window' in sys.argv[1:]
probe='''
 Timer { interval:12000;running:true;repeat:false;onTriggered:budgetProbe.start() }
 Connections {target:bridge;function onDecodingChanged(){if(bridge.decoding && budgetProbe.phase===2)budgetProbe.seen=true}}
 Timer { id:budgetProbe;interval:300;running:false;repeat:true;
  property int step:0;property int phase:0;property bool seen:false;property int settle:0;
  property var budgets:[1,3,1,3,1,3];property var rowsByStep:[];
  onTriggered:{
   if(step>=budgets.length){console.log("[BUDGET-TRANSITION] "+JSON.stringify({rows:rowsByStep,lowCpu:bridge.lowCpuModeEnabled,tx:bridge.transmitting}));stop();Qt.quit();return}
   if(phase===0){__CONFIG__ settle=0;phase=1;return}
   if(phase===1){if(++settle<3)return;bridge.clearDecodeList();seen=false;phase=2;bridge.openWavForDecode(__WAV__);if(bridge.decoding)seen=true;return}
   if(phase===2){if(bridge.decoding)seen=true;else if(seen){settle=0;phase=3};return}
   if(++settle<4)return;
   var out=[];var rows=bridge.decodeList;
   for(var i=0;i<rows.length;i++)out.push({message:String(rows[i].message),freq:String(rows[i].freq)});
   rowsByStep.push({requested:budgets[step],low:bridge.lowCpuModeEnabled,rows:out});
   console.log("[BUDGET-STEP] "+JSON.stringify({step:step,budget:budgets[step]}));step++;phase=0;
  }
 }
'''.replace('__WAV__',json.dumps(str(wav)))
if low_toggle:
 probe=probe.replace('property var budgets:[1,3,1,3,1,3]','property var budgets:[2,3,2,3,2,3]')
 probe=probe.replace('__CONFIG__','bridge.ftThreads=3;bridge.lowCpuModeEnabled=(step%2===0);')
else:
 probe=probe.replace('__CONFIG__','bridge.ftThreads=budgets[step];')
if idle_window:
 if not low_toggle:raise RuntimeError('idle window requires --low-toggle')
 probe=probe.replace('if(step>=budgets.length)', 'if(phase===4){if(++settle>=20){console.log("[BUDGET-IDLE-END]");step++;phase=0}return}\n   if(step>=budgets.length)')
 probe=probe.replace('step++;phase=0;\n  }', 'if(step===2){phase=4;settle=0;console.log("[BUDGET-IDLE-START]");return}step++;phase=0;\n  }')
try:
 qml.write_text(original.replace(marker,marker+probe,1))
 with tempfile.TemporaryDirectory(prefix='decodium-ft8-budget-') as tmp:
  rig='Budget'+secrets.token_hex(5);ini=Path(tmp)/'Decodium'/'Decodium3.ini';ini.parent.mkdir(parents=True)
  ini.write_text('[General]\nLowCpuMode=false\nlowCpuModeEnabled=false\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=false\nlowCpuModeEnabled=false\n'%rig)
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','85000']
  p=subprocess.Popen(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
  output=[];snapshots=[];idle_samples=[]
  try:
   for line in p.stdout:
    output.append(line)
    if '[BUDGET-STEP] ' in line:
     info=json.loads(line.split('[BUDGET-STEP] ',1)[1]);thread_ids=[]
     for task in (Path('/proc')/str(p.pid)/'task').iterdir():
      try:
       if (task/'comm').read_text().strip()=='FT8DecodeWorker':thread_ids.append(int(task.name))
      except FileNotFoundError:pass
     info['ft8ThreadIds']=sorted(thread_ids);snapshots.append(info)
    if '[BUDGET-IDLE-START]' in line or '[BUDGET-IDLE-END]' in line:
     threads={}
     for task in (Path('/proc')/str(p.pid)/'task').iterdir():
      try:
       if (task/'comm').read_text().strip()=='FT8DecodeWorker':
        fields=(task/'stat').read_text().rsplit(')',1)[1].split()
        threads[task.name]={'state':fields[0],'ticks':int(fields[11])+int(fields[12])}
      except FileNotFoundError:pass
     idle_samples.append({'time':time.monotonic(),'threads':threads})
    if '[BUDGET-TRANSITION] ' in line:break
   returncode=p.wait(timeout=15)
  finally:
   if p.poll() is None:p.terminate();p.wait(timeout=10)
  stdout=''.join(output)
  log=root/('build-baseline/performance/results/ft8-'+('low-cpu-' if low_toggle else '')+'thread-transition.log');log.write_text(stdout)
  lines=[line.split('[BUDGET-TRANSITION] ',1)[1] for line in stdout.splitlines() if '[BUDGET-TRANSITION] ' in line]
  if returncode or len(lines)!=1 or len(snapshots)!=6:raise RuntimeError('transition failed; see '+str(log))
  result=json.loads(lines[0]);metrics=re.findall(r'\[FT8DISPATCH\].*?threads=(\d+)/(\d+) cores=(\d+)',stdout)
  if result['tx'] or result['lowCpu'] or len(result['rows'])!=6 or any(not any(r['message']=='CQ K1ABC FN42' and r['freq']=='1500' for r in step['rows']) for step in result['rows']):raise RuntimeError('decode mismatch '+repr(result))
  expected=[2,3,2,3,2,3] if low_toggle else [1,3,1,3,1,3]
  if low_toggle and [step['low'] for step in result['rows']] != [True,False,True,False,True,False]:raise RuntimeError('LOW CPU state did not alternate')
  if len(metrics)!=6 or [int(m[0]) for m in metrics] != expected:raise RuntimeError('thread budgets did not alternate: '+repr(metrics))
  if idle_window and len(idle_samples)!=2:raise RuntimeError('missing idle samples')
  print(json.dumps({'requested':[step['requested'] for step in result['rows']],'metrics':metrics,'decodes':len(result['rows']),'threadSnapshots':snapshots,'idleSamples':idle_samples,'clockTicksPerSecond':os.sysconf('SC_CLK_TCK')}))
finally:qml.write_text(original)
