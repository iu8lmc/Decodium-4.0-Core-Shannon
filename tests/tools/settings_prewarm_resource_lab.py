#!/usr/bin/env python3
"""Compare optional settings prewarming in isolated LOW CPU app sessions.

Runs offline and changes only the build-tree QML, restored on every exit.
The prewarm timer is shortened equally in both sessions for the experiment.
"""
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
p.add_argument('--binary', type=Path, default=root/'build-baseline/decodium')
p.add_argument('--qml', type=Path, default=root/'build-baseline/qml/decodium/Main.qml')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
qml = a.qml.resolve(); original = qml.read_text(); marker = 'GlassApplicationWindow {'
if original.count(marker) != 1: raise RuntimeError('unexpected QML root')
old = 'id: firstUseWarmupTimer\n        interval: 30000'
if original.count(old) != 1: raise RuntimeError('unexpected warmup timer')
probe = '''
 Timer { interval: 26000; running: true; repeat: false; onTriggered: {
   console.log("[SETTINGS-LAB] BEFORE " + JSON.stringify({low:bridge.lowCpuModeEnabled,loaded:!!settingsDialogLoader.item, monitoring:bridge.monitoring}));
   settingsLabStart=Date.now(); mainWindow.openSettingsDialog(); settingsOpenWait.start();
 }}
 property double settingsLabStart: 0
 Timer { id: settingsOpenWait; interval: 50; repeat: true; property int count: 0; onTriggered: {
   count++;
   if(settingsDialogLoader.item && settingsDialogLoader.item.visible) {
     console.log("[SETTINGS-LAB] OPEN " + JSON.stringify({delayMs:Date.now()-settingsLabStart,tab:settingsDialogLoader.item.currentTab}));
     stop(); Qt.quit();
   } else if(count > 300) { console.log("[SETTINGS-LAB] OPEN-TIMEOUT"); stop(); Qt.quit(); }
 }}
'''

def rss(pid):
    for line in (Path('/proc')/str(pid)/'status').read_text().splitlines():
        if line.startswith('VmRSS:'): return int(line.split()[1])*1024
    raise RuntimeError('missing RSS')

def run(skip):
    with tempfile.TemporaryDirectory(prefix='decodium-settings-prewarm-') as tmp:
        rig = 'Warmup' + secrets.token_hex(5)
        ini = Path(tmp)/'Decodium'/'Decodium3.ini'; ini.parent.mkdir(parents=True)
        ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n'%rig)
        contents = original.replace(old, 'id: firstUseWarmupTimer\n        interval: 5000')
        if skip:
            target = '            if (!settingsDialogLoader.item) {'
            if contents.count(target) != 1: raise RuntimeError('unexpected settings warmup')
            contents = contents.replace(target, '            if (!settingsDialogLoader.item && !bridge.lowCpuModeEnabled) {',1)
        qml.write_text(contents.replace(marker,marker+probe,1))
        env = dict(os.environ, DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd = [str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor',
               '--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','65000']
        start = time.monotonic()
        proc = subprocess.Popen(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
        try:
            result = {'skipPrewarm':skip}; lines = []
            for line in proc.stdout:
                lines.append(line)
                if '[SETTINGS-LAB] BEFORE ' in line:
                    result.update(json.loads(line.split('[SETTINGS-LAB] BEFORE ',1)[1])); result['rssBeforeOpen'] = rss(proc.pid)
                if '[SETTINGS-LAB] OPEN ' in line:
                    result.update(json.loads(line.split('[SETTINGS-LAB] OPEN ',1)[1])); result['rssAfterOpen'] = rss(proc.pid)
            code = proc.wait(timeout=10); result['exit']=code; result['wallSeconds']=round(time.monotonic()-start,3)
            if code or '[LAB] CAT auto-connect disabled' not in ''.join(lines) or not result.get('low') or result.get('monitoring') or result.get('loaded') != (not skip) or 'delayMs' not in result:
                raise RuntimeError(json.dumps(result)+'\n'+''.join(lines)[-8000:])
            return result
        finally:
            if proc.poll() is None:
                proc.terminate()
                try: proc.wait(timeout=5)
                except subprocess.TimeoutExpired: proc.kill(); proc.wait()
try:
    result = [run(False), run(True)]
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))
finally:
    qml.write_text(original)
