#!/usr/bin/env python3
"""Run a hash-pinned TRXAMADRM 3.6 TX against Decodium via BlackHole on macOS."""
import argparse
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time

from sstv_lab_rx_diagnostics import clean_hamdrm_rx_diagnostics
from trxamadrm_hybrid_fixture import hybrid_fixture

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('variant', choices=('a', 'b'))
parser.add_argument('transmitter', type=Path)
parser.add_argument('--sha256', required=True, help='SHA-256 of the prepared laboratory transmitter')
parser.add_argument('--hybrid', action='store_true', help='transmit a synthetic hybrid reference')
parser.add_argument('--qam', type=int, choices=(4, 16, 64), default=16, help='MSC QAM constellation')
parser.add_argument('--interleaver', choices=('short', 'long'), default='long')
parser.add_argument('--protection', choices=('normal', 'high'), default='high')
args = parser.parse_args()
transmitter = args.transmitter.resolve(strict=True)
if hashlib.sha256(transmitter.read_bytes()).hexdigest() != args.sha256.lower():
    raise SystemExit('transmitter SHA-256 mismatch')
source = Path('tests/sstv/fixtures/qsstv-image-' + args.variant + '.jpg').resolve(strict=True)
binary = Path('build/decodium').resolve(strict=True)
root = Path(tempfile.mkdtemp(prefix='decodium-trxamadrm-live-' + args.variant + '-'))
(root / 'test').mkdir()
(root / 'pics').mkdir()
filename = ('de_TEST1-1-image-a.jpg' if args.variant == 'a' else 'de_TEST2-1-image-b.jpg') if args.hybrid else source.name
# Synthetic reference bytes; independent TX supplies MOT framing and audio.
expected = hybrid_fixture(args.variant, bool(os.environ.get('DECODIUM_TRX_LAB_FTP_PORT'))) if args.hybrid else source.read_bytes()
(root / 'pics' / filename).write_bytes(expected)
home = str(Path.home()).replace('\\', '\\\\').replace('"', '\\"')
sandbox = root / 'sandbox.sb'
sandbox.write_text('(version 1)\n(allow default)\n'
                   '(deny network-outbound (remote ip "*:*") )\n'
                   '(deny network-inbound (local ip "*:*") )\n'
                   '(deny file-read* file-write* (subpath "' + home + '"))\n')

class Info(c.Structure):
    _fields_ = [('version', c.c_int), ('name', c.c_char_p), ('host', c.c_int),
                ('inputs', c.c_int), ('outputs', c.c_int), ('lowin', c.c_double),
                ('lowout', c.c_double), ('highin', c.c_double), ('highout', c.c_double),
                ('rate', c.c_double)]

pa = c.CDLL('/opt/homebrew/lib/libportaudio.dylib')
pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
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

config = root / 'txamadrm.ini'
config.write_text('[sounddevices]\ndevin=%d\ndevout=%d\n[mode]\nrobustness=2\nBW=1\n'
                  'intlvldepth=%d\nqam=%d\nprotlev=%d\n[callsign]\nmycall=LABTX\n'
                  '[ptt]\npttkind=none\n' % (input_index, output_index,
                  args.interleaver == 'short', args.qam, args.protection == 'normal'))
env = os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
           DECODIUM_LAB_SETTINGS_DIR=str(root / 'settings'))
if args.hybrid:
    env['DECODIUM_LAB_HAMDRM_HYBRID'] = args.variant
    if os.environ.get('DECODIUM_TRX_LAB_FTP_PORT'):
        env['DECODIUM_LAB_HYBRID_FTP_PORT'] = os.environ['DECODIUM_TRX_LAB_FTP_PORT']
rx_args = [str(binary), '--test-mode', '--rig-name', 'TRX' + secrets.token_hex(3),
           '--lab-no-cat', '--lab-audio-input', 'BlackHole 2ch', '--lab-audio-output', 'BlackHole 2ch',
           '--lab-reapply-ms', '500', '--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500',
           '--lab-hamdrm', '--lab-hamdrm-profile',
           'hamdrm-E-2500-%s-%d-QAM-%s' % (args.protection, args.qam, args.interleaver),
           '--lab-sstv-rx-ready-file', str(root / 'ready'),
           '--lab-sstv-rx-image-file', str(root / 'received.jpg'), '--lab-quit-ms', '90000']
rx = tx = None
try:
    with (root / 'rx.log').open('wb') as log:
        rx = subprocess.Popen(rx_args, env=env, stdout=log, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + 25
    while not (root / 'ready').exists():
        if rx.poll() is not None or time.monotonic() > deadline:
            raise RuntimeError('RX not ready: ' + str(root))
        time.sleep(.1)
    if json.loads((root / 'ready').read_text()).get('sourceDevice') != 'BlackHole 2ch':
        raise RuntimeError('wrong RX device: ' + str(root))
    txenv = os.environ.copy()
    txenv['DECODIUM_TRX_LAB_CONFIG'] = str(config)
    with (root / 'tx.log').open('wb') as log:
        tx = subprocess.Popen(['sandbox-exec', '-f', str(sandbox), str(transmitter), './pics/' + filename], cwd=root, env=txenv,
                              stdout=log, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + 65
    while not (root / 'received.jpg').exists():
        if tx.poll() is not None or rx.poll() is not None or time.monotonic() > deadline:
            raise RuntimeError('transfer did not complete: ' + str(root))
        time.sleep(.25)
    if (root / 'received.jpg').read_bytes() != expected:
        raise RuntimeError('received bytes differ: ' + str(root))
    if args.hybrid and os.environ.get('DECODIUM_TRX_LAB_FTP_PORT'):
        deadline = time.monotonic() + 30
        while not (root / 'received.jpg.download').exists():
            if rx.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError('hybrid download did not complete: ' + str(root))
            time.sleep(.25)
        if (root / 'received.jpg.download').read_bytes() != source.read_bytes():
            raise RuntimeError('hybrid downloaded image differs: ' + str(root))
    time.sleep(2)
finally:
    for process in (tx, rx):
        if process and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()

txlog = (root / 'tx.log').read_text(errors='replace')
rxlog = (root / 'rx.log').read_text(errors='replace')
if 'opening BlackHole 2ch' not in txlog or not clean_hamdrm_rx_diagnostics(rxlog):
    raise RuntimeError('device or RX diagnostics failed: ' + str(root))
if args.hybrid and '[LAB][HAMDRM] hybrid reference saved' not in rxlog:
    raise RuntimeError('hybrid reference was not identified: ' + str(root))
if args.hybrid and os.environ.get('DECODIUM_TRX_LAB_FTP_PORT') and '[LAB][HAMDRM] hybrid FTP finished true' not in rxlog:
    raise RuntimeError('hybrid FTP did not report success: ' + str(root))
print('PASS live TRXAMADRM to Decodium', args.variant, 'exact bytes', len(expected),
      'artifacts', root, flush=True)
