#!/usr/bin/env python3
"""Check real QML world-clock access in an isolated local app on macOS."""
import json
import os
import secrets
import shutil
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
rig = 'WorldClockProbe' + secrets.token_hex(5)
qml = root / 'build/qml/decodium/Main.qml'
original = qml.read_text()
marker = 'GlassApplicationWindow {'
assert original.count(marker) == 1
probe = '''
 Timer { interval:9000; running:true; repeat:false; onTriggered: {
   var utc = bridge.worldClockSnapshot("UTC");
   var invalid = bridge.worldClockSnapshot("Invalid/Zone");
   var cities = bridge.worldClockCityOptions("rome", 10);
   var rome = false;
   for (var i=0; i<cities.length; ++i) if (cities[i].zoneId === "Europe/Rome") rome=true;
   console.log("[CLOCK-APP] "+JSON.stringify({utc:utc.timeZoneId==="UTC" && utc.offsetSeconds===0,
      invalid:invalid.timeZoneId==="UTC" && invalid.offsetSeconds===0,
      hour:utc.hours>=0 && utc.hours<24, rome:rome, tx:bridge.transmitting}));
   Qt.quit();
 } }
'''
try:
    with tempfile.TemporaryDirectory(prefix='decodium-clock-probe-') as temp:
        qml.write_text(original.replace(marker, marker + probe, 1))
        env = os.environ.copy()
        env.update(DECODIUM_LAB_SETTINGS_DIR=temp, DECODIUM_DISABLE_QML_CACHE='1',
                   QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
        policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
        cmd = ['sandbox-exec','-p',policy,str(root/'build/decodium'),'--test-mode',
               '--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','22000']
        proc = subprocess.run(cmd, cwd=root, env=env, text=True, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, timeout=40)
        lines = [x.split('[CLOCK-APP] ',1)[1] for x in proc.stdout.splitlines() if '[CLOCK-APP] ' in x]
        if proc.returncode or len(lines) != 1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:
            raise RuntimeError(proc.stdout[-6000:])
        result = json.loads(lines[0])
        assert result == {'utc':True,'invalid':True,'hour':True,'rome':True,'tx':False}, result
        print(json.dumps(result, sort_keys=True))
finally:
    qml.write_text(original)
    for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',
                   Path.home()/'.qttest/Library/Caches/IU8LMC'):
        target = parent/('Decodium - '+rig+' - test')
        if target.is_dir():
            shutil.rmtree(target)
