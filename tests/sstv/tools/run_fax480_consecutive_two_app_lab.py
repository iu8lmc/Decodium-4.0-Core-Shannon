#!/usr/bin/env python3
"""Send A/B FAX480 images to one continuously running Decodium receiver."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import time
import ctypes as c
import ctypes.util
import threading

if len(sys.argv) != 3 or sys.argv[1] not in ('a', 'b') or sys.argv[2] not in ('dayton-480', 'qsstv-500', 'libsstv-vis-480'):
    raise SystemExit('expected a|b dayton-480|qsstv-500|libsstv-vis-480')
order = ('a', 'b') if sys.argv[1] == 'a' else ('b', 'a')
profile = sys.argv[2]
root = Path(tempfile.mkdtemp(prefix='decodium-fax480-consecutive-' + sys.argv[1] + '-'))
binary = Path('build/decodium').resolve(strict=True)
common = ['--test-mode', '--lab-no-cat', '--lab-audio-input', 'BlackHole 2ch',
          '--lab-audio-output', 'BlackHole 2ch', '--lab-reapply-ms', '500']
processes = []
capture_stop = threading.Event()
capture_thread = None
capture_errors = []

def launch(role, args, sequence=False):
    env = os.environ.copy()
    env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
               DECODIUM_LAB_SETTINGS_DIR=str(root / (role + '-settings')))
    if sequence: env['DECODIUM_LAB_RX_IMAGE_SEQUENCE'] = '1'
    with (root / (role + '.log')).open('wb') as log:
        process = subprocess.Popen([str(binary), *common, '--rig-name',
                                    'FAX' + role + secrets.token_hex(4), *args],
                                   env=env, stdout=log, stderr=subprocess.STDOUT)
    processes.append(process)
    return process

try:
    rx = launch('rx', ['--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500',
        '--lab-sstv-fax480-rx', profile, '--lab-sstv-rx-ready-file', str(root/'ready'),
        '--lab-sstv-rx-image-file', str(root/'received'), '--lab-quit-ms', '360000'], True)
    deadline = time.monotonic() + 20
    while not (root/'ready').exists():
        if rx.poll() is not None or time.monotonic() > deadline: raise RuntimeError('RX not ready')
        time.sleep(.1)
    if json.loads((root/'ready').read_text()).get('sourceDevice') != 'BlackHole 2ch':
        raise RuntimeError('wrong RX device')
    if os.environ.get('DECODIUM_LAB_FAX480_CAPTURE_HEADER') == '1':
        from run_hamdrm_two_app_echo_lab import Info, Params
        pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
        pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
        pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.POINTER(Params), c.c_void_p,
                                    c.c_double, c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
        pa.Pa_ReadStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
        if pa.Pa_Initialize(): raise RuntimeError('capture initialization failed')
        device = next(i for i in range(pa.Pa_GetDeviceCount())
                      if pa.Pa_GetDeviceInfo(i).contents.name == b'BlackHole 2ch')
        params = Params(device, 2, 1, .1, None)
        stream = c.c_void_p()
        if pa.Pa_OpenStream(c.byref(stream), c.byref(params), None, 48000, 480, 0, None, None):
            raise RuntimeError('capture open failed')
        if pa.Pa_StartStream(stream): raise RuntimeError('capture start failed')
        def capture():
            buffer = (c.c_float * 960)()
            with (root/'header.f32').open('xb') as out:
                for _ in range(3000):
                    if capture_stop.is_set(): break
                    if pa.Pa_ReadStream(stream, buffer, 480):
                        capture_errors.append('input overflow'); break
                    out.write(bytes(buffer))
        capture_thread = threading.Thread(target=capture, daemon=True)
        capture_thread.start()
    # Allow the capture path and demodulator to settle before the first VIS.
    time.sleep(4)
    for index, variant in enumerate(order):
        tx = launch('tx'+str(index), ['--lab-no-monitor', '--lab-sstv-tx-ms', '4000',
            '--lab-sstv-fax480-tx', profile, '--lab-sstv-tx-wait-file', str(root/'ready'),
            '--lab-sstv-tx-wait-timeout-ms', '20000', '--lab-sstv-image',
            str(Path('tests/sstv/fixtures/qsstv-image-'+variant+'.jpg').resolve(strict=True)),
            '--lab-sstv-tx-image-file', str(root/('reference'+str(index)+'.png')),
            '--lab-sstv-vox', '--lab-quit-ms', '160000'])
        deadline = time.monotonic()+165
        while '[LAB][FAX480] TX state Completed' not in (root/('tx'+str(index)+'.log')).read_text(errors='replace'):
            if tx.poll() is not None or rx.poll() is not None or time.monotonic()>deadline:
                raise RuntimeError('TX completion missing or RX stopped')
            time.sleep(.5)
        if tx.poll() is None:
            tx.terminate()
            try: tx.wait(timeout=5)
            except subprocess.TimeoutExpired: tx.kill(); tx.wait(timeout=5)
        print('TX',index+1,'complete; artifacts',root,flush=True)
    deadline=time.monotonic()+15
    while len(list(root.glob('received.*.png')))<2 and time.monotonic()<deadline and rx.poll() is None:
        time.sleep(.5)
    images=sorted(root.glob('received.*.png'),key=lambda p:int(p.name.split('.')[1]))
    if len(images)!=2: raise RuntimeError('expected two complete images')
    # Image publication can precede the periodic diagnostics snapshot.
    deadline=time.monotonic()+10
    while True:
        rows=[json.loads(line.split(' RX diagnostics ',1)[1]) for line in (root/'rx.log').read_text(errors='replace').splitlines()
              if '[LAB][SSTV] RX diagnostics ' in line]
        ids={row.get('imageAcquisitionId') for row in rows if row.get('imageComplete') and row.get('imageCoverage')==1}
        if len(ids)>=2 or time.monotonic()>=deadline or rx.poll() is not None: break
        time.sleep(.25)
    if len(ids)<2 or any(row.get('processingFailures') or row.get('discontinuities') or row.get('droppedChunks') for row in rows):
        raise RuntimeError('invalid RX diagnostics')
    for index,image in enumerate(images):
        subprocess.run([sys.executable,'tests/sstv/tools/compare_sstv_lab_images.py',
                        str(root/('reference'+str(index)+'.png')),str(image),
                        '--max-mae','8','--max-p95','10'],check=True,timeout=30)
    if capture_thread and (capture_thread.is_alive() or capture_errors):
        raise RuntimeError('header capture failed or did not finish')
    print('PASS consecutive FAX480 images',profile,order,'artifacts',root,flush=True)
finally:
    for process in processes:
        if process.poll() is None: process.kill(); process.wait()
    if capture_thread:
        capture_stop.set(); capture_thread.join(timeout=5)
        pa.Pa_StopStream(stream); pa.Pa_CloseStream(stream); pa.Pa_Terminate()
        if capture_errors: print('capture errors:', capture_errors, file=sys.stderr)
