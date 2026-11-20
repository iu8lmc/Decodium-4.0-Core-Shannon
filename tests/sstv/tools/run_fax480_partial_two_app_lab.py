#!/usr/bin/env python3
"""Two FAX480 apps with a continuous virtual-audio stream muted mid-frame."""
import ctypes as c
import ctypes.util
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading

from run_hamdrm_two_app_echo_lab import Info, Params

if len(sys.argv) != 3 or sys.argv[1] not in ('dayton-480', 'qsstv-500', 'libsstv-vis-480') or sys.argv[2] not in ('a', 'b'):
    raise SystemExit('expected profile and a|b')
root = Path(tempfile.mkdtemp(prefix='decodium-fax480-partial-relay-'))
pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.c_void_p, c.c_void_p,
                             c.c_double, c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
pa.Pa_ReadStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
pa.Pa_WriteStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
if pa.Pa_Initialize(): raise RuntimeError('PortAudio initialisation failed')
source, target = c.c_void_p(), c.c_void_p()
source_started = target_started = False
stop = threading.Event()
errors = []
stats = {'frames_after_start': 0, 'muted_frames': 0}
worker = None
try:
    devices = {pa.Pa_GetDeviceInfo(i).contents.name.decode(): i for i in range(pa.Pa_GetDeviceCount())}
    input_params = Params(devices['BlackHole 16ch'], 2, 1, 0.05, None)
    output_params = Params(devices['BlackHole 2ch'], 2, 1, 0.05, None)
    if pa.Pa_OpenStream(c.byref(source), c.byref(input_params), None, 48000, 480, 0, None, None):
        raise RuntimeError('cannot open relay input')
    if pa.Pa_OpenStream(c.byref(target), None, c.byref(output_params), 48000, 480, 0, None, None):
        raise RuntimeError('cannot open relay output')
    if pa.Pa_StartStream(source): raise RuntimeError('cannot start relay input')
    source_started = True
    if pa.Pa_StartStream(target): raise RuntimeError('cannot start relay output')
    target_started = True

    def relay():
        incoming, outgoing = (c.c_float * 960)(), (c.c_float * 960)()
        started = False
        while not stop.is_set():
            if pa.Pa_ReadStream(source, incoming, 480):
                errors.append('input overflow'); break
            for frame in range(480):
                samples = (float(incoming[2 * frame]), float(incoming[2 * frame + 1]))
                if not all(math.isfinite(v) and abs(v) <= 1 for v in samples):
                    errors.append('invalid or clipped input'); return
                started |= max(map(abs, samples)) > 0.005
                muted = started and stats['frames_after_start'] >= 70 * 48000
                for channel in (0, 1): outgoing[2 * frame + channel] = 0.0 if muted else samples[channel]
                if started: stats['frames_after_start'] += 1
                if muted: stats['muted_frames'] += 1
            if pa.Pa_WriteStream(target, outgoing, 480):
                errors.append('output underflow'); break

    worker = threading.Thread(target=relay, daemon=True)
    worker.start()
    env = os.environ.copy()
    env.update(DECODIUM_LAB_FAX480_EXPECT_PARTIAL='1', DECODIUM_LAB_FAX480_TX_OUTPUT='BlackHole 16ch')
    result = subprocess.run([sys.executable, 'tests/sstv/tools/run_fax480_two_app_lab.py', *sys.argv[1:]],
                            env=env, capture_output=True, text=True, timeout=200)
    (root / 'apps.log').write_text(result.stdout + result.stderr)
    stop.set(); worker.join(timeout=5)
    if worker.is_alive() or errors or stats['muted_frames'] < 48000:
        raise RuntimeError('invalid relay: '+repr((errors, stats))+'; artifacts '+str(root))
    if result.returncode:
        raise RuntimeError('partial application check failed; artifacts '+str(root))
    print(result.stdout.strip(), 'relay artifacts', root, 'muted frames', stats['muted_frames'], flush=True)
finally:
    stop.set()
    if worker: worker.join(timeout=5)
    if source_started: pa.Pa_StopStream(source)
    if target_started: pa.Pa_StopStream(target)
    if source: pa.Pa_CloseStream(source)
    if target: pa.Pa_CloseStream(target)
    pa.Pa_Terminate()
