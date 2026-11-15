#!/usr/bin/env python3
"""Compare two FT2 bursts separated by two slots using private virtual audio."""
import json
import os
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2];out=root/'build-baseline/performance/results'
if '// Each check row is read once.' not in (root/'Detector/FtxLdpc.cpp').read_text():
 raise RuntimeError('The production build no longer contains the experimental row optimization; rebuild an explicit candidate before this A/B test')
strong=root/'build-baseline/performance/wavs-ft2-ft4/ft2-history-strong.wav'
subprocess.run([str(root/'build-baseline/tests/ft2_make_test_wav'),'--message','CQ K1ABC FN42',
                '--freq','1500','--gain','0.05',str(strong)],cwd=root,check=True,stdout=subprocess.DEVNULL)
env=dict(os.environ,QT_QUICK_BACKEND='software',DECODIUM_LAB_MODE='FT2',DECODIUM_LAB_FT2_REPEAT='1',DECODIUM_FT2_AP_MSG_LOG='1')
results=[]
for i,name in enumerate(('decodium-ldpc-rows-reference','decodium','decodium','decodium-ldpc-rows-reference')):
 run=subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py',name],cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=110)
 if run.returncode:raise RuntimeError(run.stdout)
 measurement=[json.loads(x.split('MEASUREMENT ',1)[1]) for x in run.stdout.splitlines() if x.startswith('MEASUREMENT ')]
 if len(measurement)!=1:raise RuntimeError(run.stdout)
 path=out/f'live-audio-{name}-visible-ft2-ft2-repeat.log';data=path.read_text(errors='replace')
 states=[json.loads(x.split('[LIVE-PROBE] ',1)[1]) for x in data.splitlines() if '[LIVE-PROBE] ' in x]
 rows=sorted({(r['message'],r['freq'],r['time'],r['aptype']) for state in states for r in state['rows']})
 wanted=[r for r in rows if r[0]=='CQ K1ABC FN42' and abs(int(r[1])-1500)<=2]
 times=sorted({r[2] for r in wanted})
 def second_of_day(value):
  if len(value)!=6 or not value.isdigit():raise RuntimeError('unexpected FT2 UTC label: '+value)
  return int(value[:2])*3600+int(value[2:4])*60+int(value[4:])
 start=int(measurement[0]['playbackEpochMs']/1000)%86400
 offsets=[(second_of_day(value)-start+43200)%86400-43200 for value in times]
 if not any(-2<=offset<=4 for offset in offsets) or not any(5<=offset<=12 for offset in offsets):
  raise RuntimeError('both FT2 bursts not visible at expected times: '+repr(wanted))
 if '[APMSG]' not in data:raise RuntimeError('no historical-message checks observed')
 measurement[0].update(rows=rows,apChecks=data.count('[APMSG]'),distinctTimes=times,timeOffsets=offsets)
 results.append(measurement[0]);shutil.copy2(path,out/f'ft2-history-live-ab-{i}.log')
 print(json.dumps(measurement[0],sort_keys=True),flush=True)
(out/'ft2-history-live-ab.json').write_text(json.dumps(results,indent=2)+'\n')
