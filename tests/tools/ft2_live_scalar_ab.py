#!/usr/bin/env python3
"""Compare LOW CPU FT2 virtual input using scalar and current local builds."""
import json
import os
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2]
outdir=root/'build-baseline/performance/results'
env=os.environ.copy()
env.update(QT_QUICK_BACKEND='software',DECODIUM_LAB_MODE='FT2',
           DECODIUM_LAB_FIRST_WAV=str(root/'build-baseline/performance/wavs-ft2-ft4/ft2-weak16-seed1101.wav'))
results=[]
for index,binary in enumerate(('decodium-ft2-scalar-reference','decodium','decodium','decodium-ft2-scalar-reference')):
    run=subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py',binary],
                       cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=100)
    if run.returncode:raise RuntimeError(run.stdout)
    measurements=[json.loads(line.split('MEASUREMENT ',1)[1]) for line in run.stdout.splitlines() if line.startswith('MEASUREMENT ')]
    if len(measurements)!=1:raise RuntimeError(run.stdout)
    path=outdir/('live-audio-'+binary+'-visible-ft2-custom-first.log')
    data=path.read_text(errors='replace')
    states=[json.loads(line.split('[LIVE-PROBE] ',1)[1]) for line in data.splitlines() if '[LIVE-PROBE] ' in line]
    received=sorted({(r['message'],r['freq']) for state in states for r in state['rows']})
    if not any(m=='CQ K1ABC FN42' and abs(int(f)-1500)<=2 for m,f in received):
        raise RuntimeError('expected FT2 message absent')
    measurement=measurements[0];measurement['received']=received;results.append(measurement)
    shutil.copy2(path,outdir/f'ft2-live-scalar-ab-{index}.log')
    print(json.dumps(measurement,sort_keys=True),flush=True)
(outdir/'ft2-live-scalar-ab.json').write_text(json.dumps(results,indent=2)+'\n')
