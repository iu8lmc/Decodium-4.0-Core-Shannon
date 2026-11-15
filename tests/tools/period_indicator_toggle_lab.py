#!/usr/bin/env python3
"""Verify period status and animation when LOW CPU is toggled at runtime."""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=root/'build-baseline/decodium');p.add_argument('--qml',type=Path,default=root/'build-baseline/qml/decodium/Main.qml');p.add_argument('--log',type=Path,required=True);a=p.parse_args()
qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''
 Timer {interval:5000;running:true;repeat:false;onTriggered:{periodTimer.stop();mainWindow.period1Detached=false;decodePanel.isCurrentPeriodEven=true;toggleStep.start()}}
 Timer {id:toggleStep;interval:550;repeat:true;running:false;property int step:0;property var results:({});onTriggered:{
  if(step===0){results.normalRunning=evenPeriodIndicatorPulse.running && evenPeriodBadgePulse.running;bridge.lowCpuModeEnabled=true;}
  else if(step===1){results.lowStopped=!evenPeriodIndicatorPulse.running && !evenPeriodBadgePulse.running;
    results.indicatorVisible=evenPeriodIndicator.visible;results.badgeVisible=evenPeriodBadge.visible;
    results.indicatorOpacity=evenPeriodIndicator.opacity;results.badgeOpacity=evenPeriodBadge.opacity;
    decodePanel.isCurrentPeriodEven=false;}
  else if(step===2){results.inactiveOpacity=evenPeriodIndicator.opacity;
    results.inactiveBadgeVisible=evenPeriodBadge.visible;decodePanel.isCurrentPeriodEven=true;}
  else if(step===3){results.reactivatedOpacity=evenPeriodIndicator.opacity;bridge.lowCpuModeEnabled=false;}
  else {results.resumed=evenPeriodIndicatorPulse.running && evenPeriodBadgePulse.running;
    results.mode=bridge.mode;results.tx=bridge.transmitting;console.log("[PERIOD-TOGGLE] "+JSON.stringify(results));stop();Qt.quit();return;}
  step++;
 }}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-period-toggle-') as tmp:
  rig='PeriodToggle'+secrets.token_hex(5);ini=Path(tmp)/'Decodium'/'Decodium3.ini';ini.parent.mkdir(parents=True)
  ini.write_text('[General]\nLowCpuMode=false\nlowCpuModeEnabled=false\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=false\nlowCpuModeEnabled=false\n'%rig)
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','45000']
  run=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=65);a.log.write_text(run.stdout)
  lines=[line.split('[PERIOD-TOGGLE] ',1)[1] for line in run.stdout.splitlines() if '[PERIOD-TOGGLE] ' in line]
  if run.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in run.stdout:raise RuntimeError(run.stdout[-8000:])
  data=json.loads(lines[0]);print(json.dumps(data,sort_keys=True))
  if not all(data.get(x) for x in ('normalRunning','lowStopped','indicatorVisible','badgeVisible','resumed')) or data.get('tx') or data.get('mode')!='FT8' or data.get('indicatorOpacity',0)<0.9 or data.get('badgeOpacity',0)<0.9 or abs(data.get('inactiveOpacity',-1)-0.5)>0.01 or data.get('inactiveBadgeVisible') or data.get('reactivatedOpacity',0)<0.9:
   raise RuntimeError('unexpected period status: '+repr(data))
finally:qml.write_text(original)
