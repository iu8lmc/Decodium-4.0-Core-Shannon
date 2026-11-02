#!/usr/bin/env python3
"""Replay an FLdigi stereo float capture to a full Decodium Radiofax receiver."""
import argparse
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

parser = argparse.ArgumentParser()
parser.add_argument('--expect-partial', action='store_true')
parser.add_argument('--wmo-dead-sector', action='store_true',
                    help='decode the explicit WMO dead-sector profile')
parser.add_argument('capture', type=Path)
parser.add_argument('reference', type=Path)
options = parser.parse_args()
source = options.capture.resolve(strict=True)
reference = options.reference.resolve(strict=True)
if source.stat().st_size <= 0 or source.stat().st_size % 8 or source.stat().st_size > 240 * 48000 * 8:
    raise SystemExit('invalid stereo float capture')
repo = Path.cwd()
binary = (repo / 'build/decodium').resolve(strict=True)
root = Path(tempfile.mkdtemp(prefix='decodium-fldigi-radiofax-replay-'))
env = os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
           DECODIUM_LAB_SETTINGS_DIR=str(root / 'settings'))
args = [str(binary), '--test-mode', '--rig-name', 'FLFAX' + secrets.token_hex(4),
        '--lab-no-cat', '--lab-audio-input', 'BlackHole 2ch', '--lab-audio-output', 'BlackHole 2ch',
        '--lab-reapply-ms', '500', '--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500',
        '--lab-sstv-fax-rx', '576,128,150,0,1' if options.wmo_dead_sector else '576,128,150,1',
        '--lab-sstv-rx-ready-file', str(root / 'ready'),
        '--lab-sstv-rx-image-file', str(root / 'received.png'), '--lab-quit-ms',
        str(max(115000, source.stat().st_size * 1000 // (48000 * 8) + 20000))]
env['DECODIUM_LAB_FAX_DIAGNOSTICS'] = '1'

class Info(c.Structure):
    _fields_ = [('version', c.c_int), ('name', c.c_char_p), ('host', c.c_int),
        ('inputs', c.c_int), ('outputs', c.c_int), ('lowin', c.c_double),
        ('lowout', c.c_double), ('highin', c.c_double), ('highout', c.c_double), ('rate', c.c_double)]
class Params(c.Structure):
    _fields_ = [('device', c.c_int), ('channels', c.c_int), ('format', c.c_ulong),
                ('latency', c.c_double), ('hostinfo', c.c_void_p)]

process = None
try:
    with (root / 'rx.log').open('wb') as log:
        process = subprocess.Popen(args, env=env, stdout=log, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + 25
    while not (root / 'ready').exists():
        if process.poll() is not None or time.monotonic() > deadline:
            raise RuntimeError('receiver not ready; artifacts ' + str(root))
        time.sleep(.1)
    ready = json.loads((root / 'ready').read_text())
    if ready.get('sourceDevice') != 'BlackHole 2ch': raise RuntimeError('incorrect audio device')
    pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
    pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
    pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.c_void_p, c.POINTER(Params),
                                 c.c_double, c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
    pa.Pa_WriteStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
    if pa.Pa_Initialize(): raise RuntimeError('PortAudio initialization failed')
    device = next(i for i in range(pa.Pa_GetDeviceCount()) if pa.Pa_GetDeviceInfo(i).contents.name == b'BlackHole 2ch')
    params = Params(device, 2, 1, .05, None)
    stream = c.c_void_p()
    if pa.Pa_OpenStream(c.byref(stream), None, c.byref(params), 48000, 1024, 0, None, None):
        raise RuntimeError('cannot open BlackHole output')
    if pa.Pa_StartStream(stream): raise RuntimeError('cannot start BlackHole output')
    with source.open('rb') as capture:
        while True:
            chunk = capture.read(8192 * 8)
            if not chunk: break
            data = c.create_string_buffer(chunk)
            if pa.Pa_WriteStream(stream, data, len(chunk) // 8): raise RuntimeError('audio playback failed')
    pa.Pa_StopStream(stream); pa.Pa_CloseStream(stream); pa.Pa_Terminate()
    if process.wait(timeout=30): raise RuntimeError('receiver exited with failure; artifacts ' + str(root))
    lines = [json.loads(line.split(' RX diagnostics ', 1)[1])
             for line in (root / 'rx.log').read_text(errors='replace').splitlines()
             if '[LAB][SSTV] RX diagnostics ' in line]
    if not lines or not lines[-1].get('imageRadiofaxStopDetected') or lines[-1].get('processingFailures'):
        raise RuntimeError('RX did not report a clean Radiofax stop; artifacts ' + str(root))
    if any(item.get('sourceDevice') != 'BlackHole 2ch'
           or item.get('discontinuities') != 0
           or item.get('droppedChunks') != 0
           or item.get('processingFailures') != 0 for item in lines):
        raise RuntimeError('RX source or audio continuity changed during replay; artifacts ' + str(root))
    print('receiver lines', lines[-1].get('imageLinesPublished'),
          'coverage', lines[-1].get('imageCoverage'), 'artifacts', root, flush=True)
    received = root / 'received.png'
    if not received.exists(): raise RuntimeError('RX image not saved; artifacts ' + str(root))
    cropped = root / 'first-120.png'
    subprocess.run(['ffmpeg', '-v', 'error', '-i', str(received), '-vf', 'crop=128:120:0:0',
                    '-frames:v', '1', str(cropped)], check=True, timeout=30)
    subprocess.run([sys.executable, str(repo / 'tests/sstv/tools/compare_sstv_lab_images.py'),
                    str(reference), str(cropped), '--max-mae', '255' if options.expect_partial else '8',
                    '--max-p95', '255' if options.expect_partial else '4'], check=True, timeout=30)
    count = lines[-1].get('imageLinesPublished')
    if options.expect_partial and not (lines[-1].get('imagePartial') and 0 < count < 120):
        raise RuntimeError('expected an incomplete image from the modified signal; artifacts ' + str(root))
    if not options.expect_partial and count != 120:
        raise RuntimeError('unexpected terminal line count; artifacts ' + str(root))
    print('PASS Radiofax replay', 'partial control' if options.expect_partial else 'complete', 'artifacts', root, flush=True)
finally:
    if process and process.poll() is None:
        process.kill()
        process.wait()
