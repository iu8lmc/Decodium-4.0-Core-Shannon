#!/usr/bin/env python3
"""Check unchanged and externally modified Logbook snapshots in the real UI."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
main = root/'build-baseline/qml/decodium/Main.qml'
window = root/'build-baseline/qml/decodium/components/LogWindow.qml'
old_main, old_window = main.read_text(), window.read_text()
setup = '''
 property string labPath:""
 Timer {interval:8000;running:true;onTriggered:{
  var name="Revision__RIG__";if(!bridge.createLogbook(name,false)){console.log("[REV-ERROR] create");return}
  var profiles=bridge.logbookProfiles();for(var i=0;i<profiles.length;i++)if(profiles[i].name===name)labPath=profiles[i].path;
  if(labPath.indexOf("/.qttest/")<0 || !bridge.importAdif(__SOURCE__))console.log("[REV-ERROR] import");
  bridge.warmLogCacheAsync();
 }}
 Timer {interval:14000;running:true;onTriggered:mainWindow.openLogWindow()}
 Timer {interval:26000;running:true;onTriggered:{if(labPath.indexOf("/.qttest/")>=0)bridge.deleteLogbook(labPath,true);Qt.quit()}}
'''
probe = '''
 property int labRefresh:0
 Timer {interval:5000;running:true;onTriggered:{
  labRefresh=0;refreshLogIfChanged();
  console.log("[REV-UNCHANGED] "+JSON.stringify({refreshes:labRefresh,rows:qsoList.length,low:bridge.lowCpuModeEnabled}));
  console.log("[REV-WRITE] "+mainWindow.labPath);labCheck.start();
 }}
 Timer {id:labCheck;interval:2500;running:false;onTriggered:{
  bridge.getQsoStats();labRefresh=0;refreshLogIfChanged();
  var after=labRefresh,rows=qsoList.length;
  searchField.text="W9XYZ";refreshLog();
  console.log("[REV-RESULT] "+JSON.stringify({refreshes:after,rows:rows,filtered:qsoList.length,
    call:qsoList.length?String(qsoList[0].call):"",tx:bridge.transmitting,monitor:bridge.monitoring}));
 }}
'''
try:
    if old_main.count('GlassApplicationWindow {')!=1 or old_window.count('    id: logWindow\n')!=1:
        raise RuntimeError('unexpected QML structure')
    with tempfile.TemporaryDirectory(prefix='decodium-revision-') as tmp:
        rig='Revision'+secrets.token_hex(5)
        fixture=Path(tmp)/'test.adi'
        fixture.write_text('Synthetic ADIF\n<EOH>\n<CALL:5>K1ABC <QSO_DATE:8>20261107 <TIME_ON:6>120000 <BAND:3>20M <MODE:3>FT8 <EOR>\n')
        main.write_text(old_main.replace('GlassApplicationWindow {','GlassApplicationWindow {'+setup.replace('__RIG__',rig).replace('__SOURCE__',json.dumps(str(fixture))),1))
        window.write_text(old_window.replace('    id: logWindow\n','    id: logWindow\n'+probe,1).replace('    function refreshLog() {\n','    function refreshLog() {\n        ++labRefresh\n',1))
        ini=Path(tmp)/'Decodium/Decodium3.ini';ini.parent.mkdir(parents=True)
        ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n')
        env=dict(os.environ,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','35000']
        proc=subprocess.Popen(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
        lines=[];unchanged=None;result=None
        try:
            for line in proc.stdout:
                lines.append(line)
                if '[REV-UNCHANGED] ' in line:unchanged=json.loads(line.split('[REV-UNCHANGED] ',1)[1])
                if '[REV-WRITE] ' in line:
                    path=Path(line.split('[REV-WRITE] ',1)[1].strip())
                    if '/.qttest/' not in str(path) or rig not in str(path):raise RuntimeError('unsafe test path')
                    with path.open('a') as f:f.write('<CALL:5>W9XYZ <QSO_DATE:8>20261107 <TIME_ON:6>130000 <BAND:3>20M <MODE:3>FT8 <EOR>\n')
                if '[REV-RESULT] ' in line:result=json.loads(line.split('[REV-RESULT] ',1)[1])
            rc=proc.wait(timeout=10)
        finally:
            if proc.poll() is None:proc.terminate();proc.wait(timeout=10)
        (root/'build-baseline/performance/results/log-revision-runtime.log').write_text(''.join(lines))
        if rc or '[REV-ERROR]' in ''.join(lines) or unchanged!={'refreshes':0,'rows':1,'low':True} or result!={'refreshes':1,'rows':2,'filtered':1,'call':'W9XYZ','tx':False,'monitor':False}:
            raise RuntimeError('unexpected result: '+repr((unchanged,result))+''.join(lines[-30:]))
        print(json.dumps({'unchanged':unchanged,'externalAfterCacheRefresh':result},sort_keys=True))
finally:
    main.write_text(old_main);window.write_text(old_window)
