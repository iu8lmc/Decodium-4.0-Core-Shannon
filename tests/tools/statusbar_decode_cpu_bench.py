#!/usr/bin/env python3
"""Compare simulated decode indicators with and without their animations."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[2]
status = root/'build-baseline/qml/decodium/components/StatusBar.qml'
original = status.read_text()
anchor = '    id: statusBarComponent\n'
condition = ' && !(bridge && bridge.lowCpuModeEnabled)'
if original.count(anchor) != 1 or original.count(condition) != 2:
    raise RuntimeError('unexpected status bar structure')
probe = '''
    Timer { interval: 12000; running: true; repeat: false; onTriggered: {
        statusBarComponent.decoding = true; decLed.testDecoding = true;
        ftThreadsLed.isActive = true;
    }}
    Timer { interval: 1000; running: true; repeat: true; onTriggered: {
        console.log('[DEC-BENCH] '+JSON.stringify({low:bridge.lowCpuModeEnabled,
            decoding:statusBarComponent.decoding, led:decLed.ledOn,
            visible:statusBarComponent.visible, dec:pulseAnimation.running,
            ft:ftPulseAnimation.running, tx:bridge.transmitting, tune:bridge.tuning}));
    }}
'''
results = []
app = None
try:
    for animated in (True, False, False, True):
        variant = original.replace(condition, '') if animated else original
        status.write_text(variant.replace(anchor, anchor + probe, 1))
        with tempfile.TemporaryDirectory(prefix='decodium-dec-bench-') as tmp:
            settings = Path(tmp)/'Decodium/Decodium3.ini'
            settings.parent.mkdir(parents=True)
            settings.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n')
            env = os.environ.copy()
            env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1',
                       DECODIUM_FORCE_STANDALONE_UDP='1')
            cmd = [str(root/'build-baseline/decodium'), '--test-mode', '--rig-name',
                   'DecBench'+secrets.token_hex(5), '--lab-no-cat', '--lab-no-monitor',
                   '--lab-callsign', 'IU8LMC', '--lab-mode', 'FT8', '--lab-quit-ms', '35000']
            path = Path(tmp)/'app.log'
            with path.open('w') as log:
                app = subprocess.Popen(cmd, env=env, stdout=log, stderr=subprocess.STDOUT)
                deadline = time.monotonic()+25
                while time.monotonic() < deadline:
                    data = path.read_text(errors='replace')
                    if any('"decoding":true' in row for row in data.splitlines() if '[DEC-BENCH] ' in row):
                        break
                    if app.poll() is not None:
                        raise RuntimeError('application exited: '+data[-3000:])
                    time.sleep(.1)
                else:
                    raise RuntimeError('indicators did not activate')
                def ticks():
                    fields = Path(f'/proc/{app.pid}/stat').read_text().rsplit(')', 1)[1].split()
                    return int(fields[11])+int(fields[12])
                start = ticks(); offset = path.stat().st_size; wall_start = time.monotonic()
                time.sleep(9)
                cpu = (ticks()-start)/os.sysconf('SC_CLK_TCK'); wall = time.monotonic()-wall_start
                states = [json.loads(row.split('[DEC-BENCH] ', 1)[1]) for row in
                          path.read_text(errors='replace')[offset:].splitlines() if '[DEC-BENCH] ' in row]
                if len(states) < 6 or not all(s['low'] and s['decoding'] and s['led'] and s['visible']
                    and s['dec'] == animated and s['ft'] == animated and not s['tx'] and not s['tune'] for s in states):
                    raise RuntimeError('unexpected state '+repr(states))
                results.append({'animated':animated, 'cpuSeconds':cpu, 'wallSeconds':wall, 'samples':len(states)})
                print(json.dumps(results[-1]), flush=True)
                app.terminate(); app.wait(timeout=10); app = None
    output = root/'build-baseline/performance/results/statusbar-decode-cpu.json'
    output.write_text(json.dumps(results, indent=2)+'\n')
finally:
    if app is not None and app.poll() is None:
        app.terminate()
        try: app.wait(timeout=5)
        except subprocess.TimeoutExpired: app.kill(); app.wait()
    status.write_text(original)
