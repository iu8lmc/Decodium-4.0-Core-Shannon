#!/usr/bin/env python3
"""Exercise status indicators with simulated decoding; no radio or TX."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
main = root / 'build-baseline/qml/decodium/Main.qml'
status = root / 'build-baseline/qml/decodium/components/StatusBar.qml'
original_main, original_status = main.read_text(), status.read_text()
anchor = '    id: statusBarComponent\n'
if original_main.count('GlassApplicationWindow {') != 1 or original_status.count(anchor) != 1:
    raise RuntimeError('unexpected QML structure')
probe = '''
    Timer {interval: 12000; running: true; repeat: false; onTriggered: {
        statusBarComponent.decoding = true; decLed.testDecoding = true;
        ftThreadsLed.isActive = true; sample.start();
    }}
    Timer {id: sample; interval: 700; repeat: true; property int phase: 0; onTriggered: {
        console.log('[DEC-LED-LAB] '+JSON.stringify({low:bridge.lowCpuModeEnabled,
            decoding:statusBarComponent.decoding, led:decLed.ledOn,
            decPulse:pulseAnimation.running, ftPulse:ftPulseAnimation.running,
            decOpacity:decLed.opacity, ftOpacity:ftThreadsLed.opacity,
            tx:bridge.transmitting, tune:bridge.tuning}));
        if (phase === 0) bridge.lowCpuModeEnabled = false;
        else if (phase === 2) bridge.lowCpuModeEnabled = true;
        else if (phase === 4) Qt.quit();
        ++phase;
    }}
'''
try:
    with tempfile.TemporaryDirectory(prefix='decodium-dec-led-') as tmp:
        settings = Path(tmp) / 'Decodium/Decodium3.ini'
        settings.parent.mkdir(parents=True)
        settings.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n')
        status.write_text(original_status.replace(anchor, anchor + probe, 1))
        env = os.environ.copy()
        env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1',
                   DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd = [str(root/'build-baseline/decodium'), '--test-mode', '--rig-name',
               'DecLed'+secrets.token_hex(5), '--lab-no-cat', '--lab-no-monitor',
               '--lab-callsign', 'IU8LMC', '--lab-mode', 'FT8', '--lab-quit-ms', '30000']
        app = subprocess.run(cmd, env=env, text=True, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, timeout=40)
        states = [json.loads(line.split('[DEC-LED-LAB] ', 1)[1]) for line in app.stdout.splitlines()
                  if '[DEC-LED-LAB] ' in line]
        if app.returncode or len(states) != 5 or '[LAB] CAT auto-connect disabled' not in app.stdout:
            raise RuntimeError(app.stdout[-8000:])
        for state in states:
            if not state['decoding'] or not state['led'] or state['tx'] or state['tune']:
                raise RuntimeError('invalid test state '+repr(states))
            if state['low']:
                if state['decPulse'] or state['ftPulse'] or state['decOpacity'] < .99 or state['ftOpacity'] < .99:
                    raise RuntimeError('LOW CPU indicators incorrect '+repr(states))
            elif not state['decPulse'] or not state['ftPulse']:
                raise RuntimeError('normal animations did not resume '+repr(states))
        print(json.dumps(states))
finally:
    main.write_text(original_main)
    status.write_text(original_status)
