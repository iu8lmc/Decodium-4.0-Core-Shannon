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
         for variant in ('narrow','qrm','narrow-qrm')}
allowed.add('avt-24')
allowed.add('robot-c12')
if mode not in allowed: raise SystemExit('expected robot-c12, avt-24 or an AVT 24/90/94 narrow, QRM or narrow-QRM mode')
root=Path(tempfile.mkdtemp(prefix='decodium-avt-two-app-'+mode+'-'))
binary=Path('build/decodium').resolve(strict=True)
if len(sys.argv) > 4: raise SystemExit('expected mode, optional variant, and optional image path')
variant=sys.argv[2] if len(sys.argv)>2 else 'a'
if variant not in ('a','b'): raise SystemExit('image variant must be a or b')
if len(sys.argv)==4 and sys.argv[3]=='pattern':
    fixture=root/'input.ppm'
    pixels=bytearray()
    for y in range(120):
        for x in range(128):
            if variant=='a':
                rgb=((x*2+y//3)%256, (y*2+x//2)%256,
                     ((x//16+y//12)%2)*160+(x+y)%96)
            else:
                rgb=((255-2*x+y)%256, ((x//12+y//15)%2)*150+y%106,
                     (255-2*y+x)%256)
            pixels.extend(rgb)
    fixture.write_bytes(b'P6\n128 120\n255\n'+pixels)
else:
    fixture=(Path(sys.argv[3]) if len(sys.argv)==4 else Path('tests/sstv/fixtures/qsstv-image-'+variant+'.jpg')).resolve(strict=True)
if not fixture.is_file(): raise SystemExit('image path must be a file')
try:
    tx_delay_ms = int(os.environ.get('DECODIUM_LAB_AVT_TX_MS', '14000'))
except ValueError:
    raise SystemExit('DECODIUM_LAB_AVT_TX_MS must be an integer')
if not 1000 <= tx_delay_ms <= 30000:
    raise SystemExit('DECODIUM_LAB_AVT_TX_MS must be between 1000 and 30000')
duration_ms=(35000 if mode == 'robot-c12' else (60000 if mode.startswith('avt-24') else 130000))
duration_ms += max(0, tx_delay_ms - 14000)
common=['--test-mode','--lab-no-cat','--lab-audio-input','BlackHole 2ch',
        '--lab-reapply-ms','500']
tx_output=os.environ.get('DECODIUM_LAB_AVT_TX_OUTPUT','BlackHole 2ch')
if tx_output not in ('BlackHole 2ch','BlackHole 16ch'): raise SystemExit('unsupported lab output')
processes=[]
class Info(c.Structure):
    _fields_=[('version',c.c_int),('name',c.c_char_p),('host',c.c_int),('inputs',c.c_int),
              ('outputs',c.c_int),('lowin',c.c_double),('lowout',c.c_double),
              ('highin',c.c_double),('highout',c.c_double),('rate',c.c_double)]
class Params(c.Structure):
    _fields_=[('device',c.c_int),('channels',c.c_int),('format',c.c_ulong),
              ('latency',c.c_double),('hostinfo',c.c_void_p)]
capture_enabled=os.environ.get('DECODIUM_LAB_AVT_NO_CAPTURE')!='1'
require_continuous_capture = os.environ.get('DECODIUM_LAB_AVT_REQUIRE_CONTINUOUS_CAPTURE') == '1'
if require_continuous_capture and not capture_enabled:
    raise SystemExit('continuous capture cannot be required when capture is disabled')
pa=None; stream=c.c_void_p(); thread=None
if capture_enabled:
    pa=c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
    pa.Pa_GetDeviceInfo.restype=c.POINTER(Info)
    pa.Pa_OpenStream.argtypes=[c.POINTER(c.c_void_p),c.POINTER(Params),c.c_void_p,
                               c.c_double,c.c_ulong,c.c_ulong,c.c_void_p,c.c_void_p]
    pa.Pa_ReadStream.argtypes=[c.c_void_p,c.c_void_p,c.c_ulong]
    if pa.Pa_Initialize(): raise RuntimeError('PortAudio initialization failed')
    device=next(i for i in range(pa.Pa_GetDeviceCount())
                if pa.Pa_GetDeviceInfo(i).contents.name==b'BlackHole 2ch')
    params=Params(device,2,1,.05,None)
    if pa.Pa_OpenStream(c.byref(stream),c.byref(params),None,48000,1024,0,None,None):
        raise RuntimeError('cannot open BlackHole capture')
    if pa.Pa_StartStream(stream): raise RuntimeError('cannot start BlackHole capture')
stop=threading.Event();capture_errors=[]
capture_path=root/'capture.f32'
def capture():
    buffer=(c.c_float*2048)()
    with capture_path.open('wb') as out:
        while not stop.is_set():
            result=pa.Pa_ReadStream(stream,buffer,1024)
            if result:
                capture_errors.append(result);break
            out.write(bytes(buffer))
if capture_enabled:
    thread=threading.Thread(target=capture)
    thread.start()
try:
    for role in ('rx','tx'):
        env=os.environ.copy()
        env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',
                   DECODIUM_LAB_SETTINGS_DIR=str(root/(role+'-settings')))
        args=[str(binary),*common,'--lab-audio-output',tx_output if role=='tx' else 'BlackHole 2ch',
              '--rig-name','AVT'+role+secrets.token_hex(4)]
        if role=='rx':
            args+=['--lab-monitor-ms','1800','--lab-sstv-rx-ms','3500',
                   '--lab-sstv-rx-ready-file',str(root/'ready'),
                   '--lab-sstv-rx-image-file',str(root/'received.png'),'--lab-quit-ms',str(duration_ms+5000)]
            if mode != 'robot-c12': args += ['--lab-sstv-rx-mode', mode]
        else:
            args+=['--lab-no-monitor','--lab-sstv-tx-ms',str(tx_delay_ms),'--lab-sstv-mode',mode,
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
    while True:
        if capture_errors: raise RuntimeError('capture error '+str(capture_errors))
        if any(p.poll() not in (None,0) for p in processes): raise RuntimeError('application failed')
        tx_text=(root/'tx.log').read_text(errors='replace')
        rx_text=(root/'rx.log').read_text(errors='replace')
        tx_done=False
        for line in tx_text.splitlines():
            if 'tx.completed {' not in line: continue
            try: report=json.loads(line.split('tx.completed ',1)[1])
            except json.JSONDecodeError: continue  # Writer may still be appending the line.
            tx_done |= report.get('success') and report.get('modeId')==mode
        diagnostics=[]
        for line in rx_text.splitlines():
            if '[LAB][SSTV] RX diagnostics ' not in line: continue
            try: diagnostics.append(json.loads(line.split(' RX diagnostics ',1)[1]))
            except json.JSONDecodeError: continue
        rx_done=(diagnostics and diagnostics[-1].get('imageComplete')
                 and diagnostics[-1].get('imageMode')==mode and (root/'received.png').is_file())
        if tx_done and rx_done:
            break
        if all(p.poll() is not None for p in processes):
            raise RuntimeError('applications exited before TX and RX completed; artifacts '+str(root))
        if time.monotonic()>deadline: raise RuntimeError('applications timed out')
        time.sleep(.5)
    tx=(root/'tx.log').read_text(errors='replace')
    if not any(json.loads(line.split('tx.completed ',1)[1]).get('success') and
               json.loads(line.split('tx.completed ',1)[1]).get('modeId')==mode
               for line in tx.splitlines() if 'tx.completed {' in line):
        raise RuntimeError('TX did not report completion; artifacts '+str(root))
    rx=(root/'rx.log').read_text(errors='replace')
    if os.environ.get('DECODIUM_LAB_VERIFY_MONITOR_ISOLATION') == '1':
        if ('[LAB] startup auto-monitor suppressed by --lab-no-monitor' not in tx
            or 'SoundInput::start' in tx
            or '[LAB] monitor requested active= true' not in rx):
            raise RuntimeError('lab monitor isolation was not maintained; artifacts '+str(root))
    lines=[json.loads(line.split(' RX diagnostics ',1)[1]) for line in rx.splitlines()
           if '[LAB][SSTV] RX diagnostics ' in line]
    if (not lines or not lines[-1]['imageComplete'] or lines[-1]['imageMode'] != mode
        or any(item.get('sourceDevice') != 'BlackHole 2ch' or item.get('processingFailures') != 0
               or item.get('discontinuities') != 0 or item.get('droppedChunks') != 0 for item in lines)):
        raise RuntimeError('RX did not report a complete image; artifacts '+str(root))
    notch=env.get('DECODIUM_LAB_SSTV_NOTCH_HZ')
    if notch and ('[LAB][SSTV] manual interference notch '+notch+' Q 300') not in rx:
        raise RuntimeError('requested notch was not active; artifacts '+str(root))
    if env.get('DECODIUM_LAB_AVT24_WINDOW') == '1' and (
        mode != 'avt-24-narrow-qrm' or '[LAB][AVT] AVT24 edge window diagnostic enabled' not in rx):
        raise RuntimeError('requested AVT24 diagnostic window was not applied; artifacts '+str(root))
    if env.get('DECODIUM_LAB_AVT24_SPLIT_EDGE') == '1' and (
        mode != 'avt-24-narrow-qrm' or '[LAB][AVT] AVT24 split-edge diagnostic enabled' not in rx):
        raise RuntimeError('requested AVT24 split-edge diagnostic was not applied; artifacts '+str(root))
    shift=env.get('DECODIUM_LAB_AVT_ANCHOR_SHIFT')
    if shift is not None and ('[LAB][AVT] anchor shift '+shift) not in rx:
        raise RuntimeError('requested AVT anchor shift was not applied; artifacts '+str(root))
    if not (root/'received.png').exists(): raise RuntimeError('RX did not save image; artifacts '+str(root))
    compare=[sys.executable,'tests/sstv/tools/compare_sstv_lab_images.py',
             str(root/'reference.png'),str(root/'received.png'),
             '--max-mae','5','--max-p95','15']
    last_blue_limit=env.get('DECODIUM_LAB_AVT_MAX_LAST_BLUE_ERROR')
    if last_blue_limit is not None:
        compare += ['--max-last-blue-error',last_blue_limit]
    last_column_limit=env.get('DECODIUM_LAB_AVT_MAX_LAST_COLUMN_ERROR')
    if last_column_limit is not None:
        compare += ['--max-last-column-error',last_column_limit]
    first_column_limit=env.get('DECODIUM_LAB_AVT_MAX_FIRST_COLUMN_ERROR')
    if first_column_limit is not None:
        compare += ['--max-first-column-error',first_column_limit]
    subprocess.run(compare,check=True,timeout=30)
finally:
    for process in processes:
        if process.poll() is None:
            process.kill()
            process.wait()
    stop.set()
    if thread: thread.join(timeout=5)
    if pa:
        pa.Pa_StopStream(stream);pa.Pa_CloseStream(stream);pa.Pa_Terminate()

if capture_enabled:
    print('Capture integrity (internal exact-zero runs; may be intentional):', flush=True)
    integrity = [sys.executable, 'tests/sstv/tools/check_avt_capture_integrity.py', str(capture_path)]
    if require_continuous_capture:
        integrity.append('--fail-on-gap')
    subprocess.run(integrity, check=True, timeout=30)
print('PASS two Decodium apps:',mode,'edge limits',
      {'first':first_column_limit,'last':last_column_limit},'coverage',lines[-1]['imageCoverage'],
      'rx_discontinuities',lines[-1]['discontinuities'],'artifacts',root,flush=True)
