#!/usr/bin/env python3
"""Time old and cached CPU row coloring inside virtual FT8 reception."""
import json
import os
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
source = root/'src/ui/PanadapterItem.cpp'
original = source.read_text()
start_anchor = '    if (prepareCpuPixels && line) {\n'
end_anchor = '''    } else {
        m_waterfallRgbValid = false;
    }
    // Se palette/black/gain'''
if original.count(start_anchor) != 1 or original.count(end_anchor) != 1:
    raise RuntimeError('unexpected source structure')
start = '''    if (prepareCpuPixels && line) {
        timespec labStart {}; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &labStart);
        if (qEnvironmentVariableIsSet("DECODIUM_LAB_OLD_WF_ROW")) {
            for (int x = 0; x < w; ++x) {
                float pixFreq = viewStart + (float)x * viewRange / w;
                if (pixFreq < dataFreqMin || pixFreq > dataFreqMax) {
                    line[x] = wfBg;
                    continue;
                }
                int bin = (int)((pixFreq - dataFreqMin) / dataRange * nBins);
                bin = qBound(0, bin, nBins - 1);
                float rawNorm = static_cast<float>(intensityLine[bin]) / 255.0f;
                float adjusted = (rawNorm - blackThresh) / usableRange;
                if (adjusted <= 0.f) { line[x] = wfBg; continue; }
                float pct = std::pow(qBound(0.f, adjusted, 1.f), gamma);
                line[x] = wfColor(pct);
            }
        } else {
'''
end = '''        }
        timespec labEnd {}; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &labEnd);
        qInfo().noquote() << "[WF-ROW-TIME] ns="
            << ((labEnd.tv_sec-labStart.tv_sec)*1000000000LL + labEnd.tv_nsec-labStart.tv_nsec)
            << "width=" << w << "palette=" << m_paletteIndex;
    } else {
        m_waterfallRgbValid = false;
    }
    // Se palette/black/gain'''
try:
    source.write_text(original.replace('#include <algorithm>', '#include <time.h>\n#include <algorithm>', 1)
                      .replace(start_anchor, start, 1).replace(end_anchor, end, 1))
    with (root/'build-baseline/performance/waterfall-row-timing-build.log').open('w') as log:
        subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],
                       cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)
    results = []
    for index, old in enumerate((True, False, False, True)):
        env = os.environ.copy()
        env.update(QT_QUICK_BACKEND='software', DECODIUM_LAB_TWO_SLOTS='1')
        if old: env['DECODIUM_LAB_OLD_WF_ROW'] = '1'
        else: env.pop('DECODIUM_LAB_OLD_WF_ROW', None)
        app = subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py','decodium'],
                             cwd=root, env=env, text=True, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, timeout=120)
        if app.returncode: raise RuntimeError(app.stdout)
        log_path = root/'build-baseline/performance/results/live-audio-decodium-visible-two-slots.log'
        log = log_path.read_text(errors='replace')
        timings = [tuple(map(int, m.groups())) for m in re.finditer(
            r'\[WF-ROW-TIME\] ns=\s*(\d+) width=\s*(\d+) palette=\s*(\d+)', log)]
        if len(timings) < 30 or any(w <= 0 for _, w, _ in timings):
            raise RuntimeError('too few timed waterfall rows: '+str(len(timings)))
        result = {'variant':'old' if old else 'cached', 'rows':len(timings),
                  'sumCpuMs':sum(ns for ns,_,_ in timings)/1e6,
                  'meanCpuUs':sum(ns for ns,_,_ in timings)/len(timings)/1e3,
                  'widths':sorted({w for _,w,_ in timings}),
                  'palettes':sorted({p for _,_,p in timings})}
        results.append(result)
        (root/f'build-baseline/performance/results/waterfall-row-timing-{index}.log').write_text(log)
        print(json.dumps(result), flush=True)
    (root/'build-baseline/performance/results/waterfall-row-timing.json').write_text(json.dumps(results, indent=2)+'\n')
finally:
    source.write_text(original)
    subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'], cwd=root,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
