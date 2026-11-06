#!/usr/bin/env python3
"""Exercise extended-BSR repair with adapted TRXAMADRM and Decodium over BlackHole.

Run on macOS from the repository root with prepared, hash-pinned lab binaries.
The sender's lab patch must pace its audio ring buffer. Outputs are retained.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import pty
import secrets
import select
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('variant', choices=('a', 'b'))
parser.add_argument('transmitter', type=Path)
parser.add_argument('receiver', type=Path)
parser.add_argument('lowpass', type=Path)
parser.add_argument('--tx-sha256', required=True)
parser.add_argument('--rx-sha256', required=True)
args = parser.parse_args()

def verified(path, digest):
    path = path.resolve(strict=True)
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest.lower():
        raise RuntimeError('SHA-256 mismatch: ' + str(path))
    return path

txbin = verified(args.transmitter, args.tx_sha256)
rxbin = verified(args.receiver, args.rx_sha256)
lowpass = verified(args.lowpass, '5365e6ab619e2a0c26ae47bd191f09d1531b8d609ac527ee8eb1f9a7a3b8df27')
decodium = Path('build/decodium').resolve(strict=True)
source = Path('tests/sstv/fixtures/qsstv-image-' + args.variant + '.jpg').read_bytes() + bytes(2000)

class Info(ctypes.Structure):
    _fields_ = [('version', ctypes.c_int), ('name', ctypes.c_char_p), ('host', ctypes.c_int),
                ('inputs', ctypes.c_int), ('outputs', ctypes.c_int), ('lowin', ctypes.c_double),
                ('lowout', ctypes.c_double), ('highin', ctypes.c_double),
                ('highout', ctypes.c_double), ('rate', ctypes.c_double)]

pa = ctypes.CDLL('/opt/homebrew/lib/libportaudio.dylib')
pa.Pa_GetDeviceInfo.restype = ctypes.POINTER(Info)
if pa.Pa_Initialize():
    raise RuntimeError('PortAudio initialization failed')
try:
    devices = [pa.Pa_GetDeviceInfo(i).contents for i in range(pa.Pa_GetDeviceCount())]
    inputs = [d.name for d in devices if d.inputs > 1]
    outputs = [d.name for d in devices if d.outputs > 1]
    if inputs.count(b'BlackHole 2ch') != 1 or outputs.count(b'BlackHole 2ch') != 1:
        raise RuntimeError('BlackHole 2ch must be a unique input and output')
    input_index, output_index = inputs.index(b'BlackHole 2ch'), outputs.index(b'BlackHole 2ch')
finally:
    pa.Pa_Terminate()

root = Path(tempfile.mkdtemp(prefix='decodium-trxamadrm-repair-' + args.variant + '-'))
for directory in ('pics', 'picsrx', 'incomplete', 'test'):
    (root / directory).mkdir()
(root / 'pics/FrhBlthiHM.jpg').write_bytes(source)
(root / 'lowpass.dat').write_bytes(lowpass.read_bytes())
(root / 'txamadrm.ini').write_text(
    '[sounddevices]\ndevin=%d\ndevout=%d\n[mode]\nrobustness=2\nBW=1\n'
    'intlvldepth=0\nqam=16\nprotlev=0\n[callsign]\nmycall=LABTX\n[ptt]\npttkind=none\n'
    % (input_index, output_index))
home = str(Path.home()).replace('\\', '\\\\').replace('"', '\\"')
profile = root / 'sandbox.sb'
profile.write_text('(version 1)\n(allow default)\n'
                   '(deny network-outbound (remote ip "*:*") )\n'
                   '(deny network-inbound (local ip "*:*") )\n'
                   '(deny file-read* file-write* (subpath "' + home + '"))\n')
sandbox = ['sandbox-exec', '-f', str(profile)]
env = os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
           DECODIUM_LAB_SETTINGS_DIR=str(root / 'settings'), DECODIUM_LAB_REPAIR_LIVE='a',
           DECODIUM_LAB_REPAIR_AUDIO_LOSS='1', DECODIUM_LAB_REPAIR_INITIAL=str(root / 'initial'),
           DECODIUM_LAB_REPAIR_READY=str(root / 'repair-ready'),
           DECODIUM_LAB_DROP_MOT_UNTIL=str(root / 'unlock'), DECODIUM_LAB_TRACE_MOT='1')
txenv = os.environ.copy()
txenv['DECODIUM_TRX_LAB_CONFIG'] = str(root / 'txamadrm.ini')
rxcmd = [str(decodium), '--test-mode', '--rig-name', 'REPAIR' + secrets.token_hex(3),
         '--lab-no-cat', '--lab-audio-input', 'BlackHole 2ch', '--lab-audio-output', 'BlackHole 2ch',
         '--lab-reapply-ms', '500', '--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500',
         '--lab-hamdrm', '--lab-hamdrm-profile', 'hamdrm-E-2500-high-16-QAM-long',
         '--lab-sstv-rx-ready-file', str(root / 'rx-ready'),
         '--lab-sstv-rx-image-file', str(root / 'received.jpg'), '--lab-quit-ms', '180000']
processes = []
masters = []

def launch(cmd, log, **kwargs):
    with (root / log).open('wb') as stream:
        process = subprocess.Popen(cmd, stdout=stream, stderr=subprocess.STDOUT, **kwargs)
    processes.append(process)
    return process

def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()

def wait(predicate, timeout, label):
    deadline = time.monotonic() + timeout
    while not predicate():
        if time.monotonic() > deadline:
            raise RuntimeError(label + ' timed out: ' + str(root))
        time.sleep(.2)

def log(name):
    path = root / name
    return path.read_text(errors='replace') if path.exists() else ''

def sender():
    master, slave = pty.openpty()
    try:
        process = subprocess.Popen(sandbox + [str(txbin), './pics/FrhBlthiHM.jpg'],
                                   cwd=root, env=txenv, stdout=slave, stderr=subprocess.STDOUT)
    finally:
        os.close(slave)
    processes.append(process)
    masters.append(master)
    return process, master, bytearray()

def pump(master, data):
    if select.select([master], [], [], .1)[0]:
        try:
            data.extend(os.read(master, 8192))
        except OSError:
            pass

last_output = bytearray()
try:
    independent = launch(sandbox + [str(rxbin)], 'historical-rx.log', cwd=root)
    wait(lambda: 'LAB RX BlackHole 2ch ready' in log('historical-rx.log'), 20, 'historical RX')
    dec = launch(rxcmd, 'decodium-rx.log', env=env)
    wait(lambda: (root / 'rx-ready').exists(), 25, 'Decodium RX')
    if json.loads((root / 'rx-ready').read_text()).get('sourceDevice') != 'BlackHole 2ch':
        raise RuntimeError('wrong RX device')
    first, master, last_output = sender()
    deadline = time.monotonic() + 50
    while time.monotonic() < deadline:
        pump(master, last_output)
        if (b'GetDataGroup body situation 2nd iSegmCntBody 14' in last_output
                and '[LAB][HAMDRM] discarded initial MOT body segment 1' in log('decodium-rx.log')):
            break
        if first.poll() is not None:
            raise RuntimeError('initial transmitter exited')
    else:
        raise RuntimeError('initial partial image not established')
    time.sleep(5)
    stop(first)
    (root / 'initial-tx.log').write_bytes(last_output)
    if (root / 'received.jpg').exists():
        raise RuntimeError('image completed before repair')
    (root / 'initial').touch()
    wait(lambda: (root / 'picsrx/bsr.bin').exists(), 45, 'BSR reception')
    wait(lambda: (root / 'repair-ready').exists(), 35, 'Decodium RX restart')
    bsr = (root / 'picsrx/bsr.bin').read_text()
    if bsr.splitlines() != ['9297', 'H_OK', '154', '1', '-99', 'FrhBlthiHM.jpg', '21010']:
        raise RuntimeError('unexpected BSR: ' + repr(bsr))
    if (root / 'received.jpg').exists():
        raise RuntimeError('image completed before repair')
    stop(independent)
    second, master, last_output = sender()
    deadline = time.monotonic() + 75
    unlocked = False
    while time.monotonic() < deadline:
        pump(master, last_output)
        if not unlocked and b'valid bsr.bin file found bsrOK is 1' in last_output:
            if (root / 'received.jpg').exists():
                raise RuntimeError('image completed before selective phase')
            (root / 'unlock').touch()
            unlocked = True
        if unlocked and (root / 'received.jpg').exists():
            break
        if second.poll() is not None or dec.poll() is not None:
            raise RuntimeError('peer exited during repair')
    else:
        raise RuntimeError('repair did not complete')
    if (root / 'received.jpg').read_bytes() != source:
        raise RuntimeError('repaired bytes differ')
    print('PASS live TRXAMADRM BSR repair', args.variant, 'exact bytes', len(source), 'artifacts', root)
finally:
    (root / 'last-tx-output.log').write_bytes(last_output)
    for process in processes:
        stop(process)
    for master in masters:
        os.close(master)
    print('artifacts', root, flush=True)
