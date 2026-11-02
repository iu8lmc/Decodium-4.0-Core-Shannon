#!/usr/bin/env python3
"""Run two-app repair while silencing part of the initial virtual-audio TX."""
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

if len(sys.argv) != 2 or sys.argv[1] not in ('a', 'b'):
    raise SystemExit('expected a or b')
root = Path(tempfile.mkdtemp(prefix='decodium-hamdrm-repair-audio-loss-'))
capture_path = (Path(os.devnull) if os.environ.get('DECODIUM_LAB_REPAIR_QSSTV_BINARY')
                and os.environ.get('DECODIUM_LAB_REPAIR_REVERSE_CAPTURE') != '1' else root / 'mixed.f32')
pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.c_void_p, c.c_void_p,
                             c.c_double, c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
pa.Pa_ReadStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
pa.Pa_WriteStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
if pa.Pa_Initialize(): raise RuntimeError('PortAudio initialisation failed')
source, target, monitor = c.c_void_p(), c.c_void_p(), c.c_void_p()
started_source = started_target = started_monitor = False
stop = threading.Event()
errors = []
stats = {'active_timeline_frames': 0, 'muted_frames': 0, 'muted_active_frames': 0}
worker = capture_worker = None
try:
    devices = {pa.Pa_GetDeviceInfo(i).contents.name.decode(): i for i in range(pa.Pa_GetDeviceCount())}
    inp = Params(devices['BlackHole 16ch'], 2, 1, 0.05, None)
    out = Params(devices['BlackHole 2ch'], 2, 1, 0.05, None)
    if pa.Pa_OpenStream(c.byref(source), c.byref(inp), None, 48000, 480, 0, None, None):
        raise RuntimeError('cannot open relay input')
    if pa.Pa_OpenStream(c.byref(target), None, c.byref(out), 48000, 480, 0, None, None):
        raise RuntimeError('cannot open relay output')
    monitor_params = Params(devices['BlackHole 2ch'], 2, 1, 0.05, None)
    if pa.Pa_OpenStream(c.byref(monitor), c.byref(monitor_params), None, 48000, 480, 0, None, None):
        raise RuntimeError('cannot open mixed-audio capture')
    if pa.Pa_StartStream(source): raise RuntimeError('cannot start input')
    started_source = True
    if pa.Pa_StartStream(target): raise RuntimeError('cannot start output')
    started_target = True
    if pa.Pa_StartStream(monitor): raise RuntimeError('cannot start capture')
    started_monitor = True

    def relay():
        incoming, outgoing = (c.c_float * 960)(), (c.c_float * 960)()
        started = False
        while not stop.is_set():
            if pa.Pa_ReadStream(source, incoming, 480):
                errors.append('input overflow'); return
            for frame in range(480):
                samples = (float(incoming[2 * frame]), float(incoming[2 * frame + 1]))
                if not all(math.isfinite(v) and abs(v) <= 1 for v in samples):
                    errors.append('invalid input'); return
                active = max(map(abs, samples)) > 0.005
                started |= active
                position = stats['active_timeline_frames']
                mute = started and 6 * 48000 <= position < 9 * 48000
                for channel in (0, 1): outgoing[2 * frame + channel] = 0 if mute else samples[channel]
                if started: stats['active_timeline_frames'] += 1
                if mute:
                    stats['muted_frames'] += 1
                    stats['muted_active_frames'] += active
            if pa.Pa_WriteStream(target, outgoing, 480):
                errors.append('output underflow'); return

    def capture():
        buffer = (c.c_float * 960)()
        try:
            with capture_path.open('wb' if capture_path == Path(os.devnull) else 'xb') as file:
                while not stop.is_set():
                    if pa.Pa_ReadStream(monitor, buffer, 480):
                        errors.append('mixed-audio capture overflow'); return
                    file.write(bytes(buffer))
        except OSError as error:
            errors.append('capture write failed: ' + str(error))

    worker = threading.Thread(target=relay, daemon=True)
    capture_worker = threading.Thread(target=capture, daemon=True)
    capture_worker.start()
    worker.start()
    env = os.environ.copy()
    env['DECODIUM_LAB_REPAIR_AUDIO_LOSS'] = '1'
    if env.get('DECODIUM_LAB_REPAIR_QSSTV_REVERSE') == '1':
        command = [sys.executable, 'tests/sstv/tools/run_hamdrm_qsstv_reverse_lab.py', sys.argv[1]]
    else:
        command = [sys.executable, 'tests/sstv/tools/run_hamdrm_repair_two_app_lab.py', sys.argv[1], 'live']
    result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=170)
    (root / 'apps.log').write_text(result.stdout + result.stderr)
    stop.set(); worker.join(timeout=5); capture_worker.join(timeout=5)
    (root / 'relay.txt').write_text(repr({'stats': stats, 'errors': errors}))
    if worker.is_alive() or capture_worker.is_alive() or errors or stats['muted_frames'] != 3 * 48000 or stats['muted_active_frames'] < 48000:
        raise RuntimeError('invalid relay: ' + repr((errors, stats)) + '; artifacts ' + str(root))
    if result.returncode:
        raise RuntimeError('audio-loss repair did not complete; artifacts ' + str(root))
    print(result.stdout.strip(), 'relay artifacts', root, 'stats', stats, flush=True)
finally:
    stop.set()
    if worker: worker.join(timeout=5)
    if capture_worker: capture_worker.join(timeout=5)
    if started_source: pa.Pa_StopStream(source)
    if started_target: pa.Pa_StopStream(target)
    if started_monitor: pa.Pa_StopStream(monitor)
    if source: pa.Pa_CloseStream(source)
    if target: pa.Pa_CloseStream(target)
    if monitor: pa.Pa_CloseStream(monitor)
    pa.Pa_Terminate()
