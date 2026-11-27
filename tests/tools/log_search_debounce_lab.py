#!/usr/bin/env python3
"""Check rapid log search and immediate refresh in both actual QML panels."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
qml_dir = root/'build-baseline/qml/decodium'
main = qml_dir/'Main.qml'
components = [qml_dir/'components/LogWindow.qml', qml_dir/'components/LogWindowContent.qml']
original_main = main.read_text()
originals = {path:path.read_text() for path in components}
probe = '''
    property int labRefreshCount: 0
    Timer {interval:3000; running:true; repeat:false; onTriggered: {
        labRefreshCount=0; searchField.text="K"; searchField.text="K1"; searchField.text="K1ABC";
        labWait.start();
    }}
    Timer {id:labWait; interval:450; repeat:false; onTriggered:{
        var first=labRefreshCount;
        var expected=appEngine.logManager.searchQsos("K1ABC", "", "", "", "");
        var same=expected.length===1 && JSON.stringify(qsoList)===JSON.stringify(expected);
        labRefreshCount=0; searchField.text="K1AB"; refreshLog();
        labFinish.first=first; labFinish.same=same; labFinish.start();
    }}
    Timer {id:labFinish; property int first:0; property bool same:false;
        interval:450; repeat:false; onTriggered:{
            var expected=appEngine.logManager.searchQsos("K1AB", "", "", "", "");
            console.log("[LOG-SEARCH-LAB] __NAME__ "+JSON.stringify({first:first,same:same,
                immediate:labRefreshCount, finalMatches:JSON.stringify(qsoList)===JSON.stringify(expected),
                low:bridge.lowCpuModeEnabled,tx:bridge.transmitting,tune:bridge.tuning}));
        }}
'''
main_probe = '''
    property var labLogContent: null
    property string labLogPath: ""
    Timer {interval:8000;running:true;repeat:false;onTriggered:{
        var name="Synthetic__RIG__";
        if (!bridge.createLogbook(name,false)) { console.log("[LOG-SEARCH-SETUP-ERROR] create"); return }
        var profiles=bridge.logbookProfiles();
        for(var i=0;i<profiles.length;i++) if(profiles[i].name===name) labLogPath=profiles[i].path;
        if(labLogPath.indexOf("/.qttest/")<0 || !bridge.importAdif(__SOURCE__))
            console.log("[LOG-SEARCH-SETUP-ERROR] import");
        bridge.warmLogCacheAsync();
    }}
    Timer {interval:12000;running:true;repeat:false;onTriggered:{
        mainWindow.openLogWindow();
        var c=Qt.createComponent("components/LogWindowContent.qml");
        if (c.status===Component.Ready) labLogContent=c.createObject(mainWindow.contentItem, {width:900,height:600});
        else console.log("[LOG-SEARCH-CREATE-ERROR] "+c.errorString());
    }}
    Timer {interval:22000;running:true;repeat:false;onTriggered:{
        if(labLogPath.length>0) bridge.deleteLogbook(labLogPath,true); Qt.quit();
    }}
'''
try:
    if original_main.count('GlassApplicationWindow {') != 1:
        raise RuntimeError('unexpected main component')
    for path in components:
        text = originals[path]
        anchor = '    function refreshLog() {\n'
        root_id = '    id: logWindow\n' if path.name == 'LogWindow.qml' else '    id: logContent\n'
        if text.count(anchor) != 1 or text.count(root_id) != 1:
            raise RuntimeError('unexpected component '+str(path))
        text = text.replace(root_id,root_id+probe.replace('__NAME__',path.stem),1)
        text = text.replace(anchor,anchor+'        ++labRefreshCount\n',1)
        path.write_text(text)
    with tempfile.TemporaryDirectory(prefix='decodium-log-search-') as tmp:
        rig='LogSearch'+secrets.token_hex(5)
        fixture=Path(tmp)/'fixture.adi'
        fixture.write_text('Synthetic ADIF\n<EOH>\n<CALL:5>K1ABC <QSO_DATE:8>20261107 <TIME_ON:6>120000 <BAND:3>20M <MODE:3>FT8 <EOR>\n')
        setup=main_probe.replace('__RIG__',rig).replace('__SOURCE__',json.dumps(str(fixture)))
        main.write_text(original_main.replace('GlassApplicationWindow {','GlassApplicationWindow {'+setup,1))
        settings=Path(tmp)/'Decodium/Decodium3.ini';settings.parent.mkdir(parents=True)
        settings.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n')
        env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',
            DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name',rig,
             '--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','30000']
        app=subprocess.run(cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=40)
        states={}
        for line in app.stdout.splitlines():
            if '[LOG-SEARCH-LAB] ' in line:
                name, data=line.split('[LOG-SEARCH-LAB] ',1)[1].split(' ',1)
                states[name]=json.loads(data)
        if app.returncode or '[LOG-SEARCH-SETUP-ERROR]' in app.stdout or set(states)!={'LogWindow','LogWindowContent'}:
            raise RuntimeError(app.stdout[-10000:])
        for state in states.values():
            if state['first']!=1 or state['immediate']!=1 or not state['same'] or not state['finalMatches'] or not state['low'] or state['tx'] or state['tune']:
                raise RuntimeError('search behavior incorrect: '+repr(states))
        print(json.dumps(states,sort_keys=True))
finally:
    main.write_text(original_main)
    for path,text in originals.items():path.write_text(text)
