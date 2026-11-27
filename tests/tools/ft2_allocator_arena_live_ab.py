#!/usr/bin/env python3
"""Compare default and two glibc arenas with busy FT2 virtual input."""
import json
import os
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2];out=root/'build-baseline/performance/results';results=[]
expected={'CQ K1ABC FN42','CQ W2XYZ FN42','CQ N3ABC FN42','CQ K4ABC FN42','CQ W5ABC FN42','CQ N6ABC FN42','CQ K7ABC FN42','CQ W8ABC FN42'}
for i,setting in enumerate(('default','2','2','default')):
 env=dict(os.environ,QT_QUICK_BACKEND='software',DECODIUM_LAB_MODE='FT2',DECODIUM_LAB_FIRST_WAV=str(root/'build-baseline/performance/wavs-ft2-ft4/busy-ft2-20261115.wav'),DECODIUM_LAB_EXPECT_FIRST='CQ K1ABC FN42|400')
 if setting=='default':env.pop('MALLOC_ARENA_MAX',None)
 else:env['MALLOC_ARENA_MAX']=setting
 run=subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py'],cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=100)
 if run.returncode:raise RuntimeError(run.stdout)
 measures=[json.loads(s.split('MEASUREMENT ',1)[1]) for s in run.stdout.splitlines() if s.startswith('MEASUREMENT ')]
 if len(measures)!=1 or measures[0]['peakRssKiB'] is None:raise RuntimeError(run.stdout)
 path=out/'live-audio-decodium-visible-ft2-custom-first.log';data=path.read_text(errors='replace');shutil.copy2(path,out/f'ft2-allocator-live-{i}.log')
 states=[json.loads(x.split('[LIVE-PROBE] ',1)[1]) for x in data.splitlines() if '[LIVE-PROBE] ' in x]
 rows=sorted({(r['message'],r['freq']) for s in states for r in s['rows']});missing=sorted(expected-{x[0] for x in rows})
 result={'setting':setting,**measures[0],'rows':rows,'missingExpected':missing};results.append(result);print(json.dumps(result,sort_keys=True),flush=True)
(out/'ft2-allocator-live-ab.json').write_text(json.dumps(results,indent=2)+'\n')
if any(r['missingExpected'] for r in results):raise SystemExit('at least one run did not recover all expected signals; see results')
