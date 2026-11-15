#!/usr/bin/env python3
"""Measure recovery after switching away from an active FST4W WAV decode."""
import argparse
import json
import os
from pathlib import Path
import resource
import secrets
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--binary', type=Path, required=True)
p.add_argument('--qml', type=Path, default=root/'build-baseline/qml/decodium/Main.qml')
p.add_argument('--fst4w', type=Path, required=True)
p.add_argument('--log', type=Path, required=True)
p.add_argument('--low-cpu', action='store_true')
p.add_argument('--next-mode', choices=('FT8','FT4','FT2'), default='FT8')
p.add_argument('--next-wav', type=Path, help='WAV to decode after the switch')
p.add_argument('--next-frequency', type=int, default=1500)
p.add_argument('--next-expected', default='CQ K1ABC FN42')
p.add_argument('--switch-delay-ms', type=int, default=1500,
               help='delay after FST4W starts before switching modes')
p.add_argument('--return-fst4w', action='store_true', help='decode FST4W again after the next mode recovers')
p.add_argument('--capture-next-complete', action='store_true',
               help='wait for the next decoder to finish and log all of its rows before returning')
a = p.parse_args()
qml = a.qml.resolve()
original = qml.read_text()
marker = 'GlassApplicationWindow {'
if original.count(marker) != 1 or not a.fst4w.is_file():
    raise RuntimeError('unexpected QML root or missing FST4W WAV')
next_wav = a.next_wav or root/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'
if not next_wav.is_file() or not 0 <= a.next_frequency <= 5000 or not 1 <= a.switch_delay_ms <= 30000:
    raise ValueError('invalid next-mode WAV, audio frequency or switch delay')
probe = '''
 Timer {interval:12000; running:true; repeat:false; onTriggered: {
   bridge.rxFrequency=1500; bridge.openWavForDecode(__FST4W__); awaitFst4w.start();
 }}
 Timer {id:awaitFst4w; interval:50; running:false; repeat:true; property int ticks:0;
   onTriggered: {
     if(bridge.decoding) { stop(); switchToFt8.start(); }
     else if(++ticks>400) {console.log("[FST4W-SWITCH] no-fst4w-start");stop();Qt.quit();}
 }}
 Timer {id:switchToFt8; interval:__SWITCH_DELAY__; running:false; repeat:false; onTriggered: {
   waitFt8.fst4wStillActive=bridge.decoding;
   var begin=Date.now(); bridge.mode=__NEXT_MODE__; waitFt8.syncMs=Date.now()-begin;
   bridge.rxFrequency=__NEXT_FREQUENCY__;
   waitFt8.startMs=Date.now(); bridge.openWavForDecode(__FT8__);
   waitFt8.openMs=Date.now()-waitFt8.startMs; waitFt8.start();
 }}
 Timer {id:waitFt8; interval:250; running:false; repeat:true; property int ticks:0;
   property bool fst4wStillActive:false; property double startMs:0; property int syncMs:0;
   property int openMs:0; property int firstPollMs:-1;
   onTriggered: {
     if(firstPollMs<0) firstPollMs=Date.now()-startMs;
     var rows=bridge.decodeList;
     for(var i=0;i<rows.length;i++) if(String(rows[i].message).indexOf(__NEXT_EXPECTED__)>=0) {
       var data={fst4wStillActive:fst4wStillActive,
         mode:bridge.mode,lowCpu:bridge.lowCpuModeEnabled,tx:bridge.transmitting,
         syncMs:syncMs,openMs:openMs,firstPollMs:firstPollMs,
         startEpochMs:startMs,recoveryMs:Date.now()-startMs};
       stop(); __AFTER_FT8__ return;
     }
     if(++ticks>320) {console.log("[FST4W-SWITCH] timeout");stop();Qt.quit();}
 }}
 Timer {id:waitReturn; interval:250; running:false; repeat:true;
   property var result:({}); property bool seen:false; property int ticks:0; property int quiet:0;
   onTriggered: {
     if(bridge.decoding) seen=true;
     if(seen && !bridge.decoding && ++quiet>=4) {
       result.returnFst4wRows=bridge.decodeList.length;
       result.returnExpected=false;
       for(var i=0;i<bridge.decodeList.length;i++)
         if(String(bridge.decodeList[i].message).indexOf("K1ABC FN42 33")>=0) result.returnExpected=true;
       console.log("[FST4W-SWITCH] "+JSON.stringify(result));stop();Qt.quit();return;
     }
     if(++ticks>360) {console.log("[FST4W-SWITCH] return-timeout");stop();Qt.quit();}
 }}
 Timer {id:waitNextComplete; interval:250; running:false; repeat:true;
   property var result:({}); property int ticks:0; property int quiet:0;
   onTriggered: {
     if(!bridge.decoding) quiet++; else quiet=0;
     if(quiet>=4) {
       console.log('[FST4W-NEXT-ROWS] '+JSON.stringify(bridge.decodeList));
       var data=result; stop(); __AFTER_NEXT_COMPLETE__ return;
     }
     if(++ticks>320) {console.log('[FST4W-SWITCH] next-complete-timeout');stop();Qt.quit();}
   }
 }
'''
action = ('waitReturn.result=data; bridge.mode="FST4W-120"; bridge.rxFrequency=1500; bridge.openWavForDecode(__FST4W__); waitReturn.start();'
          if a.return_fst4w else 'console.log("[FST4W-SWITCH] "+JSON.stringify(data));Qt.quit();')
first_action = 'waitNextComplete.result=data; waitNextComplete.start();' if a.capture_next_complete else action
probe = (probe.replace('__AFTER_FT8__',first_action).replace('__AFTER_NEXT_COMPLETE__',action).replace('__FST4W__',json.dumps(str(a.fst4w.resolve())))
         .replace('__FT8__',json.dumps(str(next_wav.resolve())))
         .replace('__SWITCH_DELAY__',str(a.switch_delay_ms))
         .replace('__NEXT_MODE__',json.dumps(a.next_mode)).replace('__NEXT_FREQUENCY__',str(a.next_frequency))
         .replace('__NEXT_EXPECTED__',json.dumps(a.next_expected)))
try:
    with tempfile.TemporaryDirectory(prefix='decodium-fst4w-switch-') as tmp:
        rig='Fst4wSwitch'+secrets.token_hex(5)
        settings=Path(tmp)/'Decodium'/'Decodium3.ini'
        settings.parent.mkdir(parents=True)
        low=str(a.low_cpu).lower()
        settings.write_text('[General]\nLowCpuMode=%s\nlowCpuModeEnabled=%s\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=%s\nlowCpuModeEnabled=%s\n' % (low,low,rig,low,low))
        qml.write_text(original.replace(marker,marker+probe,1))
        env=os.environ.copy()
        env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', DECODIUM_FORCE_STANDALONE_UDP='1', DECODIUM_MODE_PROFILE='1')
        cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor',
             '--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FST4W-120','--lab-quit-ms','120000']
        start=time.monotonic()
        usage_before=resource.getrusage(resource.RUSAGE_CHILDREN)
        result=subprocess.run(cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=145)
        usage_after=resource.getrusage(resource.RUSAGE_CHILDREN)
        a.log.write_text(result.stdout)
        lines=[line.split('[FST4W-SWITCH] ',1)[1] for line in result.stdout.splitlines() if '[FST4W-SWITCH] ' in line]
        if result.returncode or len(lines)!=1 or lines[0] in ('timeout','return-timeout','no-fst4w-start','next-complete-timeout') or '[LAB] CAT auto-connect disabled' not in result.stdout:
            raise RuntimeError('FST4W switch probe failed: '+result.stdout[-10000:])
        data=json.loads(lines[0]);data['wallSeconds']=round(time.monotonic()-start,3)
        data['processCpuSeconds']=round((usage_after.ru_utime-usage_before.ru_utime)
                                        +(usage_after.ru_stime-usage_before.ru_stime),3)
        data['processPeakRssKiB']=usage_after.ru_maxrss
        if not data['fst4wStillActive'] or data['mode']!=a.next_mode or data['tx'] or data['lowCpu'] != a.low_cpu:
            raise RuntimeError('unexpected state '+repr(data))
        if a.return_fst4w and not data.get('returnExpected'):
            raise RuntimeError('FST4W did not recover the expected message: '+repr(data))
        print(json.dumps(data,sort_keys=True))
finally:
    qml.write_text(original)
