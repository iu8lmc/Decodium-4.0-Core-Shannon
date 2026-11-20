#!/usr/bin/env python3
"""Inspect retained decoder threads after isolated mode changes on Linux."""
import json, os, secrets, subprocess, tempfile, sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
qml = root / 'build-baseline/qml/decodium/Main.qml'
original = qml.read_text()
marker = 'GlassApplicationWindow {'
if original.count(marker) != 1:
    raise RuntimeError('unexpected QML root')
probe = '''
 Timer {interval:12000; running:true; repeat:false; onTriggered:modeResourceStep.start()}
 Timer {id:modeResourceStep; interval:3000; repeat:true; property int step:0;
   property var modes:["FT8","FT4","FT2","WSPR","Q65-30","MSK144","FST4","FT8"];
   onTriggered:{
     if (step >= modes.length) {console.log("[RESOURCE-DONE]"); stop(); Qt.quit(); return}
     var requested=modes[step++]; bridge.mode=requested;
     console.log("[RESOURCE-STEP] "+JSON.stringify({requested:requested,actual:bridge.mode,low:bridge.lowCpuModeEnabled,tx:bridge.transmitting,monitor:bridge.monitoring}));
   }
 }
'''
focus = '--focus-ft' in sys.argv[1:]
decode = focus or '--decode' in sys.argv[1:]
if decode:
    wavs = [root/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav',
            root/'build-baseline/performance/wavs-ft2-ft4/ft4-0.wav',
            root/'build-baseline/performance/wavs-ft2-ft4/ft2-0.wav',
            root/'build-baseline/performance/wavs-wspr/150426_0918.wav',
            root/'tests/bridge_refactor_fixtures/ft8-9h1sr-1300.wav']
    if focus:
        wavs = wavs[:3] * 2
    if not all(p.is_file() for p in wavs):raise FileNotFoundError('decode fixtures are not available')
    probe = '''
 Timer {interval:12000;running:true;repeat:false;onTriggered:resourceDecode.start()}
 Connections {target:bridge;function onDecodingChanged(){if(bridge.decoding && resourceDecode.phase===1)resourceDecode.seen=true}}
 Timer {id:resourceDecode;interval:300;repeat:true;property int step:0;property int phase:0;property int settle:0;property bool seen:false;
   property var modes:__MODES__;property var wavs:__WAVS__;
   onTriggered:{
    if(step>=modes.length){console.log("[RESOURCE-DONE]");stop();Qt.quit();return}
    if(phase===0){bridge.mode=modes[step];seen=false;phase=1;bridge.openWavForDecode(wavs[step]);if(bridge.decoding)seen=true;return}
    if(phase===1){if(bridge.decoding)seen=true;else if(seen){phase=2;settle=0};return}
    if(++settle<4)return;
    var messages=[];for(var i=0;i<bridge.decodeList.length;i++){var row=bridge.decodeList[i];messages.push({message:String(row.message),freq:Number(row.freq)})}
    console.log("[RESOURCE-STEP] "+JSON.stringify({requested:modes[step],actual:bridge.mode,low:bridge.lowCpuModeEnabled,tx:bridge.transmitting,monitor:bridge.monitoring,rows:bridge.decodeList.length,messages:messages}));step++;phase=0;
   }
 }
'''.replace('__WAVS__',json.dumps([str(p) for p in wavs])).replace('__MODES__',json.dumps(['FT8','FT4','FT2']*2 if focus else ['FT8','FT4','FT2','WSPR','FT8']))
ticks = os.sysconf('SC_CLK_TCK')
def sample(pid):
    base = Path('/proc') / str(pid)
    status = (base/'status').read_text().splitlines()
    def kb(key):
        return next(int(line.split()[1]) for line in status if line.startswith(key+':'))
    tasks = []
    for task in (base/'task').iterdir():
        try:
            name = (task/'comm').read_text().strip()
            parts = (task/'stat').read_text().rsplit(')',1)[1].split()
            if 'Worker' in name or 'Decode' in name:
                tasks.append({'tid':int(task.name),'name':name,'cpuSeconds':(int(parts[11])+int(parts[12]))/ticks})
        except (FileNotFoundError, ProcessLookupError):
            pass
    return {'rssKiB':kb('VmRSS'),'virtualKiB':kb('VmSize'),'decoderThreads':tasks}

try:
    qml.write_text(original.replace(marker, marker+probe, 1))
    with tempfile.TemporaryDirectory(prefix='decodium-mode-resources-') as tmp:
        rig='Resource'+secrets.token_hex(5)
        ini=Path(tmp)/'Decodium'/'Decodium3.ini';ini.parent.mkdir(parents=True)
        ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n'%rig)
        env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','150000']
        proc=subprocess.Popen(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
        results=[]; log=[]; completed=False
        try:
            for line in proc.stdout:
                log.append(line)
                if '[RESOURCE-STEP] ' in line:
                    state=json.loads(line.split('[RESOURCE-STEP] ',1)[1]);state.update(sample(proc.pid));results.append(state)
                if '[RESOURCE-DONE]' in line:
                    completed=True;break
            rc=proc.wait(timeout=15)
            name='mode-resource-focus-ft-profile' if focus else ('mode-resource-decode-profile' if decode else 'mode-resource-profile')
            path=root/('build-baseline/performance/results/'+name+'.log');path.write_text(''.join(log))
            if rc or not completed or len(results)!=(6 if focus else (5 if decode else 8)) or any(not r['low'] or r['tx'] or r['monitor'] or r['requested']!=r['actual'] or (decode and not r['rows']) for r in results):
                raise RuntimeError('resource probe failed: '+repr(results))
            if focus:
                for r, frequency in zip(results, [1500,1000,1000]*2):
                    if not any('CQ K1ABC FN42' in m['message'] and abs(m['freq']-frequency)<=2 for m in r['messages']):
                        raise RuntimeError('expected decode missing: '+repr(r))
            out=root/('build-baseline/performance/results/'+name+'.json');out.write_text(json.dumps(results,indent=2)+'\n')
            print(json.dumps(results,sort_keys=True))
        finally:
            if proc.poll() is None:proc.terminate();proc.wait(timeout=10)
finally:
    qml.write_text(original)
