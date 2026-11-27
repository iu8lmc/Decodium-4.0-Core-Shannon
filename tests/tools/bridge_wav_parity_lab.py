#!/usr/bin/env python3
"""Compare file decodes through two local application builds in LOW CPU mode."""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import time

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--before', type=Path, required=True)
p.add_argument('--after', type=Path, required=True)
p.add_argument('--qml', type=Path, default=root / 'build-baseline/qml/decodium/Main.qml')
p.add_argument('--wav', type=Path, nargs='+', required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--logs-dir', type=Path, help='save application logs for inspecting decoder settings')
p.add_argument('--no-ghost-filter', action='store_true', help='disable the optional ghost filter for diagnosis')
p.add_argument('--grid', default='JM75', help='local locator used in the isolated test (default: JM75)')
p.add_argument('--rx-frequency', type=int, help='set the isolated receiver audio frequency in Hz')
p.add_argument('--expect-after', help='require this message in the current build')
p.add_argument('--mode', choices=('FT8','FT4','FT2','FST4-15','FST4W-120','WSPR'), default='FT8')
a = p.parse_args()
qml = a.qml.resolve()
original = qml.read_text()
marker = 'GlassApplicationWindow {'
if original.count(marker) != 1:
    raise RuntimeError('unexpected QML root')
probe = '''
 Timer {interval:12000; running:true; repeat:false; onTriggered: {
   resultProbe.started=Date.now(); bridge.openWavForDecode(__WAV__);
 }}
 Connections {target:bridge; function onDecodingChanged() {
   if (bridge.decoding) resultProbe.seen=true;
   else if (resultProbe.seen) resultProbe.start();
 }}
 Timer {id:resultProbe; interval:800; running:false; repeat:false;
   property bool seen:false; property double started:0;
   onTriggered: {
     var rows=bridge.decodeList; var results=[];
     for(var i=0;i<rows.length;i++) results.push({message:String(rows[i].message),freq:String(rows[i].freq),
       db:String(rows[i].db),dt:String(rows[i].dt),dxCallsign:String(rows[i].dxCallsign),
       dxGrid:String(rows[i].dxGrid),dxDistance:String(rows[i].dxDistance)});
     console.log("[WAV-PARITY] "+JSON.stringify({seen:seen,lowCpu:bridge.lowCpuModeEnabled,
       tx:bridge.transmitting,elapsedMs:Date.now()-started,rows:results})); Qt.quit();
 }}
'''
if a.no_ghost_filter:
    probe = probe.replace('resultProbe.started=Date.now();', 'bridge.setHideGhostDecodes(false); resultProbe.started=Date.now();')
if a.rx_frequency is not None:
    if not 0 <= a.rx_frequency <= 5000:
        raise ValueError('RX frequency must be between 0 and 5000 Hz')
    probe = probe.replace('resultProbe.started=Date.now();',
                          'bridge.rxFrequency=%d; resultProbe.started=Date.now();' % a.rx_frequency)
def run(binary, wav, name):
    with tempfile.TemporaryDirectory(prefix='decodium-wav-parity-') as tmp:
        rig = 'Parity' + secrets.token_hex(5)
        settings = Path(tmp)/'Decodium'/'Decodium3.ini'
        settings.parent.mkdir(parents=True)
        settings.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n' % rig)
        env = os.environ.copy()
        env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', DECODIUM_FORCE_STANDALONE_UDP='1')
        qml.write_text(original.replace(marker, marker + probe.replace('__WAV__', json.dumps(str(wav.resolve()))), 1))
        cmd = [str(binary.resolve()), '--test-mode', '--rig-name',rig, '--lab-no-cat','--lab-no-monitor',
               '--lab-callsign','IU8LMC','--lab-grid',a.grid,'--lab-mode',a.mode,'--lab-quit-ms','100000']
        start=time.monotonic()
        proc=subprocess.run(cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
        if a.logs_dir:
            a.logs_dir.mkdir(parents=True, exist_ok=True)
            (a.logs_dir/(wav.stem+'-'+name+'.log')).write_text(proc.stdout)
        lines=[s.split('[WAV-PARITY] ',1)[1] for s in proc.stdout.splitlines() if '[WAV-PARITY] ' in s]
        if proc.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:
            raise RuntimeError(proc.stdout[-12000:])
        result=json.loads(lines[0]); result['wallSeconds']=round(time.monotonic()-start,3)
        if not result['seen'] or not result['lowCpu'] or result['tx']:
            raise RuntimeError('unexpected decode state '+repr(result))
        return result

def keys(rows):
    return Counter((str(row['freq']).strip(), ' '.join(row['message'].upper().split())) for row in rows)
try:
    report=[]
    for i,wav in enumerate(a.wav):
        if not wav.is_file(): raise FileNotFoundError(wav)
        values={}
        for name,binary in ([('before',a.before),('after',a.after)] if i%2==0 else [('after',a.after),('before',a.before)]):
            values[name]=run(binary,wav,name)
        missing=keys(values['before']['rows'])-keys(values['after']['rows'])
        added=keys(values['after']['rows'])-keys(values['before']['rows'])
        row={'wav':str(wav.resolve()),'sha256':hashlib.sha256(wav.read_bytes()).hexdigest(),
             'localGrid':a.grid,'mode':a.mode,'ghostFilterEnabled':not a.no_ghost_filter,**values,
             'missing':[{'freq':f,'message':m,'count':n} for (f,m),n in sorted(missing.items())],
             'added':[{'freq':f,'message':m,'count':n} for (f,m),n in sorted(added.items())]}
        report.append(row)
        print(json.dumps({'wav':wav.name,'before':len(values['before']['rows']),
                          'after':len(values['after']['rows']),'missing':len(missing),'added':len(added)}),flush=True)
        if not values['before']['rows']:
            print('No baseline rows for '+wav.name+'; this file cannot demonstrate preservation of decoded messages.',
                  file=sys.stderr, flush=True)
        if a.expect_after and not any(a.expect_after in item['message'] for item in values['after']['rows']):
            raise RuntimeError('expected message absent in current build: '+a.expect_after)
    a.output.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
    if any(row['missing'] for row in report): raise SystemExit(1)
finally:
    qml.write_text(original)
