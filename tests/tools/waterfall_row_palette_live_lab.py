#!/usr/bin/env python3
"""Compare real waterfall rows against the previous color formula on live virtual audio."""
import os
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
source = root/'src/ui/PanadapterItem.cpp'
qml = root/'build-baseline/qml/decodium/components/Waterfall.qml'
original_source, original_qml = source.read_text(), qml.read_text()
row_anchor = '            line[x] = m_waterfallRowColors[intensityLine[bin]];'
loop_anchor = '        for (int x = 0; x < w; ++x) {\n            float pixFreq = viewStart + (float)x * viewRange / w;'
end_anchor = '''        }
    } else {
        m_waterfallRgbValid = false;
    }
    // Se palette/black/gain'''
qml_anchor = 'Item {\n    id: waterfallPanel'
if any((original_source.count(row_anchor) != 1, original_source.count(loop_anchor) != 1,
        original_source.count(end_anchor) != 1, original_qml.count(qml_anchor) != 1)):
    raise RuntimeError('unexpected source structure')
check = '''
            if (qEnvironmentVariableIsSet("DECODIUM_LAB_WF_ROW_CHECK")) {
                float const rawNorm = static_cast<float>(intensityLine[bin]) / 255.0f;
                float const adjusted = (rawNorm - blackThresh) / usableRange;
                QRgb const expected = adjusted <= 0.f ? wfBg
                    : wfColor(std::pow(qBound(0.f, adjusted, 1.f), gamma));
                ++labPixels;
                if (line[x] != expected) ++labMismatch;
            }
'''
qml_probe = '''
    Timer {interval:1100;running:true;repeat:true;property int step:0;onTriggered:{
        waterfallDisplay.paletteIndex=step%11;
        waterfallDisplay.blackLevel=(step*17)%101;
        waterfallDisplay.colorGain=(step*29)%101;
        ++step;
    }}
'''
try:
    modified = original_source.replace(loop_anchor,
        '        int labPixels = 0; int labMismatch = 0;\n'+loop_anchor, 1)
    modified = modified.replace(row_anchor, row_anchor+check, 1)
    modified = modified.replace(end_anchor, '''        }
        if (qEnvironmentVariableIsSet("DECODIUM_LAB_WF_ROW_CHECK"))
            qInfo().noquote() << "[WF-ROW-CHECK] palette=" << m_paletteIndex
                << "black=" << m_blackLevel << "gain=" << m_colorGain
                << "pixels=" << labPixels << "mismatch=" << labMismatch;
    } else {
        m_waterfallRgbValid = false;
    }
    // Se palette/black/gain''', 1)
    source.write_text(modified)
    with (root/'build-baseline/performance/waterfall-row-check-build.log').open('w') as log:
        subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],
                       cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)
    qml.write_text(original_qml.replace(qml_anchor, qml_anchor+qml_probe, 1))
    env = os.environ.copy()
    env.update(QT_QUICK_BACKEND='software', DECODIUM_LAB_WF_ROW_CHECK='1',
               DECODIUM_LAB_TWO_SLOTS='1')
    result = subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py','decodium'],
                            cwd=root, env=env, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=120)
    if result.returncode:
        raise RuntimeError(result.stdout)
    print(result.stdout, flush=True)
    log = (root/'build-baseline/performance/results/live-audio-decodium-visible-two-slots.log').read_text(errors='replace')
    rows = [tuple(map(int, m.groups())) for m in re.finditer(
        r'\[WF-ROW-CHECK\] palette=\s*(\d+) black=\s*(\d+) gain=\s*(\d+) pixels=\s*(\d+) mismatch=\s*(\d+)', log)]
    if not rows or {r[0] for r in rows} != set(range(11)) or any(r[3] <= 0 or r[4] != 0 for r in rows):
        raise RuntimeError('missing palettes or differing pixels: '+repr(rows[-30:]))
    print('Actual rows:', len(rows), 'pixels:', sum(r[3] for r in rows), 'palettes:', sorted({r[0] for r in rows}), 'mismatches: 0')
finally:
    source.write_text(original_source)
    qml.write_text(original_qml)
    subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'], cwd=root,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
