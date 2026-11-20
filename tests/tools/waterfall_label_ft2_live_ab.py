#!/usr/bin/env python3
"""Compare actual FT2 decodes and waterfall labels with and without dedup."""
import json
import os
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2];component=root/'build-baseline/qml/decodium/components/Waterfall.qml';original=component.read_text()
start=original.index('    function updateSpectrumDecodeLabels(labels) {');end=original.index('\n    function setShowDecodeCallsigns(',start)
reference=original[:start]+'''    function updateSpectrumDecodeLabels(labels) {
        spectrumDecodeLabels = labels
    }
'''+original[end:]
out=root/'build-baseline/performance/results'
env=dict(os.environ,QT_QUICK_BACKEND='software',DECODIUM_LAB_MODE='FT2',
 DECODIUM_LAB_FIRST_WAV=str(root/'build-baseline/performance/wavs-ft2-ft4/busy-ft2-20261115.wav'),DECODIUM_LAB_EXPECT_FIRST='CQ K1ABC FN42|400')
expected={'K1ABC':400,'W2XYZ':700,'N3ABC':1000,'K4ABC':1300,'W5ABC':1600,'N6ABC':1900,'K7ABC':2200,'W8ABC':2500}
results=[]
try:
 for index,(label,text) in enumerate((('reference',reference),('current',original))):
  component.write_text(text)
  run=subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py'],cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=100)
  if run.returncode:raise RuntimeError(run.stdout)
  path=out/'live-audio-decodium-visible-ft2-custom-first.log';data=path.read_text(errors='replace')
  states=[json.loads(x.split('[LIVE-PROBE] ',1)[1]) for x in data.splitlines() if '[LIVE-PROBE] ' in x]
  rows=sorted({(r['message'],int(r['freq'])) for s in states for r in s['rows']})
  labels=sorted({(v['call'],int(v['freq'])) for s in states for v in s['labels']})
  for call,freq in expected.items():
   if not any(m==f'CQ {call} FN42' and abs(f-freq)<=2 for m,f in rows):raise RuntimeError('missing decode '+call+': '+repr(rows))
   if not any(c==call and abs(f-freq)<=2 for c,f in labels):raise RuntimeError('missing label '+call+': '+repr(labels))
  metrics=[json.loads(x.split('MEASUREMENT ',1)[1]) for x in run.stdout.splitlines() if x.startswith('MEASUREMENT ')]
  if len(metrics)!=1:raise RuntimeError(run.stdout)
  result={'variant':label,'rows':rows,'labels':labels,'measurement':metrics[0]};results.append(result)
  shutil.copy2(path,out/f'waterfall-label-ft2-live-{index}.log');print(json.dumps(result,sort_keys=True),flush=True)
 if {x[0] for x in results[0]['rows']}!={x[0] for x in results[1]['rows']}:raise RuntimeError('decode sets differ')
 (out/'waterfall-label-ft2-live-ab.json').write_text(json.dumps(results,indent=2)+'\n')
finally:component.write_text(original)
