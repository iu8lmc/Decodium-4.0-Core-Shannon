#!/usr/bin/env python3
"""Real-time Radiofax TX capture through BlackHole; macOS/PortAudio lab only."""
import ctypes as c
import ctypes.util
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import threading
import time

variant=sys.argv[1]
if variant not in ('a','b'): raise SystemExit('expected image variant a or b')
profile='576,120,1'
root=Path(tempfile.mkdtemp(prefix='decodium-radiofax-tx-'+variant+'-'))
binary=Path('build/decodium').resolve(strict=True)
fixture=Path(sys.argv[2] if len(sys.argv)>2 else
             'tests/sstv/fixtures/qsstv-image-'+variant+'.jpg').resolve(strict=True)
duration_ms=int(sys.argv[3]) if len(sys.argv)>3 else 118000
if duration_ms<10000 or duration_ms>600000: raise SystemExit('invalid duration')
env = os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
           DECODIUM_LAB_SETTINGS_DIR=str(root / 'settings'))

class Info(c.Structure):
    _fields_ = [('version',c.c_int),('name',c.c_char_p),('host',c.c_int),('inputs',c.c_int),
        ('outputs',c.c_int),('lowin',c.c_double),('lowout',c.c_double),
        ('highin',c.c_double),('highout',c.c_double),('rate',c.c_double)]
class Params(c.Structure):
    _fields_ = [('device',c.c_int),('channels',c.c_int),('format',c.c_ulong),
                ('latency',c.c_double),('hostinfo',c.c_void_p)]
pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p),c.POINTER(Params),c.c_void_p,
                             c.c_double,c.c_ulong,c.c_ulong,c.c_void_p,c.c_void_p]
pa.Pa_ReadStream.argtypes = [c.c_void_p,c.c_void_p,c.c_ulong]
if pa.Pa_Initialize() != 0: raise RuntimeError('PortAudio initialization failed')
device = next(i for i in range(pa.Pa_GetDeviceCount())
              if pa.Pa_GetDeviceInfo(i).contents.name == b'BlackHole 2ch')
params = Params(device,2,1,.05,None)
stream = c.c_void_p()
if pa.Pa_OpenStream(c.byref(stream),c.byref(params),None,48000,1024,0,None,None):
    raise RuntimeError('cannot open BlackHole input')
if pa.Pa_StartStream(stream): raise RuntimeError('cannot start BlackHole input')
stop = threading.Event()
errors = []
def capture():
    buf = (c.c_float * 2048)()
    with (root / 'capture.f32').open('wb') as out:
        while not stop.is_set():
            result = pa.Pa_ReadStream(stream,buf,1024)
            if result != 0:
                errors.append(result)
                break
            out.write(bytes(buf))
thread = threading.Thread(target=capture)
thread.start()
args = [str(binary),'--test-mode','--rig-name','DRMTX'+secrets.token_hex(4),
        '--lab-no-cat','--lab-no-monitor','--lab-audio-input','BlackHole 2ch',
        '--lab-audio-output','BlackHole 2ch','--lab-reapply-ms','500',
        '--lab-sstv-tx-ms','3000','--lab-sstv-mode','avt-24','--lab-sstv-fax-tx',profile,
        '--lab-sstv-image',str(fixture),
        '--lab-sstv-vox','--lab-quit-ms',str(duration_ms)]
proc = None
try:
    with (root/'tx.log').open('wb') as log:
        proc=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT)
        deadline=time.monotonic()+duration_ms/1000+15
        while proc.poll() is None:
            if time.monotonic()>deadline:
                proc.kill(); proc.wait(); raise RuntimeError('application timeout')
            if errors: proc.kill(); proc.wait(); raise RuntimeError('capture error '+str(errors))
            time.sleep(.5)
    if proc.returncode: raise RuntimeError('application exit '+str(proc.returncode))
finally:
    if proc is not None and proc.poll() is None:
        proc.kill()
        proc.wait()
    stop.set(); thread.join(timeout=5)
    pa.Pa_StopStream(stream); pa.Pa_CloseStream(stream); pa.Pa_Terminate()
if errors: raise RuntimeError('capture error '+str(errors))
log=(root/'tx.log').read_text(errors='replace')
if '[LAB][SSTV] TX requested accepted= true' not in log:
    raise RuntimeError('TX was not accepted; artifacts '+str(root))
if '"modeId":"hffax-experimental","state":"Completed"' not in log:
    raise RuntimeError('Radiofax TX did not report completion; artifacts '+str(root))
print('PASS Radiofax TX capture:',variant,'artifacts',root,flush=True)
