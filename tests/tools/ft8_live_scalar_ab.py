#!/usr/bin/env python3
"""Compare LOW CPU FT8 virtual input with scalar and current sync search."""
import json
import os
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2]
outdir=root/'build-baseline/performance/results'
env=os.environ.copy()
env.update(QT_QUICK_BACKEND='software',DECODIUM_LAB_MODE='FT8',DECODIUM_LAB_TWO_SLOTS='1')
results=[]
for index,binary in enumerate(('decodium-ft8-scalar-reference','decodium','decodium','decodium-ft8-scalar-reference')):
    run=subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py',binary],
                       cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
    if run.returncode:raise RuntimeError(run.stdout)
    values=[json.loads(line.split('MEASUREMENT ',1)[1]) for line in run.stdout.splitlines() if line.startswith('MEASUREMENT ')]
    if len(values)!=1:raise RuntimeError(run.stdout)
    path=outdir/('live-audio-'+binary+'-visible-two-slots.log')
    states=[json.loads(line.split('[LIVE-PROBE] ',1)[1]) for line in path.read_text(errors='replace').splitlines() if '[LIVE-PROBE] ' in line]
    received=sorted({(r['message'],r['freq']) for state in states for r in state['rows']})
    for message,hz in [('CQ K1ABC FN42',1500),('CQ 9H1SR JM75',1300)]:
        if not any(m==message and abs(int(f)-hz)<=2 for m,f in received):
            raise RuntimeError('expected FT8 message absent: '+message)
    measurement=values[0];measurement['received']=received;results.append(measurement)
    shutil.copy2(path,outdir/f'ft8-live-scalar-ab-{index}.log')
    print(json.dumps(measurement,sort_keys=True),flush=True)
(outdir/'ft8-live-scalar-ab.json').write_text(json.dumps(results,indent=2)+'\n')
