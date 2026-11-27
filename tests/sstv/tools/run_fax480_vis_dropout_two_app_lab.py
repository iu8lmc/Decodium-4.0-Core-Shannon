#!/usr/bin/env python3
"""Two FAX480 apps with a reproducible VIS-bit dropout on virtual audio."""
import ctypes as c
import ctypes.util
import array
import json
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
variant = sys.argv[1]
root = Path(tempfile.mkdtemp(prefix='decodium-fax480-vis-dropout-'+variant+'-'))
pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.POINTER(Params), c.c_void_p,
                             c.c_double, c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
pa.Pa_ReadStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
pa.Pa_WriteStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
if pa.Pa_Initialize():
    raise RuntimeError('PortAudio initialisation failed')
input_stream, output_stream = c.c_void_p(), c.c_void_p()
started_input = started_output = False
stop = threading.Event()
errors = []
stats = {'frames': 0, 'first_active': None, 'spike_frames': 0,
         'muted_frames': 0, 'muted_active_frames': 0, 'peak': 0.0}
worker = None

def verify_interruption():
    source, altered = array.array('f'), array.array('f')
    source.frombytes((root/'source.f32').read_bytes())
    altered.frombytes((root/'relay.f32').read_bytes())
    if sys.byteorder != 'little':
        source.byteswap(); altered.byteswap()
    first = stats['first_active']
    start = first + 67080 if first is not None else -1
    if start < 0 or len(source) < 2*(start+480) or len(altered) < 2*(start+480):
        raise RuntimeError('relay header recording incomplete; artifacts '+str(root))
    samples = source[2*start:2*(start+480):2]
    def energy(frequency):
        coefficient = 2 * math.cos(2 * math.pi * frequency / 48000)
        a = b = 0.0
        for sample in samples:
            a, b = sample + coefficient*a - b, a
        return a*a + b*b - coefficient*a*b
    powers = {frequency: energy(frequency) for frequency in (1100, 1200, 1300, 1900)}
    if powers[1100] < 100 or powers[1100] <= 2*max(powers[f] for f in (1200,1300,1900)):
        raise RuntimeError('dropout did not fall inside a 1100 Hz VIS bit; artifacts '+str(root))
    if any(value != 0.0 for value in altered[2*start:2*(start+480)]):
        raise RuntimeError('relay dropout contains nonzero samples; artifacts '+str(root))
    # Check the actual spike in both channels as well as the counters.
    for frame in range(24):
        relative = 67056 + frame
        expected = 0.25 * math.sin(2 * math.pi * 1200 * relative / 48000)
        if any(abs(altered[2*(first+relative)+channel] - expected) > 1e-6 for channel in (0,1)):
            raise RuntimeError('relay spike did not match the requested waveform; artifacts '+str(root))
    return powers

try:
    devices = {pa.Pa_GetDeviceInfo(i).contents.name.decode(): i for i in range(pa.Pa_GetDeviceCount())}
    if 'BlackHole 16ch' not in devices or 'BlackHole 2ch' not in devices:
        raise RuntimeError('both BlackHole devices are required')
    input_params = Params(devices['BlackHole 16ch'], 2, 1, 0.5, None)
    output_params = Params(devices['BlackHole 2ch'], 2, 1, 0.5, None)
    if pa.Pa_OpenStream(c.byref(input_stream), c.byref(input_params), None, 48000, 480, 0, None, None):
        raise RuntimeError('cannot open relay input')
    if pa.Pa_OpenStream(c.byref(output_stream), None, c.byref(output_params), 48000, 480, 0, None, None):
        raise RuntimeError('cannot open relay output')
    if pa.Pa_StartStream(input_stream): raise RuntimeError('cannot start relay input')
    started_input = True
    if pa.Pa_StartStream(output_stream): raise RuntimeError('cannot start relay output')
    started_output = True

    def relay():
        source, output = (c.c_float * 960)(), (c.c_float * 960)()
        index = 0
        try:
            with (root/'source.f32').open('xb') as original, (root/'relay.f32').open('xb') as altered:
                while not stop.is_set():
                    if pa.Pa_ReadStream(input_stream, source, 480):
                        errors.append('relay input overflow'); break
                    for frame in range(480):
                        values = (float(source[2*frame]), float(source[2*frame+1]))
                        if not all(math.isfinite(value) for value in values):
                            errors.append('non-finite input'); return
                        active = max(abs(value) for value in values) > 0.005
                        if active and stats['first_active'] is None: stats['first_active'] = index
                        relative = index - stats['first_active'] if stats['first_active'] is not None else -1
                        # 750 ms VOX + 640 ms VIS preamble + 7 ms into bit 0.
                        # A 0.5 ms tone spike precedes a 10 ms dropout.
                        spike = 67056 <= relative < 67080
                        mute = 67080 <= relative < 67560
                        stats['spike_frames'] += spike
                        stats['muted_frames'] += mute
                        stats['muted_active_frames'] += mute and active
                        for channel in (0, 1):
                            value = (0.25 * math.sin(2 * math.pi * 1200 * relative / 48000)
                                     if spike else (0.0 if mute else 0.8 * values[channel]))
                            if abs(value) > 1.0:
                                errors.append('clipped relay'); return
                            stats['peak'] = max(stats['peak'], abs(value))
                            output[2*frame+channel] = value
                        index += 1
                    if pa.Pa_WriteStream(output_stream, output, 480):
                        errors.append('relay output underflow'); break
                    # Only the first five seconds of active signal are needed
                    # to audit the header; do not retain the full image audio.
                    if stats['first_active'] is None or index - stats['first_active'] <= 240480:
                        original.write(bytes(source)); altered.write(bytes(output))
                    stats['frames'] += 480
        except OSError as error:
            errors.append(str(error))

    worker = threading.Thread(target=relay, daemon=True)
    worker.start()
    env = os.environ.copy()
    env['DECODIUM_LAB_FAX480_TX_OUTPUT'] = 'BlackHole 16ch'
    env['DECODIUM_LAB_SSTV_VOX_PREKEY_MS'] = '750'
    run = subprocess.run([sys.executable, 'tests/sstv/tools/run_fax480_two_app_lab.py',
                          'libsstv-vis-480', variant], env=env, text=True,
                         capture_output=True, timeout=180)
    (root/'apps.log').write_text(run.stdout + run.stderr)
    stop.set(); worker.join(timeout=5)
    if worker.is_alive(): raise RuntimeError('relay did not stop; artifacts '+str(root))
    (root/'relay.json').write_text(json.dumps({'stats': stats, 'errors': errors}, indent=2)+'\n')
    if errors or stats['spike_frames'] != 24 or stats['muted_frames'] != 480 or stats['muted_active_frames'] < 400:
        raise RuntimeError('relay interruption not verified: '+repr(errors)+'; artifacts '+str(root))
    powers = verify_interruption()
    (root/'relay.json').write_text(json.dumps({'stats': stats, 'errors': errors,
                                               'source_dropout_tone_energy': powers}, indent=2)+'\n')
    if run.returncode:
        raise RuntimeError('two-app transfer failed; artifacts '+str(root))
    print(run.stdout.strip(), 'relay artifacts', root, flush=True)
finally:
    stop.set()
    if worker: worker.join(timeout=5)
    if started_input: pa.Pa_StopStream(input_stream)
    if started_output: pa.Pa_StopStream(output_stream)
    if input_stream.value: pa.Pa_CloseStream(input_stream)
    if output_stream.value: pa.Pa_CloseStream(output_stream)
    pa.Pa_Terminate()
