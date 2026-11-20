#!/usr/bin/env python3
"""Measure active period indicator rendering with reception disabled."""
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
a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:5000;running:true;repeat:false;onTriggered:{
   periodTimer.stop();mainWindow.period1Detached=false;decodePanel.isCurrentPeriodEven=true;
   console.log("[PERIOD-START] "+JSON.stringify({low:bridge.lowCpuModeEnabled,quality:bridge.uiQuality,
       indicator:evenPeriodIndicatorPulse.running,badge:evenPeriodBadgePulse.running,
       indicatorVisible:evenPeriodIndicator.visible,badgeVisible:evenPeriodBadge.visible}));periodMeasure.start();
 }}
 Timer {id:periodMeasure;interval:10000;running:false;repeat:false;onTriggered:{
   console.log("[PERIOD-END] "+JSON.stringify({indicator:evenPeriodIndicatorPulse.running,badge:evenPeriodBadgePulse.running,
       even:decodePanel.isCurrentPeriodEven,tx:bridge.transmitting}));Qt.quit();
 }}
'''
def cpu(pid):
    fields=(Path('/proc')/str(pid)/'stat').read_text().split(') ',1)[1].split()
    return (int(fields[11])+int(fields[12]))/os.sysconf('SC_CLK_TCK')
try:
    qml.write_text(original.replace(marker,marker+probe,1))
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
        if rc or not start or not end or not state_start.get('low') or not state_start.get('indicatorVisible') or not state_start.get('badgeVisible') or not state_end.get('even') or state_end.get('tx'):
            raise RuntimeError('unexpected benchmark state '+repr((rc,state_start,state_end)))
        result={'seconds':round(end[0]-start[0],3),'cpuSeconds':round(end[1]-start[1],3),
                'initial':state_start,'final':state_end}
        a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,sort_keys=True))
finally:qml.write_text(original)
