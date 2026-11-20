#!/usr/bin/env python3
"""Replay a muted real TX capture; exchange BSR and repair between two apps.

Requires macOS BlackHole 16ch, PortAudio, numpy, ffmpeg and a built Decodium.
The replayed initial waveform was previously captured from Decodium; the BSR
and selective repair are newly transmitted over virtual audio in each run.
"""
import ctypes as c
import ctypes.util
import json
import os
from pathlib import Path
import subprocess
import secrets
import sys
import tempfile
import time

import numpy as np
from run_hamdrm_two_app_echo_lab import Info, Params

if len(sys.argv) != 2 or sys.argv[1] not in ('a', 'b'):
    raise SystemExit('usage: run_hamdrm_rs_compat_audio_loss_lab.py a|b')
variant = sys.argv[1]
repo = Path(__file__).resolve().parents[3]
root = Path(tempfile.mkdtemp(prefix=f'decodium-rs-compat-audio-loss-{variant}-'))
app = (repo / 'build/decodium').resolve(strict=True)
fixtures = repo / 'tests/sstv/fixtures/easypal-2014-live'
original = (fixtures / f'reverse-{variant}.jpg').read_bytes()
source = root / f'{variant}.jpg'
# Match the source used to create the saved capture: RS2 header field 128.
source_bytes = original + bytes((64 - len(original) % 192) % 192)
source.write_bytes(source_bytes)
capture = fixtures / f'decodium-rs2-compat-{variant}.flac'
raw = root / 'capture.f32'
subprocess.run(['ffmpeg', '-v', 'error', '-i', str(capture), '-f', 'f32le',
                '-ar', '48000', '-ac', '2', str(raw)], check=True, timeout=30)
audio = np.fromfile(raw, dtype='<f4').reshape(-1, 2)
activity = np.flatnonzero(np.max(np.abs(audio), axis=1) > .005)
if not len(activity):
    raise RuntimeError('capture has no measurable audio')
first, last = int(activity[0]), int(activity[-1])
left, right = max(0, first - 48000), min(len(audio), last + 96000)
audio = audio[left:right].copy()
start = first - left + 6 * 48000
end = min(first - left + 16 * 48000, len(audio))
if end <= start:
    raise RuntimeError('capture is too short for the loss interval')
active = int(np.count_nonzero(np.max(np.abs(audio[start:end]), axis=1) > .005))
if active <= 48000:
    raise RuntimeError('loss interval did not contain enough active audio')
audio[start:end] = 0
raw.unlink()
print('muted_frames', end - start, 'muted_active_frames', active, 'artifacts', root, flush=True)

tx_initial, rx_gate = root / 'tx-initial', root / 'rx-gate'
ready, rx_ready, received = root / 'repair-ready', root / 'rx-ready', root / 'received.jpg'
processes = []
identity = secrets.token_hex(4)


def launch(role, extra, args):
    env = os.environ.copy()
    env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
               DECODIUM_LAB_SETTINGS_DIR=str(root / (role + '-settings')), **extra)
    common = ['--test-mode', '--lab-no-cat', '--lab-audio-input', 'BlackHole 16ch',
              '--lab-audio-output', 'BlackHole 16ch', '--lab-reapply-ms', '500',
              '--lab-hamdrm', '--lab-hamdrm-profile', 'hamdrm-B-2500-high-16-QAM-long',
              '--rig-name', 'LossReplay' + role + variant + identity]
    with (root / (role + '.log')).open('wb') as log:
        process = subprocess.Popen([str(app), *common, *args], env=env, stdout=log, stderr=subprocess.STDOUT)
    processes.append(process)


def wait_for(predicate, seconds, message):
    deadline = time.monotonic() + seconds
    while not predicate():
        if any(process.poll() is not None for process in processes) or time.monotonic() > deadline:
            raise RuntimeError(message + '; artifacts ' + str(root))
        time.sleep(.2)


def play_audio(samples):
    pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
    pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
    pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.c_void_p, c.c_void_p,
                                c.c_double, c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
    pa.Pa_WriteStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
    if pa.Pa_Initialize():
        raise RuntimeError('PortAudio initialization failed')
    stream = c.c_void_p()
    started = False
    try:
        ids = [i for i in range(pa.Pa_GetDeviceCount())
               if pa.Pa_GetDeviceInfo(i).contents.name == b'BlackHole 16ch'
               and pa.Pa_GetDeviceInfo(i).contents.outputs >= 2]
        if len(ids) != 1:
            raise RuntimeError('BlackHole 16ch output not uniquely available')
        params = Params(ids[0], 2, 1, .05, None)
        if pa.Pa_OpenStream(c.byref(stream), None, c.byref(params), 48000, 480, 0, None, None):
            raise RuntimeError('cannot open virtual audio')
        if pa.Pa_StartStream(stream):
            raise RuntimeError('cannot start virtual audio')
        started = True
        for offset in range(0, len(samples), 480):
            block = samples[offset:offset + 480]
            if pa.Pa_WriteStream(stream, block.ctypes.data_as(c.c_void_p), len(block)):
                raise RuntimeError('virtual audio output failed')
    finally:
        if started:
            pa.Pa_StopStream(stream)
        if stream:
            pa.Pa_CloseStream(stream)
        pa.Pa_Terminate()


try:
    launch('tx', {'DECODIUM_LAB_HAMDRM_RS_LEVEL': '2', 'DECODIUM_LAB_HAMDRM_QSSTV_COMPAT': '1',
                  'DECODIUM_LAB_REPAIR_VARIANT': variant, 'DECODIUM_LAB_REPAIR_AIR': '1',
                  'DECODIUM_LAB_REPAIR_INITIAL': str(tx_initial), 'DECODIUM_LAB_REPAIR_READY': str(ready),
                  'DECODIUM_LAB_REPAIR_AUDIO_LOSS': '1'},
           ['--lab-monitor-ms', '1800', '--lab-sstv-tx-ms', '4000', '--lab-sstv-image', str(source),
            '--lab-sstv-vox', '--lab-quit-ms', '300000'])
    wait_for(tx_initial.exists, 120, 'sender not ready')
    sender_log = (root / 'tx.log').read_text(errors='replace')
    if ('[Bridge] Audio capture started: BlackHole 16ch' not in sender_log
            or 'dev= BlackHole 16ch' not in sender_log):
        raise RuntimeError('sender did not use BlackHole 16ch for RX and TX; artifacts ' + str(root))
    launch('rx', {'DECODIUM_LAB_REPAIR_LIVE': variant, 'DECODIUM_LAB_REPAIR_AUDIO_LOSS': '1',
                  'DECODIUM_LAB_REPAIR_INITIAL': str(rx_gate), 'DECODIUM_LAB_REPAIR_READY': str(ready)},
           ['--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500', '--lab-sstv-rx-ready-file', str(rx_ready),
            '--lab-sstv-rx-image-file', str(received), '--lab-quit-ms', '180000'])
    wait_for(rx_ready.exists, 25, 'receiver not ready')
    if json.loads(rx_ready.read_text()).get('sourceDevice') != 'BlackHole 16ch':
        raise RuntimeError('receiver opened the wrong audio device; artifacts ' + str(root))
    play_audio(audio)
    time.sleep(2)
    rx_gate.touch()
    expected = source_bytes + bytes(1)
    wait_for(lambda: received.exists() and received.read_bytes() == expected, 100, 'repaired output missing')
    tx_log = (root / 'tx.log').read_text(errors='replace')
    rx_log = (root / 'rx.log').read_text(errors='replace')
    for log, marker in ((rx_log, '[LAB][REPAIR] live partial received'),
                        (rx_log, '[LAB][REPAIR] over-audio BSR TX started true'),
                        (rx_log, 'dev= BlackHole 16ch'),
                        (tx_log, '[LAB][REPAIR] over-audio BSR received'),
                        (tx_log, '[LAB][REPAIR] TX started true')):
        if marker not in log:
            raise RuntimeError('missing ' + marker + '; artifacts ' + str(root))
    print('PASS', variant, 'repaired_bytes', len(expected), 'muted_active_frames', active,
          'artifacts', root, flush=True)
finally:
    for process in processes:
        if process.poll() is None:
            process.terminate()
    for process in processes:
        if process.poll() is None:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
