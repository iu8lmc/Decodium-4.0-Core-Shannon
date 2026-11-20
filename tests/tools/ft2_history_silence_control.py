#!/usr/bin/env python3
"""Observe FT2 history checks after a strong burst followed by silence."""
import json
import os
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2];out=root/'build-baseline/performance/results'
env=dict(os.environ,QT_QUICK_BACKEND='software',DECODIUM_LAB_MODE='FT2',DECODIUM_LAB_FT2_REPEAT='1',DECODIUM_LAB_FT2_OMIT_WEAK='1',DECODIUM_FT2_AP_MSG_LOG='1')
results=[]
for index,binary in enumerate(('decodium-ldpc-rows-reference','decodium')):
 run=subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py',binary],cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=110)
 if run.returncode:raise RuntimeError(run.stdout)
 metrics=[json.loads(line.split('MEASUREMENT ',1)[1]) for line in run.stdout.splitlines() if line.startswith('MEASUREMENT ')]
 if len(metrics)!=1:raise RuntimeError(run.stdout)
 path=out/f'live-audio-{binary}-visible-ft2-ft2-repeat-silent-repeat.log'
 data=path.read_text(errors='replace')
 states=[json.loads(line.split('[LIVE-PROBE] ',1)[1]) for line in data.splitlines() if '[LIVE-PROBE] ' in line]
 rows=sorted({(r['message'],r['freq'],r['time'],r['aptype'],r['dt']) for state in states for r in state['rows']})
 if not any(r[0]=='CQ K1ABC FN42' and abs(int(r[1])-1500)<=2 for r in rows):raise RuntimeError('strong burst missing')
 ap_count=data.count('[APMSG]')
 if ap_count==0:raise RuntimeError('no history checks observed')
 metrics[0].update(rows=rows,apChecks=ap_count);results.append(metrics[0]);shutil.copy2(path,out/f'ft2-history-silence-control-{index}.log')
 print(json.dumps(metrics[0],sort_keys=True),flush=True)
(out/'ft2-history-silence-control.json').write_text(json.dumps(results,indent=2)+'\n')
