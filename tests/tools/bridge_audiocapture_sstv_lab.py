#!/usr/bin/env python3
"""Check dedicated SSTV capture consumes synthetic virtual audio."""
import json,os,secrets,subprocess,tempfile,time,atexit
from pathlib import Path
import numpy as np
import sounddevice as sd
r=Path(__file__).resolve().parents[2]
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
with tempfile.TemporaryDirectory(prefix='decodium-capture-sstv-lab-') as t:
 root=Path(t);ready=root/'ready.json';log=root/'rx.log';rig='Capture'+secrets.token_hex(5)
 env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=t,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software')
 args=[str((r/'build/decodium').resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 16ch','--lab-monitor-ms','1500','--lab-sstv-rx-ms','3000','--lab-sstv-rx-ready-file',str(ready),'--lab-quit-ms','20000']
 device=next(i for i,d in enumerate(sd.query_devices()) if d['name']=='BlackHole 2ch' and d['max_output_channels']>=2)
 with log.open('w') as out:
  process=subprocess.Popen(['sandbox-exec','-p',policy]+args,cwd=r,env=env,stdout=out,stderr=subprocess.STDOUT)
  try:
   time.sleep(4);samples=np.arange(8*48000)/48000;wave=(0.2*np.sin(2*np.pi*1500*samples)).astype(np.float32);sd.play(np.column_stack((wave,wave)),samplerate=48000,device=device,blocking=True)
   code=process.wait(timeout=30)
  finally:
   sd.stop()
   if process.poll() is None:process.terminate();process.wait(timeout=10)
  output=log.read_text(errors='replace')
  if code or not ready.exists() or '[CAPTURE-AB] startAudioCapture invoked' not in output or '[LAB] CAT auto-connect disabled' not in output:raise RuntimeError('capture did not start: '+output[-5000:])
  data=json.loads(ready.read_text())
  lines=[line.split('[LAB][SSTV] RX diagnostics ',1)[1] for line in output.splitlines() if '[LAB][SSTV] RX diagnostics ' in line]
  diagnostics=[json.loads(line) for line in lines]; strongest=max((float(d.get('signalRms',0)) for d in diagnostics),default=0)
  print(json.dumps({'method_invoked':True,'worker':bool(data.get('workerRunning')),'chunks_positive':int(data.get('chunksProcessed',0))>0,'active':bool(data.get('active')),'source':data.get('sourceDevice'),'signal_present':strongest>0.005,'peak_reported_rms':strongest},sort_keys=True))
