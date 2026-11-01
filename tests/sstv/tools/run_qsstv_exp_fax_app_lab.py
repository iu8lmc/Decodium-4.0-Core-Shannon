#!/usr/bin/env python3
"""Replay QSSTV-Experimental FAX480 output into Decodium over BlackHole."""
import ctypes as c
import ctypes.util
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import tempfile
import time

variant=int(sys.argv[1])
if variant not in (0,1): raise SystemExit('expected 0 or 1')
repo=Path.cwd()
qsstv=Path(os.environ.get('QSSTV_EXPERIMENTAL_BINARY',
    '/tmp/decodium-qsstv-experimental/QSSTV-Experimental.app/Contents/MacOS/qsstv-experimental')).resolve(strict=True)
expected_sha256=os.environ.get('QSSTV_EXPERIMENTAL_SHA256',
    'b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa')
if hashlib.sha256(qsstv.read_bytes()).hexdigest()!=expected_sha256:
    raise RuntimeError('QSSTV-Experimental binary does not match the expected SHA-256')
root=Path(tempfile.mkdtemp(prefix='decodium-qsstv-exp-app-'+str(variant)+'-'))
with (root/'reference.pgm').open('wb') as ref:
    ref.write(b'P5\n512 500\n255\n')
    ref.write(bytes(((((x//64)+(y//80)+variant*2)%4)*60+35) for y in range(500) for x in range(512)))
encodeEnv=os.environ.copy()
encodeEnv['QT_QPA_PLATFORM']=os.environ.get('QSSTV_QPA_PLATFORM','cocoa')
existing=os.environ.get('QSSTV_EXISTING_WAV')
if existing:
    # Diagnostic replay of an already generated file; this does not establish
    # that the originating QSSTV encode process exited successfully.
    shutil.copyfile(existing,root/'source.wav')
else:
    with (root/'qsstv-encode.log').open('wb') as log:
        subprocess.run([str(qsstv),'--encode',str(root/'reference.pgm'),'--mode','FAX480',
                        '--wav-out',str(root/'source.wav')],env=encodeEnv,stdout=log,stderr=subprocess.STDOUT,
                       check=True,timeout=240)
subprocess.run(['ffmpeg','-v','error','-i',str(root/'source.wav'),
                '-f','f32le','-ar','48000','-ac','2',str(root/'source.f32')],check=True)
env=os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=str(root/'settings'))
args=[str((repo/'build/decodium').resolve(strict=True)),'--test-mode','--rig-name','QSFAX'+secrets.token_hex(4),
      '--lab-no-cat','--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 2ch',
      '--lab-reapply-ms','500','--lab-monitor-ms','1800','--lab-sstv-rx-ms','3500',
      '--lab-sstv-fax480-rx','qsstv-500','--lab-sstv-rx-ready-file',str(root/'ready'),
      '--lab-sstv-rx-image-file',str(root/'received.png'),'--lab-quit-ms','160000']
class Info(c.Structure):
    _fields_=[('v',c.c_int),('name',c.c_char_p),('host',c.c_int),('inputs',c.c_int),('outputs',c.c_int),
              ('li',c.c_double),('lo',c.c_double),('hi',c.c_double),('ho',c.c_double),('rate',c.c_double)]
class Params(c.Structure):
    _fields_=[('device',c.c_int),('channels',c.c_int),('format',c.c_ulong),('latency',c.c_double),('hostinfo',c.c_void_p)]
proc=None
try:
    with (root/'rx.log').open('wb') as log:
        proc=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT)
    deadline=time.monotonic()+20
    while not (root/'ready').exists():
        if proc.poll() is not None or time.monotonic()>deadline: raise RuntimeError('RX not ready: '+str(root))
        time.sleep(.1)
    ready=json.loads((root/'ready').read_text())
    if ready.get('sourceDevice')!='BlackHole 2ch': raise RuntimeError('wrong RX device: '+str(ready.get('sourceDevice')))
    pa=c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
    pa.Pa_GetDeviceInfo.restype=c.POINTER(Info)
    pa.Pa_OpenStream.argtypes=[c.POINTER(c.c_void_p),c.c_void_p,c.POINTER(Params),c.c_double,c.c_ulong,c.c_ulong,c.c_void_p,c.c_void_p]
    pa.Pa_WriteStream.argtypes=[c.c_void_p,c.c_void_p,c.c_ulong]
    if pa.Pa_Initialize(): raise RuntimeError('PortAudio initialization failed')
    dev=next(i for i in range(pa.Pa_GetDeviceCount()) if pa.Pa_GetDeviceInfo(i).contents.name==b'BlackHole 2ch')
    params=Params(dev,2,1,.05,None);stream=c.c_void_p()
    if pa.Pa_OpenStream(c.byref(stream),None,c.byref(params),48000,1024,0,None,None): raise RuntimeError('cannot open output')
    if pa.Pa_StartStream(stream): raise RuntimeError('cannot start output')
    with (root/'source.f32').open('rb') as source:
        while True:
            chunk=source.read(8192*8)
            if not chunk: break
            data=c.create_string_buffer(chunk)
            if pa.Pa_WriteStream(stream,data,len(chunk)//8): raise RuntimeError('PortAudio playback failed')
    pa.Pa_StopStream(stream);pa.Pa_CloseStream(stream);pa.Pa_Terminate()
    if proc.wait(timeout=25): raise RuntimeError('RX exited with failure')
    log=(root/'rx.log').read_text(errors='replace')
    rows=[json.loads(line.split(' RX diagnostics ',1)[1]) for line in log.splitlines()
          if '[LAB][SSTV] RX diagnostics ' in line]
    if not rows or not rows[-1]['imageComplete'] or rows[-1]['processingFailures']:
        raise RuntimeError('RX not complete; artifacts '+str(root))
    subprocess.run([sys.executable,str(repo/'tests/sstv/tools/compare_sstv_lab_images.py'),
        str(root/'reference.pgm'),str(root/'received.png'),'--max-mae','8','--max-p95','10'],check=True,timeout=30)
    print('PASS QSSTV-Experimental variant',variant,'coverage',rows[-1]['imageCoverage'],
          'discontinuities',rows[-1]['discontinuities'],'artifacts',root,flush=True)
finally:
    if proc and proc.poll() is None:
        proc.kill();proc.wait()
