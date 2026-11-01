#!/usr/bin/env python3
"""SSTV two-app audio proof using BlackHole 2ch on macOS."""
import json
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

mode=sys.argv[1]
allowed={f'avt-{duration}-{variant}' for duration in ('24','90','94')
         for variant in ('qrm','narrow-qrm')}
allowed.add('robot-c12')
if mode not in allowed: raise SystemExit('expected robot-c12 or an AVT 24/90/94 QRM or narrow-QRM mode')
root=Path(tempfile.mkdtemp(prefix='decodium-avt-two-app-'+mode+'-'))
binary=Path('build/decodium').resolve(strict=True)
variant=sys.argv[2] if len(sys.argv)>2 else 'a'
if variant not in ('a','b'): raise SystemExit('image variant must be a or b')
fixture=Path('tests/sstv/fixtures/qsstv-image-'+variant+'.jpg').resolve(strict=True)
duration_ms=35000 if mode == 'robot-c12' else (60000 if mode.startswith('avt-24-') else 130000)
common=['--test-mode','--lab-no-cat','--lab-audio-input','BlackHole 2ch',
        '--lab-audio-output','BlackHole 2ch','--lab-reapply-ms','500']
processes=[]
class Info(c.Structure):
    _fields_=[('version',c.c_int),('name',c.c_char_p),('host',c.c_int),('inputs',c.c_int),
              ('outputs',c.c_int),('lowin',c.c_double),('lowout',c.c_double),
              ('highin',c.c_double),('highout',c.c_double),('rate',c.c_double)]
class Params(c.Structure):
    _fields_=[('device',c.c_int),('channels',c.c_int),('format',c.c_ulong),
              ('latency',c.c_double),('hostinfo',c.c_void_p)]
pa=c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
pa.Pa_GetDeviceInfo.restype=c.POINTER(Info)
pa.Pa_OpenStream.argtypes=[c.POINTER(c.c_void_p),c.POINTER(Params),c.c_void_p,
                           c.c_double,c.c_ulong,c.c_ulong,c.c_void_p,c.c_void_p]
pa.Pa_ReadStream.argtypes=[c.c_void_p,c.c_void_p,c.c_ulong]
if pa.Pa_Initialize(): raise RuntimeError('PortAudio initialization failed')
device=next(i for i in range(pa.Pa_GetDeviceCount())
            if pa.Pa_GetDeviceInfo(i).contents.name==b'BlackHole 2ch')
params=Params(device,2,1,.05,None);stream=c.c_void_p()
if pa.Pa_OpenStream(c.byref(stream),c.byref(params),None,48000,1024,0,None,None):
    raise RuntimeError('cannot open BlackHole capture')
if pa.Pa_StartStream(stream): raise RuntimeError('cannot start BlackHole capture')
stop=threading.Event();capture_errors=[]
def capture():
    buffer=(c.c_float*2048)()
    with (root/'capture.f32').open('wb') as out:
        while not stop.is_set():
            result=pa.Pa_ReadStream(stream,buffer,1024)
            if result:
                capture_errors.append(result);break
            out.write(bytes(buffer))
thread=threading.Thread(target=capture)
thread.start()
try:
    for role in ('rx','tx'):
        env=os.environ.copy()
        env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',
                   DECODIUM_LAB_SETTINGS_DIR=str(root/(role+'-settings')))
        args=[str(binary),*common,'--rig-name','AVT'+role+secrets.token_hex(4)]
        if role=='rx':
            args+=['--lab-monitor-ms','1800','--lab-sstv-rx-ms','3500',
                   '--lab-sstv-rx-ready-file',str(root/'ready'),
                   '--lab-sstv-rx-image-file',str(root/'received.png'),'--lab-quit-ms',str(duration_ms+5000)]
            if mode != 'robot-c12': args += ['--lab-sstv-rx-mode', mode]
        else:
            args+=['--lab-no-monitor','--lab-sstv-tx-ms','4000','--lab-sstv-mode',mode,
                   '--lab-sstv-tx-wait-file',str(root/'ready'),'--lab-sstv-tx-wait-timeout-ms','20000',
                   '--lab-sstv-image',str(fixture),'--lab-sstv-tx-image-file',str(root/'reference.png'),
                   '--lab-sstv-vox','--lab-quit-ms',str(duration_ms)]
        with (root/(role+'.log')).open('wb') as log:
            processes.append(subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT))
        if role=='rx':
            deadline=time.monotonic()+20
            while not (root/'ready').exists():
                if processes[0].poll() is not None or time.monotonic()>deadline:
                    raise RuntimeError('RX did not become ready; artifacts '+str(root))
                time.sleep(.1)
            ready=json.loads((root/'ready').read_text())
            if ready.get('sourceDevice')!='BlackHole 2ch':
                raise RuntimeError('wrong RX audio device: '+str(ready.get('sourceDevice')))
    deadline=time.monotonic()+duration_ms/1000+15
    while any(p.poll() is None for p in processes):
        if capture_errors: raise RuntimeError('capture error '+str(capture_errors))
        if any(p.poll() not in (None,0) for p in processes): raise RuntimeError('application failed')
        if time.monotonic()>deadline: raise RuntimeError('applications timed out')
        time.sleep(.5)
    tx=(root/'tx.log').read_text(errors='replace')
    if not any(json.loads(line.split('tx.completed ',1)[1]).get('success') and
               json.loads(line.split('tx.completed ',1)[1]).get('modeId')==mode
               for line in tx.splitlines() if 'tx.completed {' in line):
        raise RuntimeError('TX did not report completion; artifacts '+str(root))
    rx=(root/'rx.log').read_text(errors='replace')
    lines=[json.loads(line.split(' RX diagnostics ',1)[1]) for line in rx.splitlines()
           if '[LAB][SSTV] RX diagnostics ' in line]
    if not lines or not lines[-1]['imageComplete'] or lines[-1]['processingFailures']:
        raise RuntimeError('RX did not report a complete image; artifacts '+str(root))
    if not (root/'received.png').exists(): raise RuntimeError('RX did not save image; artifacts '+str(root))
    subprocess.run([sys.executable,'tests/sstv/tools/compare_sstv_lab_images.py',
                    str(root/'reference.png'),str(root/'received.png'),
                    '--max-mae','5','--max-p95','15'],check=True,timeout=30)
    print('PASS two Decodium apps:',mode,'coverage',lines[-1]['imageCoverage'],
          'discontinuities',lines[-1]['discontinuities'],'artifacts',root,flush=True)
finally:
    for process in processes:
        if process.poll() is None:
            process.kill()
            process.wait()
    stop.set();thread.join(timeout=5)
    pa.Pa_StopStream(stream);pa.Pa_CloseStream(stream);pa.Pa_Terminate()
