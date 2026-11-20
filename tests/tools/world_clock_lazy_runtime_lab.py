#!/usr/bin/env python3
"""Exercise the lazy world-clock selector in a test-mode application."""
import argparse
import json
import os
import secrets
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--binary', type=Path, default=root / 'build-baseline/decodium')
parser.add_argument('--qml', type=Path, default=root / 'build-baseline/qml/decodium/Main.qml')
args = parser.parse_args()
qml = args.qml.resolve()
original = qml.read_text()
marker = 'GlassApplicationWindow {'
if original.count(marker) != 1:
    raise RuntimeError('unexpected QML root')
probe = '''
 Timer { interval:5000; running:true; repeat:false; onTriggered: {
   var initialEmpty = worldClock.citySearchResults.length === 0;
   var utc = bridge.worldClockSnapshot("UTC");
   timezoneSelectorMA.clicked(null);
   clockResult.initialEmpty = initialEmpty;
   clockResult.utc = utc.timeZoneId === "UTC" && utc.offsetSeconds === 0;
   clockResult.start();
 }}
 Timer { id:clockResult; interval:500; running:false; repeat:false;
   property bool initialEmpty:false; property bool utc:false;
   onTriggered: {
     var defaults = worldClock.citySearchResults.length > 0;
     var opened = citySearchPopup.opened;
     worldClock.citySearchText = "rome";
     worldClock.refreshCitySearch();
     var rome = false;
     for (var i=0; i<worldClock.citySearchResults.length; ++i)
       if (worldClock.citySearchResults[i].zoneId === "Europe/Rome") rome=true;
     console.log("[CLOCK-LAZY] "+JSON.stringify({initialEmpty:initialEmpty,utc:utc,
        opened:opened,defaults:defaults,rome:rome,tx:bridge.transmitting}));
     Qt.quit();
 }}
'''
try:
    with tempfile.TemporaryDirectory(prefix='decodium-clock-lazy-') as tmp:
        qml.write_text(original.replace(marker, marker + probe, 1))
        env = os.environ.copy()
        env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1',
                   DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd = [str(args.binary.resolve()), '--test-mode', '--rig-name',
               'ClockLazy' + secrets.token_hex(5), '--lab-no-cat', '--lab-no-monitor',
               '--lab-callsign', 'IU8LMC', '--lab-mode', 'FT8', '--lab-quit-ms', '35000']
        result = subprocess.run(cmd, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=50)
        lines = [line.split('[CLOCK-LAZY] ', 1)[1] for line in result.stdout.splitlines()
                 if '[CLOCK-LAZY] ' in line]
        if result.returncode or len(lines) != 1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
            raise RuntimeError(result.stdout[-10000:])
        value = json.loads(lines[0])
        expected = {'initialEmpty': True, 'utc': True, 'opened': True,
                    'defaults': True, 'rome': True, 'tx': False}
        if value != expected:
            raise RuntimeError('unexpected selector state: ' + str(value))
        print(json.dumps(value, sort_keys=True))
finally:
    qml.write_text(original)
