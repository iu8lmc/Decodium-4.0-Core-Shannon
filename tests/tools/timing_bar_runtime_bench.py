#!/usr/bin/env python3
"""Measure a forced-active timing bar with CAT and audio monitoring disabled.

Run under Xvfb with the Qt runtime available; pass 50 or 100 as the interval.
This temporarily edits the build output QML and restores it after the run.
"""
import json, os, subprocess, sys, tempfile, time
from pathlib import Path
root=Path(__file__).resolve().parents[2]
qml=root/'build-baseline/qml/decodium/Main.qml'
original=qml.read_text()
interval=int(sys.argv[1])
probe='''
 Timer { interval:5000; running:true; repeat:false; onTriggered: {
   timingBar.timerActive=true; timingBarRefreshTimer.interval=INTERVAL;
   timingBarRefreshTimer.triggered.connect(function(){ timingMeasure.count++ });
   console.log("TIMING-MEASURE-START"); timingMeasure.start();
 }}
 Timer { id:timingMeasure; property int count:0; interval:10000; running:false; repeat:false; onTriggered: {
   console.log("TIMING-MEASURE-END "+count); Qt.quit();
 }}
'''.replace('INTERVAL',str(interval))
def cpu(pid):
 stat=Path('/proc/%d/stat'%pid).read_text().split(') ',1)[1].split()
 return (int(stat[11])+int(stat[12]))/os.sysconf('SC_CLK_TCK')
try:
 qml.write_text(original.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1))
 with tempfile.TemporaryDirectory(prefix='decodium-timing-measure-') as tmp:
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name','TimingMeasure','--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','60000']
  p=subprocess.Popen(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
  start=None;end=None;count=None
  for line in p.stdout:
   if 'TIMING-MEASURE-START' in line: start=(time.monotonic(),cpu(p.pid))
   if 'TIMING-MEASURE-END' in line:
    end=(time.monotonic(),cpu(p.pid));count=int(line.rsplit(' ',1)[1]);break
  rc=p.wait(timeout=20)
  if rc or not start or not end: raise RuntimeError('measurement failed rc=%s start=%s end=%s'%(rc,start,end))
  print(json.dumps({'interval_ms':interval,'ticks':count,'wall_s':round(end[0]-start[0],3),'cpu_s':round(end[1]-start[1],3)}))
finally:qml.write_text(original)
