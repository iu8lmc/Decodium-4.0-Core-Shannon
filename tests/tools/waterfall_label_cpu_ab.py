#!/usr/bin/env python3
"""Compare repeated identical waterfall label snapshots in the real app."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[2];main=root/'build-baseline/qml/decodium/Main.qml';component=root/'build-baseline/qml/decodium/components/Waterfall.qml'
original_main,original_component=main.read_text(),component.read_text()
start=original_component.index('    function updateSpectrumDecodeLabels(labels) {')
end=original_component.index('\n    function setShowDecodeCallsigns(',start)
reference=original_component[:start]+'''    function updateSpectrumDecodeLabels(labels) {
        spectrumDecodeLabels = labels
    }
'''+original_component[end:]
probe='''
 Connections {target:waterfallEmbeddedLoader.item;ignoreUnknownSignals:true;function onSpectrumDecodeLabelsChanged(){labelRepeat.changes++}}
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
   var w=waterfallEmbeddedLoader.item;if(!w || !w.visible){console.log('[LABEL-FAIL]');Qt.quit();return}
   labelRepeat.w=w;labelRepeat.running=true; labelEnd.start();
   console.log('[LABEL-START] '+JSON.stringify({visible:w.visible,low:bridge.lowCpuModeEnabled,tx:bridge.transmitting,cat:bridge.catConnected}))
 }}
 Timer {id:labelRepeat; interval:100;repeat:true;running:false;property var w;property int changes:0;property int ticks:0;
  onTriggered:{var labels=[];for(var i=0;i<30;++i)labels.push({call:'K'+i+'ABC',freq:300+i*85,snr:-10-i%15,isCQ:true,isMyCall:false,color:'#00cc33'});
    w.updateSpectrumDecodeLabels(labels);ticks++}}
 Timer {id:labelEnd;interval:10000;repeat:false;onTriggered:{labelRepeat.stop();var labels=labelRepeat.w.spectrumDecodeLabels;
   console.log('[LABEL-END] '+JSON.stringify({visible:labelRepeat.w.visible,low:bridge.lowCpuModeEnabled,tx:bridge.transmitting,cat:bridge.catConnected,
    ticks:labelRepeat.ticks,changes:labelRepeat.changes,count:labels.length,first:labels[0].call,last:labels[labels.length-1].call}));Qt.quit()}}
'''
def cpu(pid):
 fields=(Path('/proc')/str(pid)/'stat').read_text().split(') ',1)[1].split();return (int(fields[11])+int(fields[12]))/os.sysconf('SC_CLK_TCK')
out=root/'build-baseline/performance/results';results=[]
try:
 main.write_text(original_main.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1))
 for index,(label,text) in enumerate((('reference',reference),('current',original_component),('current',original_component),('reference',reference))):
  component.write_text(text)
  with tempfile.TemporaryDirectory(prefix='decodium-label-cpu-') as tmp:
   rig='LabelCpu'+secrets.token_hex(5);ini=Path(tmp)/'Decodium'/'Decodium3.ini';ini.parent.mkdir(parents=True)
   ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n'%rig)
   env=dict(os.environ,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
   cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','45000']
   proc=subprocess.Popen(cmd,cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
   lines=[];first=last=a=b=None
   try:
    for line in proc.stdout:
     lines.append(line)
     if '[LABEL-START] ' in line:a=json.loads(line.split('[LABEL-START] ',1)[1]);first=(time.monotonic(),cpu(proc.pid))
     if '[LABEL-END] ' in line:b=json.loads(line.split('[LABEL-END] ',1)[1]);last=(time.monotonic(),cpu(proc.pid));break
    rc=proc.wait(timeout=15)
   finally:
    if proc.poll() is None:proc.terminate();proc.wait(timeout=10)
    (out/f'waterfall-label-cpu-{index}.log').write_text(''.join(lines))
   if rc or not first or not last or not a or not b:raise RuntimeError('invalid measurement '+''.join(lines[-50:]))
   if not all(x['visible'] and x['low'] and not x['tx'] and not x['cat'] for x in (a,b)) or b['ticks']<90 or b['count']!=30 or b['first']!='K0ABC' or b['last']!='K29ABC':raise RuntimeError('unexpected UI state: '+repr((a,b)))
   if label=='current' and b['changes']!=1:raise RuntimeError('duplicate notifications '+repr(b))
   if label=='reference' and b['changes']<90:raise RuntimeError('reference did not refresh '+repr(b))
   row={'variant':label,'cpuSeconds':round(last[1]-first[1],3),'wallSeconds':round(last[0]-first[0],3),'state':b};results.append(row);print(json.dumps(row,sort_keys=True),flush=True)
 (out/'waterfall-label-cpu-ab.json').write_text(json.dumps(results,indent=2)+'\n')
finally:
 main.write_text(original_main);component.write_text(original_component)
