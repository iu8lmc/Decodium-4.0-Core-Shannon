#!/usr/bin/env python3
"""Alternate reference and optimized Decometer with an isolated application."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[2]
main = root / 'build-baseline/qml/decodium/Main.qml'
meter = root / 'build-baseline/qml/decodium/components/DecometerWindow.qml'
source = root / 'qml/decodium/components/DecometerWindow.qml'
original_main, original_meter = main.read_text(), meter.read_text()
current = source.read_text()
start = current.index('        // The gauge is expensive to paint')
end = current.index('\n    }\n\n    function setRangeAuto', start)
reference = current[:start] + '        gauge.requestPaint()' + current[end:]
probe = '''
 Timer { interval:7000; running:true; repeat:false; onTriggered: { mainWindow.openDecometerWindow(); meterWait.start() } }
 Timer { id:meterWait; interval:1000; repeat:true; property int attempts:0; onTriggered: {
   if (++attempts>10) { console.log("[METER-FAIL] did not open"); Qt.quit(); return }
   if (decometerFloatingWindow.visible && decometerFloatingLoader.item) {
     stop(); console.log("[METER-START] "+JSON.stringify({visible:decometerFloatingWindow.visible,
       low:bridge.lowCpuModeEnabled, tx:bridge.transmitting, cat:bridge.catConnected,
       rf:decometerFloatingLoader.item.rfActive, paints:decometerFloatingLoader.item.labPaints})); meterMeasure.start()
   }
 }}
 Timer { id:meterMeasure; interval:10000; repeat:false; onTriggered: {
   console.log("[METER-END] "+JSON.stringify({visible:decometerFloatingWindow.visible,
     low:bridge.lowCpuModeEnabled, tx:bridge.transmitting, cat:bridge.catConnected,
     rf:decometerFloatingLoader.item.rfActive, paints:decometerFloatingLoader.item.labPaints})); Qt.quit()
 }}
'''

def cpu(pid):
    fields = (Path('/proc') / str(pid) / 'stat').read_text().split(') ', 1)[1].split()
    return (int(fields[11]) + int(fields[12])) / os.sysconf('SC_CLK_TCK')

if original_main.count('GlassApplicationWindow {') != 1:
    raise RuntimeError('unexpected main QML structure')
if 'id: decometerWindow\n' not in current or current.count('                onPaint: {\n') != 1:
    raise RuntimeError('unexpected meter QML structure')
outdir = root / 'build-baseline/performance/results'
outdir.mkdir(parents=True, exist_ok=True)
results = []
try:
    main.write_text(original_main.replace('GlassApplicationWindow {', 'GlassApplicationWindow {' + probe, 1))
    for index, (name, text) in enumerate([('reference', reference), ('current', current), ('current', current), ('reference', reference)]):
        text = text.replace('    id: decometerWindow\n', '    id: decometerWindow\n    property int labPaints: 0\n', 1)
        text = text.replace('                onPaint: {\n', '                onPaint: {\n                    decometerWindow.labPaints++\n', 1)
        meter.write_text(text)
        with tempfile.TemporaryDirectory(prefix='decodium-meter-live-') as tmp:
            rig = 'Meter' + secrets.token_hex(5)
            ini = Path(tmp) / 'Decodium' / 'Decodium3.ini'
            ini.parent.mkdir(parents=True)
            ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n' % rig)
            env = dict(os.environ, QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
                       DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', DECODIUM_FORCE_STANDALONE_UDP='1')
            cmd = [str(root/'build-baseline/decodium'), '--test-mode', '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor',
                   '--lab-callsign', 'IU8LMC', '--lab-grid', 'JM75', '--lab-mode', 'FT8', '--lab-quit-ms', '40000']
            proc = subprocess.Popen(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
            lines = []
            first = last = first_state = last_state = None
            try:
                for line in proc.stdout:
                    lines.append(line)
                    if '[METER-START] ' in line:
                        first_state = json.loads(line.split('[METER-START] ', 1)[1]); first = (time.monotonic(), cpu(proc.pid))
                    elif '[METER-END] ' in line:
                        last_state = json.loads(line.split('[METER-END] ', 1)[1]); last = (time.monotonic(), cpu(proc.pid)); break
                rc = proc.wait(timeout=20)
            finally:
                if proc.poll() is None:
                    proc.terminate(); proc.wait(timeout=10)
                (outdir / ('decometer-live-cpu-%d.log' % index)).write_text(''.join(lines))
            if rc or not first or not last:
                raise RuntimeError('measurement did not complete: ' + ''.join(lines[-60:]))
            for state in (first_state, last_state):
                if not state['visible'] or not state['low'] or state['tx'] or state['cat'] or state['rf']:
                    raise RuntimeError('unexpected state: ' + repr(state))
            row = {'variant':name, 'wallSeconds':round(last[0]-first[0], 3),
                   'cpuSeconds':round(last[1]-first[1], 3), 'paintCount':last_state['paints']-first_state['paints'],
                   'initial':first_state, 'final':last_state}
            results.append(row)
            print(json.dumps(row, sort_keys=True), flush=True)
    (outdir/'decometer-live-cpu-ab.json').write_text(json.dumps(results, indent=2)+'\n')
finally:
    main.write_text(original_main)
    meter.write_text(original_meter)
