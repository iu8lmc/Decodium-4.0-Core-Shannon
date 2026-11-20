#!/usr/bin/env python3
"""Measure actual Logbook refreshes with an isolated synthetic ADIF file."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
main = root / 'build-baseline/qml/decodium/Main.qml'
log_window = root / 'build-baseline/qml/decodium/components/LogWindow.qml'
original_main, original_log = main.read_text(), log_window.read_text()
main_probe = '''
 property string labPath:""
 Timer {interval:8000;running:true;onTriggered:{
   var name="Refresh__RIG__";
   if(!bridge.createLogbook(name,false)) {console.log("[LOG-COST-ERROR] create");return}
   var profiles=bridge.logbookProfiles();
   for(var i=0;i<profiles.length;i++)if(profiles[i].name===name)labPath=profiles[i].path;
   if(labPath.indexOf("/.qttest/")<0 || !bridge.importAdif(__SOURCE__)) {console.log("[LOG-COST-ERROR] import");return}
   bridge.warmLogCacheAsync();
 }}
 Timer {interval:15000;running:true;onTriggered:mainWindow.openLogWindow()}
 Timer {interval:28000;running:true;onTriggered:{if(labPath.indexOf("/.qttest/")>=0)bridge.deleteLogbook(labPath,true);Qt.quit()}}
'''
probe = '''
 Timer {interval:5000;running:true;onTriggered:{
   var samples=[];var search=[];var statistics=[];var count=0;
   for(var i=0;i<5;i++){var start=Date.now();refreshLog();samples.push(Date.now()-start);count=qsoList.length}
   for(var j=0;j<5;j++){var begin=Date.now();var rows=appEngine.logManager.searchQsos("","","","","");search.push(Date.now()-begin);
     begin=Date.now();var result=statsFromRows(rows);statistics.push(Date.now()-begin)}
   var checks=[];for(var k=0;k<5;k++){var checkStart=Date.now();for(var n=0;n<100;n++)refreshLogIfChanged();checks.push(Date.now()-checkStart)}
   console.log("[LOG-COST] "+JSON.stringify({ms:samples,searchMs:search,statsMs:statistics,unchangedChecks100Ms:checks,count:count,stats:stats.totalQsos,
     visible:logWindow.visible,low:bridge.lowCpuModeEnabled,tx:bridge.transmitting,monitor:bridge.monitoring}));
 }}
'''
try:
    if original_main.count('GlassApplicationWindow {') != 1 or original_log.count('    id: logWindow\n') != 1:
        raise RuntimeError('unexpected QML structure')
    with tempfile.TemporaryDirectory(prefix='decodium-log-cost-') as tmp:
        rig = 'LogCost' + secrets.token_hex(5)
        fixture = Path(tmp) / 'synthetic.adi'
        with fixture.open('w') as f:
            f.write('Synthetic ADIF\n<EOH>\n')
            for i in range(3000):
                f.write(f'<CALL:6>K{i:05d} <QSO_DATE:8>20261107 <TIME_ON:6>120000 <BAND:3>20M <MODE:3>FT8 <GRIDSQUARE:4>FN42 <EOR>\n')
        setup = main_probe.replace('__RIG__', rig).replace('__SOURCE__', json.dumps(str(fixture)))
        main.write_text(original_main.replace('GlassApplicationWindow {', 'GlassApplicationWindow {' + setup, 1))
        log_window.write_text(original_log.replace('    id: logWindow\n', '    id: logWindow\n' + probe, 1))
        ini = Path(tmp) / 'Decodium/Decodium3.ini'
        ini.parent.mkdir(parents=True)
        ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n')
        env = dict(os.environ, QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
                   DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd = [str(root/'build-baseline/decodium'), '--test-mode', '--rig-name', rig,
               '--lab-no-cat', '--lab-no-monitor', '--lab-callsign', 'IU8LMC', '--lab-mode', 'FT8', '--lab-quit-ms', '35000']
        app = subprocess.run(cmd, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=45)
        rows = [json.loads(line.split('[LOG-COST] ', 1)[1]) for line in app.stdout.splitlines() if '[LOG-COST] ' in line]
        result_dir = root/'build-baseline/performance/results'
        (result_dir/'log-refresh-cost.log').write_text(app.stdout)
        if app.returncode or '[LOG-COST-ERROR]' in app.stdout or len(rows) != 1:
            raise RuntimeError(app.stdout[-8000:])
        state = rows[0]
        if state['count'] != 3000 or state['stats'] != 3000 or not state['visible'] or not state['low'] or state['tx'] or state['monitor']:
            raise RuntimeError('unexpected state: '+repr(state))
        (result_dir/'log-refresh-cost.json').write_text(json.dumps(state, indent=2)+'\n')
        print(json.dumps(state, sort_keys=True))
finally:
    main.write_text(original_main)
    log_window.write_text(original_log)
