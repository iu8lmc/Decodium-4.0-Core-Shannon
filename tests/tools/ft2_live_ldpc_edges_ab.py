#!/usr/bin/env python3
"""Build a reference differing only in LDPC reverse-edge lookup and compare FT2."""
import json
import os
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2]
source=root/'Detector/FtxLdpc.cpp'
original=source.read_text()
start=original.index('// The parity-check graph is fixed.')
end=original.index('\nstruct Ldpc17491Tables',start)
oldloop='''              for (int edge = 0; edge < tables.ncw; ++edge)
                {
                  if (mn_at (tables, edge, bit) == check + 1)
                    {
                      value -= tov[static_cast<size_t> (bit)][static_cast<size_t> (edge)];
                      break;
                    }
                }'''
newloop='''              int const edge = kLdpc17491ReverseEdges[static_cast<size_t> (
                  row + kLdpc17491MaxRowWeight * check)];
              value -= tov[static_cast<size_t> (bit)][static_cast<size_t> (edge)];'''
if original.count(newloop)!=1:raise RuntimeError('unexpected LDPC source')
reference=(original[:start]+original[end:]).replace(newloop,oldloop,1)
outdir=root/'build-baseline/performance/results'; binary='decodium-ldpc-edges-reference'
try:
 source.write_text(reference)
 with (outdir/'ft2-ldpc-edges-build.log').open('w') as log:
  subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build-baseline/decodium',root/'build-baseline'/binary)
finally:
 source.write_text(original)
 subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)

env=dict(os.environ,QT_QUICK_BACKEND='software',DECODIUM_LAB_MODE='FT2',
         DECODIUM_LAB_FIRST_WAV=str(root/'build-baseline/performance/wavs-ft2-ft4/ft2-weak16-seed1101.wav'))
results=[]
for index,name in enumerate((binary,'decodium','decodium',binary)):
 run=subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py',name],cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=100)
 if run.returncode:raise RuntimeError(run.stdout)
 rows=[json.loads(x.split('MEASUREMENT ',1)[1]) for x in run.stdout.splitlines() if x.startswith('MEASUREMENT ')]
 if len(rows)!=1:raise RuntimeError(run.stdout)
 path=outdir/('live-audio-'+name+'-visible-ft2-custom-first.log')
 data=path.read_text(errors='replace')
 states=[json.loads(x.split('[LIVE-PROBE] ',1)[1]) for x in data.splitlines() if '[LIVE-PROBE] ' in x]
 received=sorted({(r['message'],r['freq']) for state in states for r in state['rows']})
 if not any(m=='CQ K1ABC FN42' and abs(int(f)-1500)<=2 for m,f in received):raise RuntimeError('expected message absent')
 rows[0]['received']=received;results.append(rows[0]);shutil.copy2(path,outdir/f'ft2-live-ldpc-edges-ab-{index}.log')
 print(json.dumps(rows[0],sort_keys=True),flush=True)
(outdir/'ft2-live-ldpc-edges-ab.json').write_text(json.dumps(results,indent=2)+'\n')
