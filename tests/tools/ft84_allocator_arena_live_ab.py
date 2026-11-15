#!/usr/bin/env python3
"""Explore glibc arena limits in virtual FT8 and FT4 reception."""
import json
import os
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2];out=root/'build-baseline/performance/results'
cases=[('FT8','default','wavs-ft8-reference/181201_180245.wav','CQ RU NT2A FN20|1980'),
       ('FT8','2','wavs-ft8-reference/181201_180245.wav','CQ RU NT2A FN20|1980'),
       ('FT4','2','wavs-ft2-ft4/busy-ft4-20261115.wav','CQ K1ABC FN42|400'),
       ('FT4','default','wavs-ft2-ft4/busy-ft4-20261115.wav','CQ K1ABC FN42|400')]
results=[]
for index,(mode,setting,wav,expected) in enumerate(cases):
 env=dict(os.environ,QT_QUICK_BACKEND='software',DECODIUM_LAB_MODE=mode,DECODIUM_LAB_FIRST_WAV=str(root/'build-baseline/performance'/wav),DECODIUM_LAB_EXPECT_FIRST=expected)
 if setting=='default':env.pop('MALLOC_ARENA_MAX',None)
 else:env['MALLOC_ARENA_MAX']=setting
 run=subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py'],cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=100)
 if run.returncode:raise RuntimeError(run.stdout)
 metrics=[json.loads(line.split('MEASUREMENT ',1)[1]) for line in run.stdout.splitlines() if line.startswith('MEASUREMENT ')]
 if len(metrics)!=1 or metrics[0]['peakRssKiB'] is None:raise RuntimeError(run.stdout)
 suffix='visible-ft4-custom-first' if mode=='FT4' else 'visible-custom-first'
 path=out/f'live-audio-decodium-{suffix}.log';data=path.read_text(errors='replace');shutil.copy2(path,out/f'ft84-allocator-live-{index}.log')
 states=[json.loads(line.split('[LIVE-PROBE] ',1)[1]) for line in data.splitlines() if '[LIVE-PROBE] ' in line]
 rows=sorted({(r['message'],r['freq']) for state in states for r in state['rows']})
 result={'mode':mode,'setting':setting,**metrics[0],'rows':rows};results.append(result);print(json.dumps(result,sort_keys=True),flush=True)
(out/'ft84-allocator-live-ab.json').write_text(json.dumps(results,indent=2)+'\n')
