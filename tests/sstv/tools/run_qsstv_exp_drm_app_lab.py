#!/usr/bin/env python3
"""Replay a QSSTV-Experimental DRM WAV into Decodium over BlackHole."""
import ctypes as c
import ctypes.util
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import time

if len(sys.argv)!=3:
    raise SystemExit('expected QSSTV WAV and independently decoded JP2')
source=Path(sys.argv[1]).resolve(strict=True)
expected=Path(sys.argv[2]).resolve(strict=True)
root=Path(tempfile.mkdtemp(prefix='decodium-qsstv-drm-app-'))
repo=Path.cwd()
subprocess.run(['ffmpeg','-v','error','-i',str(source),
                '-f','f32le','-ar','48000','-ac','2',str(root/'source.f32')],check=True)
env=os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=str(root/'settings'))
profile=os.environ.get('DECODIUM_HAMDRM_LAB_PROFILE','hamdrm-E-2500-high-16-QAM-long')
duration_ms=int(os.environ.get('DECODIUM_HAMDRM_LAB_DURATION_MS','80000'))
if duration_ms<10000 or duration_ms>600000: raise SystemExit('invalid duration')
args=[str((repo/'build/decodium').resolve(strict=True)),'--test-mode','--rig-name','QSDRM'+secrets.token_hex(4),
      '--lab-no-cat','--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 2ch',
      '--lab-reapply-ms','500','--lab-monitor-ms','1800','--lab-sstv-rx-ms','3500',
      '--lab-hamdrm','--lab-hamdrm-profile',profile,'--lab-sstv-rx-ready-file',str(root/'ready'),
      '--lab-sstv-rx-image-file',str(root/'received.jp2'),'--lab-quit-ms',str(duration_ms)]
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
    if proc.wait(timeout=duration_ms/1000+15): raise RuntimeError('RX exited with failure')
    log=(root/'rx.log').read_text(errors='replace')
    if '[LAB][HAMDRM] RX started true "'+profile+'"' not in log:
        raise RuntimeError('HAMDRM RX did not start; artifacts '+str(root))
    received=root/'received.jp2'
    if not received.exists() or received.read_bytes()!=expected.read_bytes():
        raise RuntimeError('QSSTV image was not reconstructed byte-exactly; artifacts '+str(root))
    print('PASS full QSSTV DRM TX to Decodium RX, exact bytes',len(received.read_bytes()),'artifacts',root,flush=True)

finally:
    if proc and proc.poll() is None:
        proc.kill();proc.wait()
