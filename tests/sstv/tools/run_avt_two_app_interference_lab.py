#!/usr/bin/env python3
"""Two AVT apps over a bounded BlackHole relay with a continuous test tone."""
import ctypes as c
import ctypes.util
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading

from run_hamdrm_two_app_echo_lab import Info, Params

if len(sys.argv) not in (4, 5, 6) or sys.argv[1] not in ('a', 'b'):
    raise SystemExit('expected a|b image-path 0|1 (notch) [tone-hz [mode]]')
variant, image, notch = sys.argv[1], Path(sys.argv[2]).resolve(strict=True), sys.argv[3]
mute_words = os.environ.get('DECODIUM_LAB_AVT_FIRST_FRAME_MUTE', '0')
if mute_words not in ('0', '1', '2'):
    raise SystemExit('countdown mute must be 0, 1 or 2 words')
mute_countdown = int(mute_words)
mute_length = 5280 + (mute_countdown - 1) * 7969 if mute_countdown else 0
if mute_countdown and notch != '0':
    raise SystemExit('countdown interruption requires the notch disabled')
if notch not in ('0', '1'): raise SystemExit('notch must be 0 or 1')
frequency = int(sys.argv[4]) if len(sys.argv) >= 5 else 1700
mode = sys.argv[5] if len(sys.argv) == 6 else 'avt-24-qrm'
allowed_modes = {f'avt-{duration}-{variant}' for duration in ('24', '90', '94')
                 for variant in ('qrm', 'narrow-qrm')}
if not 900 <= frequency <= 2500 or mode not in allowed_modes:
    raise SystemExit('unsupported frequency or mode')
root = Path(tempfile.mkdtemp(prefix='decodium-avt-live-interference-'))
pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.c_void_p, c.c_void_p,
                             c.c_double, c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
pa.Pa_ReadStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
pa.Pa_WriteStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
if pa.Pa_Initialize(): raise RuntimeError('PortAudio initialization failed')
input_stream, output_stream = c.c_void_p(), c.c_void_p()
started_input = started_output = False
stop = threading.Event()
errors = []
stats = {'frames': 0, 'active_frames': 0, 'peak': 0.0, 'muted_frames': 0,
         'muted_active_frames': 0, 'first_active_frame': None}
worker = None
try:
    devices = {pa.Pa_GetDeviceInfo(i).contents.name.decode(): i for i in range(pa.Pa_GetDeviceCount())}
    if 'BlackHole 16ch' not in devices or 'BlackHole 2ch' not in devices:
        raise RuntimeError('both BlackHole devices are required')
    # A larger buffering allowance protects this Python relay from scheduling
    # stalls during long transmissions. PortAudio errors still fail the run.
    in_params = Params(devices['BlackHole 16ch'], 2, 1, 0.5, None)
    out_params = Params(devices['BlackHole 2ch'], 2, 1, 0.5, None)
    if pa.Pa_OpenStream(c.byref(input_stream), c.byref(in_params), None, 48000, 480, 0, None, None):
        raise RuntimeError('cannot open relay input')
    if pa.Pa_OpenStream(c.byref(output_stream), None, c.byref(out_params), 48000, 480, 0, None, None):
        raise RuntimeError('cannot open relay output')
    if pa.Pa_StartStream(input_stream): raise RuntimeError('cannot start relay input')
    started_input = True
    if pa.Pa_StartStream(output_stream): raise RuntimeError('cannot start relay output')
    started_output = True

    def relay():
        source, output = (c.c_float * 960)(), (c.c_float * 960)()
        index = 0
        try:
            with (root / 'relay.f32').open('xb') as capture, \
                    (root / 'source.f32' if mute_countdown else Path(os.devnull)).open('wb') as original:
                while not stop.is_set():
                    if pa.Pa_ReadStream(input_stream, source, 480):
                        errors.append('input overflow or failure'); break
                    for frame in range(480):
                        values = (float(source[2 * frame]), float(source[2 * frame + 1]))
                        if not all(math.isfinite(value) for value in values):
                            errors.append('non-finite input'); return
                        active = max(abs(value) for value in values) > 0.005
                        if active and stats['first_active_frame'] is None:
                            stats['first_active_frame'] = index
                        relative = index - stats['first_active_frame'] if stats['first_active_frame'] is not None else -1
                        # The laboratory TX includes a 750 ms VOX preamble
                        # followed by three 910 ms VIS frames. Mute 110 ms
                        # inside the first countdown word; optionally extend
                        # through the second. Captures are checked independently.
                        mute = mute_countdown and 168000 <= relative < 168000 + mute_length
                        tone = 0.0 if mute_countdown else 0.1 * math.sin(2 * math.pi * frequency * index / 48000)
                        index += 1
                        stats['muted_frames'] += mute
                        stats['muted_active_frames'] += mute and active
                        for channel in (0, 1):
                            value = values[channel]
                            mixed = 0.0 if mute else 0.8 * (value + tone)
                            if abs(mixed) > 1:
                                errors.append('clipped relay'); return
                            stats['peak'] = max(stats['peak'], abs(mixed))
                            output[2 * frame + channel] = mixed
                        stats['active_frames'] += active
                    if pa.Pa_WriteStream(output_stream, output, 480):
                        errors.append('output underflow or failure'); break
                    capture.write(bytes(output))
                    original.write(bytes(source))
                    stats['frames'] += 480
        except OSError as error:
            errors.append(str(error))

    worker = threading.Thread(target=relay, daemon=True)
    worker.start()
    env = os.environ.copy()
    env['DECODIUM_LAB_AVT_TX_OUTPUT'] = 'BlackHole 16ch'
    # The relay already retains the mixed signal; avoid a redundant input
    # stream in the child that can overflow under load.
    env['DECODIUM_LAB_AVT_NO_CAPTURE'] = '1'
    if notch == '1': env['DECODIUM_LAB_SSTV_NOTCH_HZ'] = str(frequency)
    else: env.pop('DECODIUM_LAB_SSTV_NOTCH_HZ', None)
    run = subprocess.run([sys.executable, 'tests/sstv/tools/run_avt_two_app_lab.py',
                          mode, variant, str(image)],
                         env=env, text=True, capture_output=True,
                         timeout=100 if mode.startswith('avt-24-') else 170)
    (root / 'apps.log').write_text(run.stdout + run.stderr)
    stop.set(); worker.join(timeout=5)
    if worker.is_alive(): raise RuntimeError('relay did not stop; artifacts '+str(root))
    (root / 'relay.txt').write_text(repr({'frequency_hz': frequency, 'mode': mode, **stats, 'errors': errors}))
    if mute_countdown and (stats['muted_frames'] != mute_length
                           or stats['muted_active_frames'] < mute_length - 480):
        raise RuntimeError('countdown words were not muted as expected; artifacts '+str(root))
    if errors or stats['active_frames'] < 4800:
        raise RuntimeError('relay failed: '+repr(errors)+'; artifacts '+str(root))
    if run.returncode:
        raise RuntimeError('two-app transfer failed; artifacts '+str(root))
    if mute_countdown:
        tool = 'tests/sstv/tools/analyze_avt_countdown_audio.py'
        suffix = ['--narrow'] if mode.endswith('narrow-qrm') else []
        analyses = {}
        for label, path in (('source', root/'source.f32'), ('relay', root/'relay.f32')):
            analyses[label] = json.loads(subprocess.check_output(
                [sys.executable, tool, str(path), *suffix], text=True, timeout=30))
        (root/'countdown-analysis.json').write_text(json.dumps(analyses, indent=2)+'\n')
        if (analyses['source']['first_counter'] != 0 or analyses['relay']['first_counter'] != mute_countdown
                or analyses['relay']['longest_run_allowing_single_gaps'] < 5):
            raise RuntimeError('recording did not show expected countdown loss; artifacts '+str(root))
    print(run.stdout.strip(), 'relay artifacts', root, 'peak', stats['peak'], flush=True)
finally:
    stop.set()
    if worker: worker.join(timeout=5)
    if started_input: pa.Pa_StopStream(input_stream)
    if started_output: pa.Pa_StopStream(output_stream)
    if input_stream: pa.Pa_CloseStream(input_stream)
    if output_stream: pa.Pa_CloseStream(output_stream)
    pa.Pa_Terminate()
