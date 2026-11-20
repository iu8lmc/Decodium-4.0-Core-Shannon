#!/usr/bin/env python3
"""Verify the clock timer stops while hidden and refreshes on return."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
qml = root / 'build-baseline/qml/decodium/Main.qml'
original = qml.read_text()
marker = 'GlassApplicationWindow {'
if original.count(marker) != 1:
    raise RuntimeError('unexpected QML root')
probe = '''
 Timer {interval:15000; running:true; repeat:false; onTriggered: {
   worldClock.showWorldClock=true; mainWindow.dxPeditionMode=false;
   clockCheck.visibleTimer=worldClock.visible && worldClockRefreshTimer.running;
   mainWindow.dxPeditionMode=true;
   clockCheck.hiddenTimer=!worldClock.visible && !worldClockRefreshTimer.running;
   worldClock.seconds=-1;
   clockCheck.start();
 }}
 Timer {id:clockCheck; interval:1500; running:false; repeat:false;
   property bool visibleTimer:false; property bool hiddenTimer:false;
   onTriggered: {
     var stayedHidden=worldClock.seconds===-1;
     mainWindow.dxPeditionMode=false;
     var immediate=worldClock.seconds>=0 && worldClock.seconds<60;
     var restarted=worldClock.visible && worldClockRefreshTimer.running;
     console.log('[CLOCK-VISIBILITY] '+JSON.stringify({visibleTimer:visibleTimer,
       hiddenTimer:hiddenTimer,stayedHidden:stayedHidden,immediate:immediate,
       restarted:restarted,tx:bridge.transmitting})); Qt.quit();
 }}
'''
try:
    with tempfile.TemporaryDirectory(prefix='decodium-clock-visibility-') as tmp:
        qml.write_text(original.replace(marker, marker + probe, 1))
        env = os.environ.copy()
        env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1',
                   DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd = [str((root/'build-baseline/decodium').resolve()), '--test-mode', '--rig-name',
               'ClockVisibility'+secrets.token_hex(5), '--lab-no-cat', '--lab-no-monitor',
               '--lab-callsign', 'IU8LMC', '--lab-mode', 'FT8', '--lab-quit-ms', '35000']
        result = subprocess.run(cmd, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=50)
        lines = [line.split('[CLOCK-VISIBILITY] ', 1)[1] for line in result.stdout.splitlines()
                 if '[CLOCK-VISIBILITY] ' in line]
        if result.returncode or len(lines) != 1:
            raise RuntimeError(result.stdout[-6000:])
        value = json.loads(lines[0])
        expected = {'visibleTimer': True, 'hiddenTimer': True, 'stayedHidden': True,
                    'immediate': True, 'restarted': True, 'tx': False}
        if value != expected:
            raise RuntimeError('unexpected state: '+str(value))
        print(json.dumps(value, sort_keys=True))
finally:
    qml.write_text(original)
