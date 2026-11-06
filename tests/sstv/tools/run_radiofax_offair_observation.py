#!/usr/bin/env python3
"""Observe a partial off-air WAV through a full Decodium receiver (macOS BlackHole)."""
import argparse
import ctypes as c
import ctypes.util
import hashlib
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time

from run_hamdrm_two_app_echo_lab import Info, Params

parser = argparse.ArgumentParser()
parser.add_argument('wav', type=Path)
parser.add_argument('--profile', choices=('centered', 'wmo'), required=True)
options = parser.parse_args()
source = options.wav.resolve(strict=True)
if source.suffix.lower() != '.wav' or not 0 < source.stat().st_size < 100_000_000:
    raise SystemExit('expected bounded WAV input')
binary = Path('build/decodium').resolve(strict=True)
root = Path(tempfile.mkdtemp(prefix='decodium-radiofax-offair-'))
print('artifacts', root, flush=True)
raw = root / 'input.f32'
subprocess.run(['ffmpeg', '-v', 'error', '-i', str(source), '-ac', '2', '-ar', '48000',
                '-f', 'f32le', str(raw)], check=True, timeout=30)
if not 0 < raw.stat().st_size <= 240 * 48000 * 8 or raw.stat().st_size % 8:
    raise SystemExit('invalid converted audio')
(root / 'input.json').write_text(json.dumps({'file': str(source), 'sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'duration_seconds': raw.stat().st_size / (48000 * 8), 'profile': options.profile}, indent=2) + '\n')
env = os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
           DECODIUM_LAB_SETTINGS_DIR=str(root / 'settings'), DECODIUM_LAB_FAX_PARTIAL_SNAPSHOT='1',
           DECODIUM_LAB_FAX_DIAGNOSTICS='1')
profile = '576,512,300,1' if options.profile == 'centered' else '576,512,300,0,1'
args = [str(binary), '--test-mode', '--rig-name', 'OFFFAX' + secrets.token_hex(4),
        '--lab-no-cat', '--lab-audio-input', 'BlackHole 2ch', '--lab-audio-output', 'BlackHole 2ch',
        '--lab-reapply-ms', '500', '--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500',
        '--lab-sstv-fax-rx', profile, '--lab-sstv-rx-ready-file', str(root/'ready'),
        '--lab-sstv-rx-image-file', str(root/'received.png'), '--lab-quit-ms',
        str(int(raw.stat().st_size / (48000 * 8) * 1000) + 30000)]
process = None
try:
    with (root/'rx.log').open('wb') as log:
        process = subprocess.Popen(args, env=env, stdout=log, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + 25
    while not (root/'ready').exists():
        if process.poll() is not None or time.monotonic() > deadline: raise RuntimeError('receiver not ready')
        time.sleep(.1)
    if json.loads((root/'ready').read_text()).get('sourceDevice') != 'BlackHole 2ch':
        raise RuntimeError('incorrect input device')
    pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
    pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
    pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.c_void_p, c.POINTER(Params), c.c_double, c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
    pa.Pa_WriteStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
    if pa.Pa_Initialize(): raise RuntimeError('PortAudio initialization failed')
    device = next(i for i in range(pa.Pa_GetDeviceCount()) if pa.Pa_GetDeviceInfo(i).contents.name == b'BlackHole 2ch')
    params = Params(device, 2, 1, .05, None)
    stream = c.c_void_p()
    if pa.Pa_OpenStream(c.byref(stream), None, c.byref(params), 48000, 1024, 0, None, None):
        raise RuntimeError('cannot open output')
    if pa.Pa_StartStream(stream): raise RuntimeError('cannot start output')
    with raw.open('rb') as capture:
        while chunk := capture.read(8192 * 8):
            data = c.create_string_buffer(chunk)
            if pa.Pa_WriteStream(stream, data, len(chunk)//8): raise RuntimeError('audio playback failed')
    pa.Pa_StopStream(stream); pa.Pa_CloseStream(stream); pa.Pa_Terminate()
    if process.wait(timeout=35): raise RuntimeError('receiver exited with failure')
    lines = [json.loads(line.split(' RX diagnostics ', 1)[1]) for line in (root/'rx.log').read_text(errors='replace').splitlines() if ' RX diagnostics ' in line]
    if not lines or any(d.get('sourceDevice') != 'BlackHole 2ch' or d.get('discontinuities') != 0
                        or d.get('droppedChunks') != 0 or d.get('processingFailures') != 0 for d in lines):
        raise RuntimeError('RX diagnostics absent or audio continuity failed')
    saved = (root/'received.png').exists()
    result = {'acquisition_id': max(d.get('imageAcquisitionId', 0) for d in lines),
              'max_lines': max(d.get('imageLinesPublished', 0) for d in lines),
              'stop_detected': any(d.get('imageRadiofaxStopDetected') for d in lines),
              'snapshot_saved': saved, 'diagnostic_count': len(lines)}
    (root/'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print('OBSERVATION', json.dumps(result), 'artifacts', root, flush=True)
    if saved != (result['acquisition_id'] != 0 and result['max_lines'] > 0):
        raise RuntimeError('snapshot inconsistent with observed acquisition')
finally:
    if process and process.poll() is None:
        process.kill(); process.wait()
