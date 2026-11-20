#!/usr/bin/env python3
"""Measure WSPR-to-FT8 recovery while a file-based WSPR decode is active."""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--binary', type=Path, required=True)
p.add_argument('--qml', type=Path, default=root/'build-baseline/qml/decodium/Main.qml')
p.add_argument('--wspr', type=Path, required=True)
p.add_argument('--log', type=Path, required=True)
p.add_argument('--low-cpu', action='store_true')
p.add_argument('--return-wspr', action='store_true', help='decode WSPR again after FT8 recovers')
a = p.parse_args()
qml = a.qml.resolve()
original = qml.read_text()
marker = 'GlassApplicationWindow {'
if original.count(marker) != 1 or not a.wspr.is_file():
    raise RuntimeError('unexpected QML root or missing WSPR WAV')
ft8 = root/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'
probe = '''
 Timer {interval:12000; running:true; repeat:false; onTriggered: {
   bridge.openWavForDecode(__WSPR__); awaitWspr.start();
 }}
 Timer {id:awaitWspr; interval:50; running:false; repeat:true; property int ticks:0;
   onTriggered: {
     if(bridge.decoding) { stop(); switchToFt8.start(); }
     else if(++ticks>400) {console.log("[WSPR-SWITCH] no-wspr-start");stop();Qt.quit();}
 }}
 Timer {id:switchToFt8; interval:1500; running:false; repeat:false; onTriggered: {
   waitFt8.wsprStillActive=bridge.decoding;
   var begin=Date.now(); bridge.mode="FT8"; waitFt8.syncMs=Date.now()-begin;
   waitFt8.startMs=Date.now(); bridge.openWavForDecode(__FT8__); waitFt8.start();
 }}
 Timer {id:waitFt8; interval:250; running:false; repeat:true; property int ticks:0;
   property bool wsprStillActive:false; property double startMs:0; property int syncMs:0;
   onTriggered: {
     var rows=bridge.decodeList;
     for(var i=0;i<rows.length;i++) if(String(rows[i].message).indexOf("CQ K1ABC FN42")>=0) {
       var data={wsprStillActive:wsprStillActive,
         mode:bridge.mode,lowCpu:bridge.lowCpuModeEnabled,tx:bridge.transmitting,
         syncMs:syncMs,recoveryMs:Date.now()-startMs};
       stop(); __AFTER_FT8__ return;
     }
     if(++ticks>320) {console.log("[WSPR-SWITCH] timeout");stop();Qt.quit();}
 }}
 Timer {id:waitReturn; interval:250; running:false; repeat:true;
   property var result:({}); property bool seen:false; property int ticks:0; property int quiet:0;
   onTriggered: {
     if(bridge.decoding) seen=true;
     if(seen && !bridge.decoding && ++quiet>=4) {
       result.returnWsprRows=bridge.decodeList.length;
       console.log("[WSPR-SWITCH] "+JSON.stringify(result));stop();Qt.quit();return;
     }
     if(++ticks>360) {console.log("[WSPR-SWITCH] return-timeout");stop();Qt.quit();}
 }}
'''
action = ('waitReturn.result=data; bridge.mode="WSPR"; bridge.openWavForDecode(__WSPR__); waitReturn.start();'
          if a.return_wspr else 'console.log("[WSPR-SWITCH] "+JSON.stringify(data));Qt.quit();')
probe = probe.replace('__AFTER_FT8__',action).replace('__WSPR__',json.dumps(str(a.wspr.resolve()))).replace('__FT8__',json.dumps(str(ft8)))
try:
    with tempfile.TemporaryDirectory(prefix='decodium-wspr-switch-') as tmp:
        rig='WsprSwitch'+secrets.token_hex(5)
        settings=Path(tmp)/'Decodium'/'Decodium3.ini'
        settings.parent.mkdir(parents=True)
        low=str(a.low_cpu).lower()
        settings.write_text('[General]\nLowCpuMode=%s\nlowCpuModeEnabled=%s\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=%s\nlowCpuModeEnabled=%s\n' % (low,low,rig,low,low))
        qml.write_text(original.replace(marker,marker+probe,1))
        env=os.environ.copy()
        env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', DECODIUM_FORCE_STANDALONE_UDP='1', DECODIUM_MODE_PROFILE='1')
        cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor',
             '--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','WSPR','--lab-quit-ms','120000']
        start=time.monotonic()
        result=subprocess.run(cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=145)
        a.log.write_text(result.stdout)
        lines=[line.split('[WSPR-SWITCH] ',1)[1] for line in result.stdout.splitlines() if '[WSPR-SWITCH] ' in line]
        if result.returncode or len(lines)!=1 or lines[0] in ('timeout','return-timeout','no-wspr-start') or '[LAB] CAT auto-connect disabled' not in result.stdout:
            raise RuntimeError('WSPR switch probe failed: '+result.stdout[-10000:])
        data=json.loads(lines[0]);data['wallSeconds']=round(time.monotonic()-start,3)
        if not data['wsprStillActive'] or data['mode']!='FT8' or data['tx'] or data['lowCpu'] != a.low_cpu:
            raise RuntimeError('unexpected state '+repr(data))
        if a.return_wspr and data.get('returnWsprRows',0)<1:
            raise RuntimeError('WSPR produced no rows after returning: '+repr(data))
        print(json.dumps(data,sort_keys=True))
finally:
    qml.write_text(original)
