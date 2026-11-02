#!/usr/bin/env python3
"""Replay a captured AVT signal with a controlled continuous tone on macOS."""
import array
import ctypes as c
import ctypes.util
import json
import math
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import time

partial_expected = sys.argv[-1:] == ['--expect-partial']
no_image_expected = sys.argv[-1:] == ['--expect-no-image']
argv = sys.argv[:-1] if partial_expected or no_image_expected else sys.argv
if len(argv) not in (6, 7):
    raise SystemExit('expected MODE capture.f32 reference.png tone_hz tone_peak [output_gain] [--expect-partial]')
mode, source, reference = argv[1], Path(argv[2]).resolve(strict=True), Path(argv[3]).resolve(strict=True)
if mode not in {'avt-24', *(f'avt-{duration}-{variant}' for duration in ('24','90','94')
                             for variant in ('qrm','narrow-qrm'))}:
    raise SystemExit('unsupported test mode')
frequency, amplitude = float(argv[4]), float(argv[5])
gain = float(argv[6]) if len(argv) == 7 else 0.8
if not math.isfinite(frequency) or not 100 <= frequency <= 4000 or not math.isfinite(amplitude) or not 0 <= amplitude <= 0.2:
    raise SystemExit('invalid interference parameters')
if not math.isfinite(gain) or not 0.1 <= gain <= 1.0: raise SystemExit('invalid output gain')
size = source.stat().st_size
if not size or size % 8 or size > 180 * 48000 * 8:
    raise SystemExit('invalid 48 kHz stereo float capture')
tone_start = float(os.environ.get('DECODIUM_LAB_AVT_TONE_START_S', '0'))
tone_duration_text = os.environ.get('DECODIUM_LAB_AVT_TONE_DURATION_S')
tone_duration = float(tone_duration_text) if tone_duration_text is not None else None
if (not math.isfinite(tone_start) or tone_start < 0 or tone_start >= size / 8 / 48000
    or (tone_duration is not None and (not math.isfinite(tone_duration) or tone_duration <= 0
        or tone_start + tone_duration > size / 8 / 48000))):
    raise SystemExit('invalid interference interval')
repo = Path.cwd()
binary = (repo / 'build/decodium').resolve(strict=True)
root = Path(tempfile.mkdtemp(prefix='decodium-avt-interference-'))
duration_ms = int(size / 8 / 48000 * 1000) + 18000
env = os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
           DECODIUM_LAB_SETTINGS_DIR=str(root / 'settings'))
args = [str(binary), '--test-mode', '--rig-name', 'AVTNOISE' + secrets.token_hex(4),
        '--lab-no-cat', '--lab-audio-input', 'BlackHole 2ch', '--lab-audio-output', 'BlackHole 2ch',
        '--lab-reapply-ms', '500', '--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500',
        '--lab-sstv-rx-mode', mode, '--lab-sstv-rx-ready-file', str(root / 'ready'),
        '--lab-sstv-rx-image-file', str(root / 'received.png'), '--lab-quit-ms', str(duration_ms)]

class Info(c.Structure):
    _fields_ = [('version', c.c_int), ('name', c.c_char_p), ('host', c.c_int), ('inputs', c.c_int),
                ('outputs', c.c_int), ('lowin', c.c_double), ('lowout', c.c_double),
                ('highin', c.c_double), ('highout', c.c_double), ('rate', c.c_double)]
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
    if ready.get('sourceDevice') != 'BlackHole 2ch': raise RuntimeError('wrong audio device; artifacts ' + str(root))
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
        raise RuntimeError('cannot open output')
    if pa.Pa_StartStream(stream): raise RuntimeError('cannot start output')
    sample_index = 0
    peak = 0.0
    active_power = 0.0
    active_count = 0
    with source.open('rb') as capture:
        while True:
            chunk = capture.read(8192 * 8)
            if not chunk: break
            samples = array.array('f'); samples.frombytes(chunk)
            if sys.byteorder != 'little': samples.byteswap()
            for i in range(0, len(samples), 2):
                if not math.isfinite(samples[i]) or not math.isfinite(samples[i + 1]):
                    raise RuntimeError('capture contains non-finite samples; artifacts ' + str(root))
                if abs(samples[i]) > 0.005:
                    active_power += samples[i] * samples[i]
                    active_count += 1
                t = sample_index / 48000.0
                active_tone = t >= tone_start and (tone_duration is None or t < tone_start + tone_duration)
                tone = amplitude * math.sin(2.0 * math.pi * frequency * t) if active_tone else 0.0
                sample_index += 1
                for channel in (0, 1):
                    value = gain * (samples[i + channel] + tone)
                    peak = max(peak, abs(value))
                    if abs(value) > 1.0: raise RuntimeError('interference mixture clipped; artifacts ' + str(root))
                    samples[i + channel] = value
            data = c.create_string_buffer(samples.tobytes())
            if pa.Pa_WriteStream(stream, data, len(samples) // 2): raise RuntimeError('audio playback failed')
    pa.Pa_StopStream(stream); pa.Pa_CloseStream(stream); pa.Pa_Terminate()
    if process.wait(timeout=30): raise RuntimeError('receiver exited with failure; artifacts ' + str(root))
    lines = [json.loads(line.split(' RX diagnostics ', 1)[1])
             for line in (root / 'rx.log').read_text(errors='replace').splitlines()
             if '[LAB][SSTV] RX diagnostics ' in line]
    requested_notch = os.environ.get('DECODIUM_LAB_SSTV_NOTCH_HZ')
    if requested_notch and ('[LAB][SSTV] manual interference notch ' + requested_notch + ' Q 300') not in (root / 'rx.log').read_text(errors='replace'):
        raise RuntimeError('requested manual notch was not enabled; artifacts ' + str(root))
    if (not lines or any(item.get('sourceDevice') != 'BlackHole 2ch'
                         or item.get('processingFailures') != 0
                         or item.get('discontinuities') != 0
                         or item.get('droppedChunks') != 0 for item in lines)):
        raise RuntimeError('receiver did not complete cleanly; artifacts ' + str(root))
    if partial_expected:
        last = lines[-1]
        if (last.get('imageComplete') or not last.get('imagePartial')
            or not last.get('imageAvailable') or last.get('imageMode') != mode
            or not 0 < last.get('imageCoverage', 0) < 1):
            raise RuntimeError('expected a received partial image; artifacts ' + str(root))
        print('PASS AVT partial replay', mode, 'coverage', last['imageCoverage'],
              'completed pixels', last.get('imageCompletedPixels'), 'artifacts', root, flush=True)
        sys.exit(0)
    if no_image_expected:
        if any(item.get('imageAvailable') or item.get('imageComplete') for item in lines):
            raise RuntimeError('unexpected image during no-image control; artifacts ' + str(root))
        if not any(item.get('visAvailable') and item.get('visMappedMode') == mode for item in lines):
            raise RuntimeError('expected VIS was not detected in no-image control; artifacts ' + str(root))
        print('PASS AVT no-image control', mode, 'no processing or audio errors; artifacts', root, flush=True)
        sys.exit(0)
    if not lines[-1].get('imageComplete') or lines[-1].get('imageMode') != mode:
        raise RuntimeError('receiver did not produce a complete image; artifacts ' + str(root))
    received = root / 'received.png'
    if not received.exists(): raise RuntimeError('RX image not saved; artifacts ' + str(root))
    active_rms = math.sqrt(active_power / active_count) if active_count else 0.0
    ratio_db = 20 * math.log10(active_rms / (amplitude / math.sqrt(2))) if amplitude and active_rms else None
    print('AVT replay', mode, 'tone_hz', frequency, 'tone_peak', amplitude,
          'tone_start_s', tone_start, 'tone_duration_s', tone_duration,
          'active_signal_rms', active_rms, 'nominal_sir_db', ratio_db,
          'output_gain', gain, 'mixture_peak', peak, 'discontinuities', lines[-1].get('discontinuities'),
          'artifacts', root, flush=True)
    subprocess.run([sys.executable, str(repo / 'tests/sstv/tools/compare_sstv_lab_images.py'),
                    str(reference), str(received), '--max-mae', '5', '--max-p95', '15'], check=True, timeout=30)
    print('PASS AVT replay', mode, 'tone', frequency, 'peak', amplitude,
          'mixture peak', peak, 'discontinuities', lines[-1].get('discontinuities'), 'artifacts', root, flush=True)
finally:
    if process and process.poll() is None:
        process.kill(); process.wait()
