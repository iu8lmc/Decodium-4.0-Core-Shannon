#!/usr/bin/env python3
"""Compare Decometer gauge pixels and paint counts with deterministic telemetry."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
local_qmlscene = Path.home() / '.local/opt/decodium-qt/6.11.0/gcc_64/bin/qmlscene'
QMLSCENE = os.environ.get('QMLSCENE') or (
    str(local_qmlscene) if local_qmlscene.exists() else shutil.which('qmlscene'))
if not QMLSCENE:
    raise RuntimeError('Set QMLSCENE to a Qt 6 qmlscene executable')
source = ROOT / 'qml/decodium/components/DecometerWindow.qml'
current = source.read_text()
old_tick = '''        gauge.requestPaint()\n    }\n\n    function setRangeAuto'''
start = current.index('        // The gauge is expensive to paint')
end = current.index('\n    }\n\n    function setRangeAuto', start)
reference = current[:start] + '        gauge.requestPaint()' + current[end:]
assert old_tick in reference
running = '''        running: decometerWindow.nativeHostWindow\n                 ? decometerWindow.nativeHostWindow.visible\n                 : decometerWindow.visible'''
assert running in current
harness = '''import QtQuick
import QtQuick.Window
Window {
    width: 900; height: 420; visible: true
    property QtObject bridge: QtObject {
        property bool catConnected: true
        property bool transmitting: false
        property bool tuning: false
        property real rigPowerWatts: 0
        property real rigSwr: 1
        property real rigAlc: 0
        property bool rigAlcValid: false
        property real frequency: 14074000
        property string catRigName: "test"
        property var themeManager: null
        property var hamlibCat: null
        property var amplifier: null
        function getSetting(key, fallback) { return true }
        function setSetting(key, value) {}
    }
    Meter { id: meter; anchors.fill: parent }
    property int stage: 0
    function sample() {
        meter.tick(0.2)
        meter.gaugeForTest.grabToImage(function(result) {
            result.saveToFile("IMAGE_DIR/" + stage + ".png")
            console.log("RESULT", stage, meter.testPaints)
            stage++
            if (stage === 4) { Qt.quit(); return }
            if (stage === 1) { idleTicks.start(); return }
            if (stage === 2) { bridge.transmitting=true; bridge.rigPowerWatts=25; bridge.rigSwr=2; bridge.rigAlcValid=true; bridge.rigAlc=55 }
            if (stage === 3) { meter.setRange(2); meter.tick(0.2) }
            delay.restart()
        })
    }
    Timer {
        id: idleTicks; interval: 50; repeat: true
        property int count: 0
        onTriggered: {
            meter.tick(0.2)
            if (++count === 15) { stop(); delay.restart() }
        }
    }
    Timer { id: delay; interval: 250; onTriggered: sample() }
    Component.onCompleted: delay.start()
}
'''
with tempfile.TemporaryDirectory(prefix='decometer-gauge-') as tmp:
    base = Path(tmp)
    counts = []
    images = []
    for name, text in [('reference', reference), ('current', current)]:
        d = base / name
        d.mkdir()
        text = text.replace(running, '        running: false')
        text = text.replace('    id: decometerWindow\n', '    id: decometerWindow\n    property alias gaugeForTest: gauge\n    property int testPaints: 0\n', 1)
        text = text.replace('                onPaint: {\n', '                onPaint: {\n                    decometerWindow.testPaints++\n', 1)
        (d / 'Meter.qml').write_text(text)
        (d / 'main.qml').write_text(harness.replace('IMAGE_DIR', str(d)))
        env = dict(os.environ, QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
        result = subprocess.run([str(QMLSCENE), str(d / 'main.qml')], env=env, capture_output=True, text=True, timeout=20)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        if any(error in result.stderr for error in ('ReferenceError:', 'TypeError:', 'SyntaxError:')):
            raise RuntimeError(result.stderr)
        lines = [line for line in result.stderr.splitlines() if 'RESULT' in line]
        if len(lines) != 4:
            raise RuntimeError(result.stdout + result.stderr)
        counts.append([int(line.split()[-1]) for line in lines])
        images.append([hashlib.sha256((d / f'{i}.png').read_bytes()).hexdigest() for i in range(4)])
        print(name, 'paint counts by stage:', counts[-1], flush=True)
    if images[0] != images[1]:
        raise RuntimeError('gauge images differ: ' + repr(images))
    if counts[1][1] >= counts[0][1] or counts[1][-1] <= counts[1][1]:
        raise RuntimeError('idle paint reduction or dynamic updates not observed')
    print('PASS: 4 matching gauge images; idle redraws reduced; telemetry and scale repaint')
