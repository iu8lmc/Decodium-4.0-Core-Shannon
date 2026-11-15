#!/usr/bin/env python3
"""Trace FT2 sync/async rows for a synthetic repeated-burst experiment."""
import json
import os
from pathlib import Path
import shutil
import subprocess
from ldpc_minsum_fixture import candidate_minsum

root=Path(__file__).resolve().parents[2];out=root/'build-baseline/performance/results'
ldpc=root/'Detector/FtxLdpc.cpp';worker=root/'Detector/FT2DecodeWorker.cpp'
original_ldpc=ldpc.read_bytes();original_worker=worker.read_bytes()
text=original_worker.decode().replace('\r\n','\n')
text=text.replace('#include <QDebug>','#include <QDebug>\n#include <QDateTime>',1)
async_marker='  Q_EMIT asyncDecodeReady (build_rows (utcPrefix,'
sync_marker='  Q_EMIT decodeReady (request.serial, build_rows (utcPrefix,'
if text.count(async_marker)!=1 or text.count(sync_marker)!=1:raise RuntimeError('unexpected worker layout')
template='''  for (int labI=0; labI<nout; ++labI) {
    qInfo().noquote() << QStringLiteral("[FT2-WINDOW-TRACE] path=PATH now=%1 utc=%2 end=END dt=%3 freq=%4 nap=%5 qual=%6 msg=%7")
      .arg(QDateTime::currentMSecsSinceEpoch()).arg(request.nutc).arg(static_cast<double>(dts[labI]),0,'f',6)
      .arg(static_cast<double>(freqs[labI]),0,'f',3).arg(naps[labI]).arg(static_cast<double>(quals[labI]),0,'f',6)
      .arg(QString::fromLatin1(decodeds+labI*kDecodedChars,kDecodedChars).trimmed());
  }
'''
async_probe=template.replace('PATH','async').replace('end=END','end=%8').replace('.trimmed());','.trimmed()).arg(request.audioEnd);')
sync_probe=template.replace('PATH','sync').replace('end=END','end=unknown')
text=text.replace(async_marker,async_probe+async_marker,1).replace(sync_marker,sync_probe+sync_marker,1)
try:
 worker.write_text(text)
 for label,candidate in [('reference',False),('candidate',True)]:
  ldpc.write_text(candidate_minsum(original_ldpc.decode(),root) if candidate else original_ldpc.decode())
  with (out/f'ft2-window-trace-{label}-build.log').open('w') as log:
   subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
  shutil.copy2(root/'build-baseline/decodium',root/f'build-baseline/decodium-ft2-window-{label}')
finally:
 ldpc.write_bytes(original_ldpc);worker.write_bytes(original_worker)
 subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
env=dict(os.environ,QT_QUICK_BACKEND='software',DECODIUM_LAB_MODE='FT2',DECODIUM_LAB_FT2_REPEAT='1',DECODIUM_FT2_AP_MSG_LOG='1')
results=[]
for index,label in enumerate(('candidate','reference','reference','candidate')):
 binary='decodium-ft2-window-'+label
 run=subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py',binary],cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=110)
 if run.returncode:raise RuntimeError(run.stdout)
 metrics=[json.loads(line.split('MEASUREMENT ',1)[1]) for line in run.stdout.splitlines() if line.startswith('MEASUREMENT ')]
 if len(metrics)!=1:raise RuntimeError(run.stdout)
 path=out/f'live-audio-{binary}-visible-ft2-ft2-repeat.log';data=path.read_text(errors='replace')
 traces=[line for line in data.splitlines() if '[FT2-WINDOW-TRACE]' in line]
 states=[json.loads(line.split('[LIVE-PROBE] ',1)[1]) for line in data.splitlines() if '[LIVE-PROBE] ' in line]
 rows=sorted({(r['message'],r['freq'],r['time'],r['aptype'],r['dt']) for state in states for r in state['rows']})
 start=int(metrics[0]['playbackEpochMs']/1000)%86400
 def offset(row):
  value=row[2]
  if len(value)!=6 or not value.isdigit():raise RuntimeError('invalid UTC label: '+repr(row))
  sec=int(value[:2])*3600+int(value[2:4])*60+int(value[4:])
  return (sec-start+43200)%86400-43200
 wanted=[r for r in rows if r[0].rstrip(' ?')=='CQ K1ABC FN42' and abs(int(r[1])-1500)<=2]
 if not traces or not any(-2<=offset(r)<=4 for r in wanted) or not any(5<=offset(r)<=12 for r in wanted):
  raise RuntimeError('missing traces or FT2 burst: '+repr(rows))
 shutil.copy2(path,out/f'ft2-window-trace-{index}.log');metrics[0].update(rows=rows,traces=traces);results.append(metrics[0])
 print(label,'rows',json.dumps(rows),'traces',json.dumps(traces),flush=True)
(out/'ft2-window-trace.json').write_text(json.dumps(results,indent=2)+'\n')
