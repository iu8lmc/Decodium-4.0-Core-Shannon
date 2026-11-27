#!/usr/bin/env python3
"""Exercise application shutdown during an isolated FST4W WAV decode."""
import argparse
import json
import os
import re
from pathlib import Path
import secrets
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--binary', type=Path, default=root / 'build-baseline/decodium')
p.add_argument('--qml', type=Path, default=root / 'build-baseline/qml/decodium/Main.qml')
p.add_argument('--wav', type=Path, required=True)
a = p.parse_args()
qml = a.qml.resolve()
original = qml.read_text()
marker = 'GlassApplicationWindow {'
if original.count(marker) != 1 or not a.wav.is_file():
    raise RuntimeError('unexpected QML root or missing WAV')
probe = '''
 Timer {interval:12000; running:true; repeat:false; onTriggered: {
   bridge.rxFrequency=1500; bridge.openWavForDecode(__WAV__);
 }}
 Connections {target:bridge; function onDecodingChanged() {
   if (bridge.decoding && !shutdownProbe.started) {
     shutdownProbe.started=true; shutdownProbe.start();
     console.log("[FST4W-SHUTDOWN] decode-started");
   }
 }}
 Timer {id:shutdownProbe; interval:1500; repeat:false; property bool started:false;
   onTriggered: { console.log("[FST4W-SHUTDOWN] quitting " + Date.now()); Qt.quit(); }
 }
'''.replace('__WAV__', json.dumps(str(a.wav.resolve())))
try:
    with tempfile.TemporaryDirectory(prefix='decodium-fst4w-shutdown-') as tmp:
        rig = 'Shutdown' + secrets.token_hex(5)
        settings = Path(tmp)/'Decodium'/'Decodium3.ini'
        settings.parent.mkdir(parents=True)
        settings.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n' % rig)
        env = os.environ.copy()
        env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', DECODIUM_FORCE_STANDALONE_UDP='1')
        qml.write_text(original.replace(marker, marker + probe, 1))
        cmd = [str(a.binary.resolve()), '--test-mode', '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor',
               '--lab-callsign', 'IU8LMC', '--lab-grid', 'JM75', '--lab-mode', 'FST4W-120', '--lab-quit-ms', '65000']
        start = time.monotonic()
        proc = subprocess.run(cmd, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
        elapsed = time.monotonic()-start
        quit_marker = re.search(r'\[FST4W-SHUTDOWN\] quitting (\d+)', proc.stdout)
        shutdown_seconds = time.time() - int(quit_marker.group(1)) / 1000 if quit_marker else None
        bad = ('did not finish in', 'still running after terminate', 'destroying locked mutex', 'QThread: Destroyed while thread')
        if (proc.returncode or '[FST4W-SHUTDOWN] decode-started' not in proc.stdout or
            shutdown_seconds is None or any(s in proc.stdout for s in bad) or shutdown_seconds > 12):
            raise RuntimeError('elapsed %.2f exit %d\n%s' % (elapsed, proc.returncode, proc.stdout[-15000:]))
        print(json.dumps({'exit': proc.returncode, 'wallSeconds': round(elapsed, 3), 'shutdownSeconds': round(shutdown_seconds, 3), 'wav': str(a.wav.resolve())}))
finally:
    qml.write_text(original)
