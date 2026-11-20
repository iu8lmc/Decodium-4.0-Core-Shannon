#!/usr/bin/env python3
"""Exercise the historical TX's selective BSR reader on virtual audio."""
import argparse
import ctypes as c
import hashlib
import os
from pathlib import Path
import pty
import re
import select
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('variant', choices=('a', 'b'))
parser.add_argument('transmitter', type=Path)
parser.add_argument('bsr', type=Path, help='extended BSR received by RXAMADRM in the live A test')
parser.add_argument('--sha256', required=True)
args = parser.parse_args()
transmitter = args.transmitter.resolve(strict=True)
if hashlib.sha256(transmitter.read_bytes()).hexdigest() != args.sha256.lower():
    raise SystemExit('transmitter SHA-256 mismatch')
bsr = args.bsr.resolve(strict=True).read_bytes()
if bsr not in (b'9297\nH_OK\n64\n1\n2\n3\n-99\nprobe-a.jpg\n21010\n',
               b'9297\nH_OK\n154\n1\n2\n3\n-99\nprobe-a.jpg\n21010\n'):
    raise SystemExit('unexpected BSR fixture')

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

# The historical settings reader uses a 100-byte path buffer.
root = Path(tempfile.mkdtemp(prefix='dtrxs-' + args.variant + '-', dir='/tmp'))
for name in ('test', 'pics', 'picsrx'):
    (root / name).mkdir()
(root / 'picsrx/bsr.bin').write_bytes(bsr)
image = Path('tests/sstv/fixtures/qsstv-image-' + args.variant + '.jpg').read_bytes() + bytes(2000)
# This filename hashes to the fixture's transport ID in the historical TX.
(root / 'pics/FrhBlthiHM.jpg').write_bytes(image)
config = root / 'txamadrm.ini'
config.write_text('[sounddevices]\ndevin=%d\ndevout=%d\n[mode]\nrobustness=2\nBW=1\n'
                  'intlvldepth=0\nqam=16\nprotlev=0\n[callsign]\nmycall=LABTX\n'
                  '[ptt]\npttkind=none\n' % (input_index, output_index))
home_path = str(Path.home()).replace('\\', '\\\\').replace('"', '\\"')
profile = root / 'sandbox.sb'
profile.write_text('(version 1)\n(allow default)\n'
                   '(deny network-outbound (remote ip "*:*"))\n'
                   '(deny network-inbound (local ip "*:*"))\n'
                   '(deny file-read* file-write* (subpath "' + home_path + '"))\n')
env = os.environ.copy()
env['DECODIUM_TRX_LAB_CONFIG'] = str(config)
master, slave = pty.openpty()
process = None
data = bytearray()
try:
    process = subprocess.Popen(['sandbox-exec', '-f', str(profile), str(transmitter),
                                './pics/FrhBlthiHM.jpg'], cwd=root, env=env,
                               stdout=slave, stderr=subprocess.STDOUT)
    os.close(slave)
    slave = -1
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline:
        if select.select([master], [], [], .25)[0]:
            try:
                data.extend(os.read(master, 8192))
            except OSError:
                break
        if b'GetDataGroup body situation 2nd iSegmCntBody 14' in data:
            break
        if process.poll() is not None:
            break
finally:
    if process and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    if slave >= 0:
        os.close(slave)
    os.close(master)
    (root / 'tx.log').write_bytes(data)

text = data.decode(errors='replace')
segments = [int(x) for x in re.findall(r'GetDataGroup body situation 2nd iSegmCntBody (\d+)', text)]
skipped = [int(x) for x in re.findall(r'overgeslagen (\d+)', text)]
if ('opening BlackHole 2ch' not in text or 'valid bsr.bin file found bsrOK is 1' not in text
        or 'after reading bsr.bin missing segments is 3' not in text
        or segments != [0, 1, 2, 3, 14] or skipped != list(range(4, 14))):
    raise RuntimeError('historical selective transmission differed; artifacts ' + str(root))
print('PASS historical TX accepted BSR', args.variant,
      'sent', segments, 'skipped', skipped, 'artifacts', root, flush=True)
