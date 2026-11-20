#!/usr/bin/env python3
"""Capture displayed FT2 row timings in repeated weak-burst virtual tests."""
import json
import os
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2];out=root/'build-baseline/performance/results'
if '// Each check row is read once.' not in (root/'Detector/FtxLdpc.cpp').read_text():
 raise RuntimeError('The production build no longer contains the experimental row optimization; rebuild an explicit candidate before this A/B test')
env=dict(os.environ,QT_QUICK_BACKEND='software',DECODIUM_LAB_MODE='FT2',DECODIUM_LAB_FT2_REPEAT='1',DECODIUM_FT2_AP_MSG_LOG='1')
results=[]
for index,binary in enumerate(('decodium','decodium-ldpc-rows-reference','decodium','decodium-ldpc-rows-reference')):
 run=subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py',binary],cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=110)
 if run.returncode:raise RuntimeError(run.stdout)
 metrics=[json.loads(line.split('MEASUREMENT ',1)[1]) for line in run.stdout.splitlines() if line.startswith('MEASUREMENT ')]
 if len(metrics)!=1:raise RuntimeError(run.stdout)
 path=out/f'live-audio-{binary}-visible-ft2-ft2-repeat.log';data=path.read_text(errors='replace')
 states=[json.loads(line.split('[LIVE-PROBE] ',1)[1]) for line in data.splitlines() if '[LIVE-PROBE] ' in line]
 rows=sorted({(r['message'],r['freq'],r['time'],r['aptype'],r['dt']) for state in states for r in state['rows']})
 start=int(metrics[0]['playbackEpochMs']/1000)%86400
 def offset(row):
  t=row[2]
  if len(t)!=6 or not t.isdigit():raise RuntimeError('invalid UTC label: '+repr(row))
  sec=int(t[:2])*3600+int(t[2:4])*60+int(t[4:])
  return (sec-start+43200)%86400-43200
 wanted=[r for r in rows if r[0]=='CQ K1ABC FN42' and abs(int(r[1])-1500)<=2]
 if not any(-2<=offset(r)<=4 for r in wanted) or not any(5<=offset(r)<=12 for r in wanted):raise RuntimeError('bursts missing: '+repr(rows))
 if '[APMSG]' not in data:raise RuntimeError('no AP checks')
 metrics[0].update(rows=rows,apChecks=data.count('[APMSG]'));results.append(metrics[0]);shutil.copy2(path,out/f'ft2-history-ap-trace-{index}.log')
 print(json.dumps(metrics[0],sort_keys=True),flush=True)
(out/'ft2-history-ap-trace.json').write_text(json.dumps(results,indent=2)+'\n')
