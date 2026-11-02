#!/usr/bin/env python3
"""Two Decodium processes with a bounded 16ch-to-2ch BlackHole echo relay."""
import ctypes as c
import ctypes.util
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import threading


class Info(c.Structure):
    _fields_ = [('version', c.c_int), ('name', c.c_char_p), ('host', c.c_int),
                ('inputs', c.c_int), ('outputs', c.c_int), ('lowin', c.c_double),
                ('lowout', c.c_double), ('highin', c.c_double), ('highout', c.c_double),
                ('rate', c.c_double)]


class Params(c.Structure):
    _fields_ = [('device', c.c_int), ('channels', c.c_int), ('format', c.c_ulong),
                ('latency', c.c_double), ('hostinfo', c.c_void_p)]


def main():
    if (len(sys.argv) not in (3, 4, 5) or sys.argv[1] not in ('a', 'b')
        or sys.argv[2] not in ('short', 'long')
        or (len(sys.argv) == 5 and sys.argv[4] not in ('off', 'flat'))):
        raise SystemExit('expected a|b short|long [echo-gain [off|flat]]')
    variant, depth = sys.argv[1:3]
    profile = os.environ.get('DECODIUM_LAB_ECHO_PROFILE',
                             f'hamdrm-A-2500-normal-16-QAM-{depth}')
    if not re.fullmatch(r'hamdrm-[ABE]-(2300|2500)-(high|normal)-(4|16|64)-QAM-' + depth, profile):
        raise SystemExit('invalid laboratory profile or interleaver mismatch')
    echo = float(sys.argv[3]) if len(sys.argv) >= 4 else 0.27
    if not math.isfinite(echo) or not 0.0 <= echo <= 0.5:
        raise SystemExit('invalid echo gain')
    noise_amplitude = float(os.environ.get('DECODIUM_LAB_NOISE_AMPLITUDE', '0'))
    tone_amplitude = float(os.environ.get('DECODIUM_LAB_TONE_AMPLITUDE', '0'))
    if (not math.isfinite(noise_amplitude) or not 0 <= noise_amplitude <= 0.1
        or not math.isfinite(tone_amplitude) or not 0 <= tone_amplitude <= 0.1):
        raise SystemExit('invalid noise or tone amplitude')
    selection = sys.argv[4] if len(sys.argv) == 5 else 'fallback'
    fallback_enabled = selection == 'fallback'
    root = Path(tempfile.mkdtemp(prefix='decodium-hamdrm-two-app-echo-'))
    pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
    pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
    pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.c_void_p, c.c_void_p,
                                 c.c_double, c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
    pa.Pa_ReadStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
    pa.Pa_WriteStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
    if pa.Pa_Initialize():
        raise RuntimeError('PortAudio initialization failed')
    input_stream, output_stream = c.c_void_p(), c.c_void_p()
    started_input = started_output = False
    stop = threading.Event()
    errors = []
    stats = {'frames': 0, 'active_frames': 0, 'peak': 0.0}
    worker = None
    try:
        devices = {pa.Pa_GetDeviceInfo(i).contents.name.decode(): i for i in range(pa.Pa_GetDeviceCount())}
        if 'BlackHole 16ch' not in devices or 'BlackHole 2ch' not in devices:
            raise RuntimeError('both BlackHole devices are required')
        in_params = Params(devices['BlackHole 16ch'], 2, 1, 0.05, None)
        out_params = Params(devices['BlackHole 2ch'], 2, 1, 0.05, None)
        if pa.Pa_OpenStream(c.byref(input_stream), c.byref(in_params), None, 48000, 480, 0, None, None):
            raise RuntimeError('cannot open BlackHole 16ch input')
        if pa.Pa_OpenStream(c.byref(output_stream), None, c.byref(out_params), 48000, 480, 0, None, None):
            raise RuntimeError('cannot open BlackHole 2ch output')
        if pa.Pa_StartStream(input_stream): raise RuntimeError('cannot start relay input')
        started_input = True
        if pa.Pa_StartStream(output_stream): raise RuntimeError('cannot start relay output')
        started_output = True

        def relay():
            source, output = (c.c_float * 960)(), (c.c_float * 960)()
            delayed = [0.0] * 96
            position = 0
            noise_state, sample_index = 0x67901a2b, 0
            try:
                capture = (root / 'relay.f32').open('xb') if os.environ.get('DECODIUM_LAB_ECHO_CAPTURE') == '1' else None
                try:
                    while not stop.is_set():
                        if pa.Pa_ReadStream(input_stream, source, 480):
                            errors.append('input overflow or failure'); break
                        for frame in range(480):
                            active = False
                            noise_state = (noise_state * 1664525 + 1013904223) & 0xffffffff
                            noise = noise_amplitude * (2.0 * (noise_state >> 8) / (1 << 24) - 1.0)
                            tone = tone_amplitude * math.sin(2.0 * math.pi * 1700.0 * sample_index / 48000.0)
                            sample_index += 1
                            for channel in (0, 1):
                                value = float(source[2*frame+channel])
                                if not math.isfinite(value):
                                    errors.append('non-finite input'); return
                                active |= abs(value) > 0.005
                                slot = 2*position+channel
                                mixed = 0.5 * (value + echo * delayed[slot]) + noise + tone
                                delayed[slot] = value
                                if abs(mixed) > 1.0:
                                    errors.append('clipped relay'); return
                                stats['peak'] = max(stats['peak'], abs(mixed))
                                output[2*frame+channel] = mixed
                            stats['active_frames'] += active
                            position = (position + 1) % 48
                        if pa.Pa_WriteStream(output_stream, output, 480):
                            errors.append('output underflow or failure'); break
                        if capture: capture.write(bytes(output))
                        stats['frames'] += 480
                finally:
                    if capture: capture.close()
            except OSError as error:
                errors.append('relay capture failed: ' + str(error))

        worker = threading.Thread(target=relay, daemon=True)
        worker.start()
        env = os.environ.copy()
        env.pop('DECODIUM_LAB_HAMDRM_FLAT_FALLBACK', None)
        env.pop('DECODIUM_LAB_HAMDRM_SELECTIVE', None)
        env.update(DECODIUM_LAB_HAMDRM_TX_OUTPUT='BlackHole 16ch',
                   DECODIUM_HAMDRM_RX_TRACE='1',
                   DECODIUM_HAMDRM_LAB_PROFILE=profile)
        if selection != 'flat':
            env['DECODIUM_LAB_HAMDRM_SELECTIVE'] = '1'
        if fallback_enabled:
            env['DECODIUM_LAB_HAMDRM_FLAT_FALLBACK'] = '1'
        duration_ms = int(env.get('DECODIUM_LAB_HAMDRM_DURATION_MS', '0'))
        if duration_ms and not 35000 <= duration_ms <= 600000:
            raise RuntimeError('invalid laboratory session duration')
        timeout = max(70.0, duration_ms / 1000.0 + 30.0)
        run = subprocess.run([sys.executable, 'tests/sstv/tools/run_hamdrm_two_app_lab.py', variant],
                             env=env, capture_output=True, text=True, timeout=timeout)
        (root / 'two-app.log').write_text(run.stdout + run.stderr)
        stop.set(); worker.join(timeout=5)
        if worker.is_alive(): raise RuntimeError('audio relay did not stop; artifacts ' + str(root))
        (root / 'relay.txt').write_text(repr({'echo_gain': echo, 'noise_amplitude': noise_amplitude,
                                               'tone_amplitude': tone_amplitude, **stats, 'errors': errors}))
        if errors or stats['active_frames'] < 4800:
            raise RuntimeError('relay failed or carried too little audio: ' + repr(errors) + '; artifacts ' + str(root))
        if selection == 'off':
            match = re.search(r'^RuntimeError: HAMDRM RX did not complete; artifacts (\S+)$',
                              run.stderr, re.MULTILINE)
            if run.returncode == 0 or not match:
                raise RuntimeError('disabled control did not fail as expected; artifacts ' + str(root))
            app_root = Path(match.group(1))
            rxlog = (app_root / 'rx.log').read_text(errors='replace')
            txlog = (app_root / 'tx.log').read_text(errors='replace')
            failures = [int(value) for value in re.findall(r'packet failures\s+(\d+)', rxlog)]
            if ('[LAB][HAMDRM] TX state "Completed"' not in txlog
                or (app_root / 'received.jpg').exists() or max(failures, default=0) == 0
                or '[LAB][HAMDRM] experimental CRC-gated flat packet fallback' in rxlog):
                raise RuntimeError('disabled control failed for a different reason; artifacts ' + str(root))
            print('PASS expected incomplete control', variant, depth, 'packet failures', max(failures),
                  'active frames', stats['active_frames'], 'peak', stats['peak'],
                  'app artifacts', app_root, 'relay artifacts', root, flush=True)
            return
        if run.returncode:
            raise RuntimeError('two-app transfer failed; artifacts ' + str(root))
        match = re.search(r'artifacts (\S+)', run.stdout)
        if not match: raise RuntimeError('missing application artifacts; ' + str(root))
        rxlog = (Path(match.group(1)) / 'rx.log').read_text(errors='replace')
        if env.get('DECODIUM_LAB_CHECK_COMPLETED_INBOX') == '1':
            saved = rxlog.find('[LAB][HAMDRM] RX object saved')
            snapshots = [json.loads(line.split('[LAB][HAMDRM] RX objects ', 1)[1])
                         for line in rxlog[saved:].splitlines()
                         if '[LAB][HAMDRM] RX objects ' in line]
            if (saved < 0 or not snapshots
                or '[HAMDRM RX fractional timing] groups' not in rxlog[saved:]
                or any(item.get('error') or not any(row.get('state') == 'complete'
                                                    for row in item.get('inbox', []))
                       for item in snapshots)):
                raise RuntimeError('completed inbox did not remain error-free; artifacts ' + str(root))
        counts = [int(value) for value in re.findall(r'flat fallback packets\s+(\d+)', rxlog)]
        staged = [int(value) for value in re.findall(r'\[HAMDRM RX trace\][^\n]*multistage packets\s+(\d+)', rxlog)]
        early = [int(value) for value in re.findall(r'\[HAMDRM RX dual timing\] early groups\s+(\d+)', rxlog)]
        if (env.get('DECODIUM_LAB_HAMDRM_MULTISTAGE') == '1'
            and '[LAB][HAMDRM] experimental CRC-gated multistage packet retry' not in rxlog):
            raise RuntimeError('requested multistage retry was not enabled; artifacts ' + str(root))
        if (env.get('DECODIUM_LAB_HAMDRM_STRICT_MULTISTAGE') == '1'
            and '[LAB][HAMDRM] strict-pilot CRC-gated multistage packet retry' not in rxlog):
            raise RuntimeError('requested strict multistage retry was not enabled; artifacts ' + str(root))
        if (env.get('DECODIUM_LAB_HAMDRM_STRICT_ADAPTIVE') == '1'
            and '[LAB][HAMDRM] experimental adaptive estimator with strict pilot limits' not in rxlog):
            raise RuntimeError('requested strict adaptive estimator was not enabled; artifacts ' + str(root))
        if (env.get('DECODIUM_LAB_HAMDRM_DUAL_TIMING') == '1'
            and '[LAB][HAMDRM] experimental independent dual timing' not in rxlog):
            raise RuntimeError('requested dual timing was not enabled; artifacts ' + str(root))
        if (env.get('DECODIUM_LAB_HAMDRM_EARLY_ONLY') == '1'
            and '[LAB][HAMDRM] only early timing groups delivered' not in rxlog):
            raise RuntimeError('requested early-only delivery was not enabled; artifacts ' + str(root))
        if (env.get('DECODIUM_LAB_HAMDRM_FRACTIONAL_TIMING') == '1'
            and '[LAB][HAMDRM] experimental fractional timing candidate' not in rxlog):
            raise RuntimeError('requested fractional timing was not enabled; artifacts ' + str(root))
        if (env.get('DECODIUM_LAB_HAMDRM_FRACTIONAL_ONLY') == '1'
            and '[LAB][HAMDRM] only fractional timing groups delivered' not in rxlog):
            raise RuntimeError('requested fractional-only delivery was not enabled; artifacts ' + str(root))
        if (env.get('DECODIUM_LAB_HAMDRM_DUAL_TIMING') == '1'
            or env.get('DECODIUM_LAB_HAMDRM_STRICT_MULTISTAGE') == '1'):
            snapshots = [json.loads(line.split('[LAB][SSTV] RX diagnostics ', 1)[1])
                         for line in rxlog.splitlines()
                         if '[LAB][SSTV] RX diagnostics ' in line]
            if not snapshots or any(item.get('hamdrmRejectedChunks') != 0
                                    or item.get('hamdrmQueueOverflows') != 0
                                    for item in snapshots):
                raise RuntimeError('HAMDRM audio queue rejected or overflowed chunks; artifacts ' + str(root))
        if (env.get('DECODIUM_LAB_EXPECT_MULTISTAGE') == '1'
            and max(staged, default=0) == 0):
            raise RuntimeError('no packets recovered by multistage retry; artifacts ' + str(root))
        if env.get('DECODIUM_LAB_EXPECT_EARLY') == '1' and max(early, default=0) == 0:
            raise RuntimeError('no groups recovered by early timing; artifacts ' + str(root))
        print('PASS two-app echo', variant, depth, 'lab fallback switch', fallback_enabled, 'active frames', stats['active_frames'],
              'peak', stats['peak'], 'fallback packets', max(counts, default=0),
              'multistage packets', max(staged, default=0),
              'early groups', max(early, default=0),
              'app artifacts', match.group(1), 'relay artifacts', root, flush=True)
    finally:
        stop.set()
        if worker: worker.join(timeout=5)
        if started_input: pa.Pa_StopStream(input_stream)
        if started_output: pa.Pa_StopStream(output_stream)
        if input_stream: pa.Pa_CloseStream(input_stream)
        if output_stream: pa.Pa_CloseStream(output_stream)
        pa.Pa_Terminate()


if __name__ == '__main__':
    main()
