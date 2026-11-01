#!/usr/bin/env python3
"""Centered Radiofax two-app audio proof using BlackHole 2ch on macOS."""
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

variant=sys.argv[1]
if variant not in ('a','b'): raise SystemExit('expected a or b')
if len(sys.argv) > 3: raise SystemExit('expected a|b [60|90|120|240]')
rate=int(sys.argv[2]) if len(sys.argv)==3 else 240
if rate not in (60,90,120,240): raise SystemExit('invalid line rate')
profile=f'576,{rate},1'
tx_quit_ms=95000 + max(0, int(120 * 60000 / rate) - 30000)
root=Path(tempfile.mkdtemp(prefix='decodium-radiofax-centered-'+variant+'-'))
binary=Path('build/decodium').resolve(strict=True)
fixture=Path('tests/sstv/fixtures/qsstv-image-'+variant+'.jpg').resolve(strict=True)
common=['--test-mode','--lab-no-cat','--lab-audio-input','BlackHole 2ch',
        '--lab-audio-output','BlackHole 2ch','--lab-reapply-ms','500']
processes=[]
pa = stream = thread = None
stop = threading.Event()
capture_errors = []
try:
    if os.environ.get('DECODIUM_LAB_FAX_CAPTURE') == '1':
        from run_hamdrm_two_app_echo_lab import Info, Params
        pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
        pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
        pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.c_void_p, c.c_void_p,
                                     c.c_double, c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
        pa.Pa_ReadStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
        if pa.Pa_Initialize(): raise RuntimeError('PortAudio initialization failed')
        device = next(i for i in range(pa.Pa_GetDeviceCount())
                      if pa.Pa_GetDeviceInfo(i).contents.name == b'BlackHole 2ch')
        params = Params(device, 2, 1, 0.05, None)
        stream = c.c_void_p()
        if pa.Pa_OpenStream(c.byref(stream), c.byref(params), None, 48000, 1024, 0, None, None):
            raise RuntimeError('cannot open audio capture')
        if pa.Pa_StartStream(stream): raise RuntimeError('cannot start audio capture')
        def capture():
            buffer = (c.c_float * 2048)()
            with (root/'capture.f32').open('xb') as output:
                while not stop.is_set():
                    if pa.Pa_ReadStream(stream, buffer, 1024):
                        capture_errors.append('audio capture overflow'); break
                    output.write(bytes(buffer))
        thread = threading.Thread(target=capture, daemon=True)
        thread.start()
    for role in ('rx','tx'):
        env=os.environ.copy()
        env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',
                   DECODIUM_LAB_SETTINGS_DIR=str(root/(role+'-settings')))
        if os.environ.get('DECODIUM_LAB_FAX_CAPTURE') == '1':
            env['DECODIUM_LAB_FAX_DIAGNOSTICS'] = '1'
            env['DECODIUM_LAB_TX_IDLE_TRACE'] = '1'
        args=[str(binary),*common,'--rig-name','RFAX'+role+secrets.token_hex(4)]
        if role=='rx':
            args+=['--lab-monitor-ms','1800','--lab-sstv-rx-ms','3500',
                   '--lab-sstv-fax-rx','576,128,120,1','--lab-sstv-rx-ready-file',str(root/'ready'),
                   '--lab-sstv-rx-image-file',str(root/'received.png'),'--lab-quit-ms',str(tx_quit_ms+5000)]
        else:
            args+=['--lab-no-monitor','--lab-sstv-tx-ms','4000','--lab-sstv-mode','avt-24','--lab-sstv-fax-tx',profile,
                   '--lab-sstv-tx-wait-file',str(root/'ready'),'--lab-sstv-tx-wait-timeout-ms','20000',
                   '--lab-sstv-image',str(fixture),'--lab-sstv-tx-image-file',str(root/'reference.png'),
                   '--lab-sstv-vox','--lab-quit-ms',str(tx_quit_ms)]
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
    deadline=time.monotonic()+tx_quit_ms/1000+15
    while any(p.poll() is None for p in processes):
        if capture_errors: raise RuntimeError('capture failed; artifacts '+str(root))
        if any(p.poll() not in (None,0) for p in processes): raise RuntimeError('application failed')
        if time.monotonic()>deadline: raise RuntimeError('applications timed out')
        time.sleep(.5)
    if capture_errors: raise RuntimeError('capture failed; artifacts '+str(root))
    tx=(root/'tx.log').read_text(errors='replace')
    if os.environ.get('DECODIUM_LAB_FAX_CAPTURE') == '1' and '[LAB][SSTV] TX sink idle' not in tx:
        raise RuntimeError('TX sink diagnostic was not observed; artifacts '+str(root))
    if 'tx.completed ' not in tx or '"modeId":"hffax-experimental","state":"Completed"' not in tx:
        raise RuntimeError('TX did not report completion; artifacts '+str(root))
    rx=(root/'rx.log').read_text(errors='replace')
    lines=[json.loads(line.split(' RX diagnostics ',1)[1]) for line in rx.splitlines()
           if '[LAB][SSTV] RX diagnostics ' in line]
    if (not lines or not lines[-1]['imageComplete']
        or any(item.get('sourceDevice') != 'BlackHole 2ch' or item.get('processingFailures') != 0
               or item.get('discontinuities') != 0 or item.get('droppedChunks') != 0 for item in lines)):
        raise RuntimeError('RX did not report a complete image; artifacts '+str(root))
    if not (root/'received.png').exists(): raise RuntimeError('RX did not save image; artifacts '+str(root))
    subprocess.run([sys.executable,'tests/sstv/tools/compare_sstv_lab_images.py',
                    str(root/'reference.png'),str(root/'received.png'),
                    '--max-mae','8','--max-p95','15'],check=True,timeout=30)
    print('PASS two Decodium apps:',profile,'coverage',lines[-1]['imageCoverage'],
          'discontinuities',lines[-1]['discontinuities'],'artifacts',root,flush=True)
finally:
    for process in processes:
        if process.poll() is None:
            process.kill()
            process.wait()
    if thread:
        stop.set(); thread.join(timeout=5)
        if thread.is_alive(): raise RuntimeError('capture did not stop; artifacts '+str(root))
    if stream and stream.value:
        pa.Pa_StopStream(stream); pa.Pa_CloseStream(stream)
    if pa: pa.Pa_Terminate()
    if capture_errors: raise RuntimeError('capture failed; artifacts '+str(root))
