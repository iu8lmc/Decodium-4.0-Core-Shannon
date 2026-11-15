#!/usr/bin/env python3
"""Measure minimized-window indicator animation in an isolated LOW CPU profile."""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument('--binary',type=Path,default=root/'build-baseline/decodium')
p.add_argument('--qml',type=Path,default=root/'build-baseline/qml/decodium/Main.qml')
p.add_argument('--output',type=Path,required=True)
p.add_argument('--log',type=Path,required=True)
p.add_argument('--force-animation',action='store_true',help='force the ten animations on in LOW CPU')
p.add_argument('--toggle',action='store_true',help='check animations and opacity through LOW CPU profile changes')
a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:5000;running:true;repeat:false;onTriggered:{
   mainWindow.waterfallMinimized=true;mainWindow.logWindowMinimized=true;mainWindow.astroWindowMinimized=true;
   mainWindow.liveMapMinimized=true;mainWindow.macroDialogMinimized=true;mainWindow.rigControlMinimized=true;
   mainWindow.period1Minimized=true;mainWindow.period2Minimized=true;mainWindow.rxFreqMinimized=true;mainWindow.txPanelMinimized=true;
   console.log("[PERIOD-START] "+JSON.stringify({low:bridge.lowCpuModeEnabled,quality:bridge.uiQuality,
       indicator:waterfallRestorePulse.running,indicatorVisible:waterfallRestoreMA.parent.visible}));
   if (TEST_TOGGLE) { bridge.lowCpuModeEnabled=false; restoreToggle.start() } else periodMeasure.start();
 }}
 Timer {id:restoreToggle;interval:700;running:false;repeat:true;property int step:0;property var results:({});onTriggered:{
   var pulses=[waterfallRestorePulse,pulse_logWindow,pulse_astroWindow,pulse_liveMap,pulse_macroDialog,pulse_rigControl,pulse_period1,pulse_period2,pulse_rxFreq,pulse_txPanel];
   var items=[waterfallRestoreMA.parent,logRestoreMA.parent,astroRestoreMA.parent,liveMapRestoreMA.parent,macroRestoreMA.parent,rigRestoreMA.parent,p1RestoreMA.parent,p2RestoreMA.parent,rxRestoreMA.parent,txRestoreMA.parent];
   if(step===0){results.normal=pulses.every(function(x){return x.running});bridge.lowCpuModeEnabled=true}
   else if(step===1){results.stopped=pulses.every(function(x){return !x.running});results.visible=items.every(function(x){return x.visible && x.opacity>0.99});bridge.lowCpuModeEnabled=false}
   else if(step===2){results.resumed=pulses.every(function(x){return x.running});bridge.lowCpuModeEnabled=true}
   else {results.stoppedAgain=pulses.every(function(x){return !x.running});results.visibleAgain=items.every(function(x){return x.visible && x.opacity>0.99});console.log("[RESTORE-TOGGLE] "+JSON.stringify(results));stop();periodMeasure.start()};step++
 }}
 Timer {id:periodMeasure;interval:10000;running:false;repeat:false;onTriggered:{
   console.log("[PERIOD-END] "+JSON.stringify({indicator:waterfallRestorePulse.running,
       visible:waterfallRestoreMA.parent.visible,tx:bridge.transmitting}));Qt.quit();
 }}
'''
probe=probe.replace('TEST_TOGGLE','true' if a.toggle else 'false')
def cpu(pid):
    fields=(Path('/proc')/str(pid)/'stat').read_text().split(') ',1)[1].split()
    return (int(fields[11])+int(fields[12]))/os.sysconf('SC_CLK_TCK')
try:
    instrumented=original.replace(marker,marker+probe,1)
    names=('waterfall','logWindow','astroWindow','liveMap','macroDialog','rigControl','period1','period2','rxFreq','txPanel')
    for name in names:
        condition='running: '+name+'Minimized && bridge && !bridge.lowCpuModeEnabled && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"'
        if instrumented.count(condition)!=1: raise RuntimeError('unexpected animation condition: '+name)
        replacement=condition.replace(' && !bridge.lowCpuModeEnabled','') if a.force_animation else condition
        replacement='id: '+('waterfallRestorePulse' if name=='waterfall' else 'pulse_'+name)+'\n                        '+replacement
        instrumented=instrumented.replace(condition,replacement,1)
    qml.write_text(instrumented)
    with tempfile.TemporaryDirectory(prefix='decodium-period-bench-') as tmp:
        rig='Period'+secrets.token_hex(5); ini=Path(tmp)/'Decodium'/'Decodium3.ini';ini.parent.mkdir(parents=True)
        ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n'%rig)
        env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor',
             '--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','60000']
        process=subprocess.Popen(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
        lines=[];start=end=None; state_start=state_end=None
        try:
            for line in process.stdout:
                lines.append(line)
                if '[PERIOD-START] ' in line:
                    state_start=json.loads(line.split('[PERIOD-START] ',1)[1]);start=(time.monotonic(),cpu(process.pid))
                if '[PERIOD-END] ' in line:
                    state_end=json.loads(line.split('[PERIOD-END] ',1)[1]);end=(time.monotonic(),cpu(process.pid));break
            rc=process.wait(timeout=20)
        finally:
            if process.poll() is None:process.terminate();process.wait(timeout=10)
            a.log.write_text(''.join(lines))
        toggle_lines=[line.split('[RESTORE-TOGGLE] ',1)[1] for line in lines if '[RESTORE-TOGGLE] ' in line]
        if a.toggle and (len(toggle_lines)!=1 or not all(json.loads(toggle_lines[0]).get(k) for k in ('normal','stopped','visible','resumed','stoppedAgain','visibleAgain'))):
            raise RuntimeError('profile toggle failed '+repr(toggle_lines))
        if rc or not start or not end or not state_start.get('low') or not state_start.get('indicatorVisible') or not state_end.get('visible') or state_end.get('tx') or state_start.get('indicator') != a.force_animation or state_end.get('indicator') != a.force_animation:
            raise RuntimeError('unexpected benchmark state '+repr((rc,state_start,state_end)))
        result={'seconds':round(end[0]-start[0],3),'cpuSeconds':round(end[1]-start[1],3),
                'initial':state_start,'final':state_end}
        a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,sort_keys=True))
finally:qml.write_text(original)
