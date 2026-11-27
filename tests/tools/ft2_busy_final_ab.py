#!/usr/bin/env python3
"""Compare FT2 virtual reception using the saved reference and current app."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
out = root/'build-baseline/performance/results'
expected = {'CQ K1ABC FN42','CQ W2XYZ FN42','CQ N3ABC FN42','CQ K4ABC FN42',
            'CQ W5ABC FN42','CQ N6ABC FN42','CQ K7ABC FN42','CQ W8ABC FN42'}
results = []
mode = sys.argv[1].upper() if len(sys.argv)>1 else 'FT2'
if mode not in ('FT2','FT4','FT8'):
    raise ValueError('mode must be FT2, FT4 or FT8')
tag = mode.lower()
if mode == 'FT8':
    expected = {'W3RGA KM6JD RR73','CQ RU NT2A FN20','N8HRZ W7CD 73',
                'CQ RU K1BAA DM03','CQ UW5EJX/MM','CQ RU W7BOB DN14','K1JT 9A2JK RR73'}
fixture = (root/'build-baseline/performance/wavs-ft8-reference/181201_180245.wav' if mode == 'FT8'
           else root/f'build-baseline/performance/wavs-ft2-ft4/busy-{tag}-20261115.wav')
env = dict(os.environ, QT_QUICK_BACKEND='software', DECODIUM_LAB_MODE=mode,
           DECODIUM_LAB_FIRST_WAV=str(fixture),
           DECODIUM_LAB_EXPECT_FIRST='W3RGA KM6JD RR73|1879' if mode == 'FT8' else 'CQ K1ABC FN42|400')
for index, binary in enumerate(('decodium-before','decodium','decodium','decodium-before')):
    run = subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py',binary],
                         cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
    if run.returncode:
        raise RuntimeError(run.stdout)
    measurements = [json.loads(s.split('MEASUREMENT ',1)[1]) for s in run.stdout.splitlines() if s.startswith('MEASUREMENT ')]
    if len(measurements)!=1:
        raise RuntimeError(run.stdout)
    path=out/f'live-audio-{binary}-visible{("-"+tag) if mode != "FT8" else ""}-custom-first.log'
    data=path.read_text(errors='replace')
    shutil.copy2(path,out/f'{tag}-busy-final-{index}.log')
    states=[json.loads(x.split('[LIVE-PROBE] ',1)[1]) for x in data.splitlines() if '[LIVE-PROBE] ' in x]
    rows=sorted({(r['message'],r['freq']) for s in states for r in s['rows']})
    result={**measurements[0], 'rows':rows, 'missingExpected':sorted(expected-{r[0] for r in rows})}
    results.append(result)
    print(json.dumps(result,sort_keys=True),flush=True)
    (out/f'{tag}-busy-final-ab.json').write_text(json.dumps(results,indent=2)+'\n')
if any(r['missingExpected'] for r in results):
    raise SystemExit('missing signals; see saved results')
