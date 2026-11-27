#!/usr/bin/env python3
"""Isolate the FT4 sync scan change and compare continuous virtual input."""
import json
import os
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
source = root / 'Detector/FtxFt4Decoder.cpp'
original = source.read_text()
old = subprocess.check_output(['git', 'show', 'HEAD:Detector/FtxFt4Decoder.cpp'], cwd=root, text=True)
start_marker = '                  for (int idf = idfmin; idf <= idfmax; idf += idfstp)\n'
end_marker = '\n                }\n'
# The trailing brace is the first one at the outer indentation level.
def bounds(text):
    start = text.index(start_marker, text.index('void run_ft4_decode ('))
    end = text.index(end_marker, start) + 1
    return start, end
start, end = bounds(original)
old_start, old_end = bounds(old)
assert 'ftx_sync2d_scan_c' in original[start:end]
assert 'ftx_sync4d_c' in old[old_start:old_end]
outdir = root / 'build-baseline/performance/results'
reference = root / 'build-baseline/decodium-ft4-sync-scalar-reference'
try:
    source.write_text(original[:start] + old[old_start:old_end] + original[end:])
    with (outdir/'ft4-sync-isolated-build.log').open('w') as log:
        subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
    shutil.copy2(root/'build-baseline/decodium', reference)
finally:
    source.write_text(original)
    subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)

env = dict(os.environ, QT_QUICK_BACKEND='software', DECODIUM_LAB_MODE='FT4',
           DECODIUM_LAB_FIRST_WAV=str(root/'build-baseline/performance/wavs-ft2-ft4/ft4-weak16-seed1101.wav'))
results=[]
for index,binary in enumerate((reference.name,'decodium','decodium',reference.name)):
    run=subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py',binary],
                       cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=100)
    if run.returncode:raise RuntimeError(run.stdout)
    rows=[json.loads(line.split('MEASUREMENT ',1)[1]) for line in run.stdout.splitlines() if line.startswith('MEASUREMENT ')]
    if len(rows)!=1:raise RuntimeError(run.stdout)
    path=outdir/('live-audio-'+binary+'-visible-ft4-custom-first.log')
    data=path.read_text(errors='replace')
    states=[json.loads(line.split('[LIVE-PROBE] ',1)[1]) for line in data.splitlines() if '[LIVE-PROBE] ' in line]
    received=sorted({(r['message'],r['freq']) for state in states for r in state['rows']})
    if not any(m=='CQ K1ABC FN42' and abs(int(f)-1500)<=2 for m,f in received):
        raise RuntimeError('expected FT4 message absent')
    rows[0]['received']=received;results.append(rows[0])
    shutil.copy2(path,outdir/f'ft4-live-sync-isolated-ab-{index}.log')
    print(json.dumps(rows[0],sort_keys=True),flush=True)
(outdir/'ft4-live-sync-isolated-ab.json').write_text(json.dumps(results,indent=2)+'\n')
