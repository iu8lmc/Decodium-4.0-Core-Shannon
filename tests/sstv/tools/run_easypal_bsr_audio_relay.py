#!/usr/bin/env python3
"""Relay BlackHole 16ch to 2ch, injecting a bounded initial audio loss.

A test-only signal is muted from six to nine seconds after first activity.
After that all samples pass through, including any later repair. Use a
separate process for EasyPal and the Decodium transmitter. Stop with SIGTERM;
statistics and any failure are preserved in the supplied directory.
"""
import argparse
import ctypes as c
import ctypes.util
import json
import math
from pathlib import Path
import signal
import sys
import time
from run_hamdrm_two_app_echo_lab import Info, Params

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
parser.add_argument('--no-loss', action='store_true', help='pass all audio without an injected mute')
args = parser.parse_args()
root = args.directory.resolve(strict=True)
pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.c_void_p, c.c_void_p,
                            c.c_double, c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
pa.Pa_ReadStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
pa.Pa_WriteStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
if pa.Pa_Initialize():
    raise RuntimeError('PortAudio initialisation failed')
source, target = c.c_void_p(), c.c_void_p()
source_started = target_started = False
stopping = False
stats = {'active_timeline_frames': 0, 'muted_frames': 0, 'muted_active_frames': 0, 'error': ''}
def stop(*_):
    global stopping
    stopping = True
signal.signal(signal.SIGTERM, stop)
signal.signal(signal.SIGINT, stop)
try:
    devices = [pa.Pa_GetDeviceInfo(i).contents for i in range(pa.Pa_GetDeviceCount())]
    ins = [i for i, d in enumerate(devices) if d.name == b'BlackHole 16ch' and d.inputs >= 2]
    outs = [i for i, d in enumerate(devices) if d.name == b'BlackHole 2ch' and d.outputs >= 2]
    if len(ins) != 1 or len(outs) != 1:
        raise RuntimeError('BlackHole input/output not uniquely available')
    inp, out = Params(ins[0], 2, 1, .05, None), Params(outs[0], 2, 1, .05, None)
    if pa.Pa_OpenStream(c.byref(source), c.byref(inp), None, 48000, 480, 0, None, None):
        raise RuntimeError('cannot open relay input')
    if pa.Pa_OpenStream(c.byref(target), None, c.byref(out), 48000, 480, 0, None, None):
        raise RuntimeError('cannot open relay output')
    if pa.Pa_StartStream(source): raise RuntimeError('cannot start input')
    source_started = True
    if pa.Pa_StartStream(target): raise RuntimeError('cannot start output')
    target_started = True
    (root / 'relay-ready').write_text('BlackHole 16ch -> BlackHole 2ch\n')
    incoming, outgoing = (c.c_float * 960)(), (c.c_float * 960)()
    started = False
    deadline = time.monotonic() + 900
    while not stopping and time.monotonic() < deadline:
        if pa.Pa_ReadStream(source, incoming, 480): raise RuntimeError('input overflow')
        for frame in range(480):
            left, right = float(incoming[2 * frame]), float(incoming[2 * frame + 1])
            if not all(math.isfinite(v) and abs(v) <= 1 for v in (left, right)):
                raise RuntimeError('invalid sample')
            active = max(abs(left), abs(right)) > .005
            started |= active
            position = stats['active_timeline_frames']
            mute = not args.no_loss and started and 6 * 48000 <= position < 9 * 48000
            outgoing[2 * frame] = 0 if mute else left
            outgoing[2 * frame + 1] = 0 if mute else right
            if started: stats['active_timeline_frames'] += 1
            if mute:
                stats['muted_frames'] += 1
                stats['muted_active_frames'] += int(active)
        if pa.Pa_WriteStream(target, outgoing, 480): raise RuntimeError('output underflow')
except Exception as error:
    stats['error'] = str(error)
    raise
finally:
    (root / 'relay-stats.json').write_text(json.dumps(stats, indent=2) + '\n')
    if source_started: pa.Pa_StopStream(source)
    if target_started: pa.Pa_StopStream(target)
    if source: pa.Pa_CloseStream(source)
    if target: pa.Pa_CloseStream(target)
    pa.Pa_Terminate()
