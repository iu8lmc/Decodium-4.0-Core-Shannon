#!/usr/bin/env python3
"""Send a Decodium HAMDRM image to a prepared TRXAMADRM RX over BlackHole."""
import argparse
import hashlib
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time
from trxamadrm_hybrid_fixture import hybrid_fixture

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('variant', choices=('a', 'b'))
parser.add_argument('receiver', type=Path)
parser.add_argument('lowpass', type=Path)
parser.add_argument('--sha256', required=True, help='SHA-256 of the prepared lab receiver')
parser.add_argument('--bsr', action='store_true', help='send an A/B BSR fixture instead of an image')
parser.add_argument('--hybrid', action='store_true', help='send an A/B hybrid reference instead of an image')
parser.add_argument('--qam', type=int, choices=(4, 16, 64), default=16, help='MSC QAM constellation')
parser.add_argument('--interleaver', choices=('short', 'long'), default='long')
parser.add_argument('--protection', choices=('normal', 'high'), default='high')
parser.add_argument('--bsr-segment-size', type=int, choices=(64, 154), default=64)
args = parser.parse_args()
if args.bsr and args.hybrid:
    parser.error('--bsr and --hybrid are mutually exclusive')
receiver = args.receiver.resolve(strict=True)
lowpass = args.lowpass.resolve(strict=True)
if hashlib.sha256(receiver.read_bytes()).hexdigest() != args.sha256.lower():
    raise SystemExit('receiver SHA-256 mismatch')
if hashlib.sha256(lowpass.read_bytes()).hexdigest() != '5365e6ab619e2a0c26ae47bd191f09d1531b8d609ac527ee8eb1f9a7a3b8df27':
    raise SystemExit('lowpass SHA-256 mismatch')
source = Path('tests/sstv/fixtures/qsstv-image-' + args.variant + '.jpg').resolve(strict=True)
expected = (('9297\nH_OK\n%d\n1\n2\n3\n-99\nprobe-a.jpg\n21010\n' % args.bsr_segment_size).encode()
            if args.variant == 'a' else
            ('21362\nH_OK\n%d\n1\n-1\n3\n-99\n' % args.bsr_segment_size).encode()) if args.bsr else source.read_bytes()
if args.hybrid:
    expected = hybrid_fixture(args.variant, upload=bool(os.environ.get('DECODIUM_TRX_LAB_UPLOAD_PORT')))
binary = Path('build/decodium').resolve(strict=True)
root = Path(tempfile.mkdtemp(prefix='decodium-trxamadrm-live-rx-' + args.variant + '-'))
for name in ('picsrx', 'incomplete'):
    (root / name).mkdir()
(root / 'lowpass.dat').write_bytes(lowpass.read_bytes())
# The historical receiver handles only lab-controlled filenames. Keep it out
# of the user's home and off IP networks while allowing local audio services.
home_path = str(Path.home()).replace('\\', '\\\\').replace('"', '\\"')
profile = root / 'sandbox.sb'
profile.write_text('(version 1)\n(allow default)\n'
                   '(deny network-outbound (remote ip "*:*"))\n'
                   '(deny network-inbound (local ip "*:*"))\n'
                   '(deny file-read* file-write* (subpath "' + home_path + '"))\n')
rx = tx = None
try:
    with (root / 'rx.log').open('wb') as log:
        rx = subprocess.Popen(['sandbox-exec', '-f', str(profile), str(receiver)],
                              cwd=root, stdout=log, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + 20
    while 'LAB RX BlackHole 2ch ready' not in (root / 'rx.log').read_text(errors='replace'):
        if rx.poll() is not None or time.monotonic() > deadline:
            raise RuntimeError('RX not ready: ' + str(root))
        time.sleep(.1)
    env = os.environ.copy()
    env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
               DECODIUM_LAB_SETTINGS_DIR=str(root / 'settings'))
    if args.bsr:
        env['DECODIUM_LAB_HAMDRM_BSR'] = args.variant
        env['DECODIUM_LAB_BSR_SEGMENT_SIZE'] = str(args.bsr_segment_size)
    if args.hybrid:
        env['DECODIUM_LAB_HAMDRM_HYBRID'] = args.variant
        if os.environ.get('DECODIUM_TRX_LAB_UPLOAD_PORT'):
            env['DECODIUM_LAB_HYBRID_UPLOAD'] = '1'
            env['DECODIUM_LAB_HYBRID_FTP_PORT'] = os.environ['DECODIUM_TRX_LAB_UPLOAD_PORT']
    cmd = [str(binary), '--test-mode', '--rig-name', 'TRXRX' + secrets.token_hex(3),
           '--lab-no-cat', '--lab-no-monitor', '--lab-audio-input', 'BlackHole 2ch',
           '--lab-audio-output', 'BlackHole 2ch', '--lab-reapply-ms', '500',
           '--lab-sstv-tx-ms', '4000', '--lab-hamdrm',
           '--lab-hamdrm-profile', 'hamdrm-E-2500-%s-%d-QAM-%s' % (args.protection, args.qam, args.interleaver),
           '--lab-sstv-image', str(source), '--lab-sstv-vox', '--lab-quit-ms', '40000']
    with (root / 'tx.log').open('wb') as log:
        tx = subprocess.Popen(cmd, env=env, stdout=log, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + 50
    received = None
    while time.monotonic() < deadline:
        for candidate in (root / 'picsrx').iterdir():
            if (candidate.is_file() and candidate.read_bytes() == expected
                    and (not args.bsr or candidate.name == 'bsr.bin')
                    and (not args.hybrid or bool(os.environ.get('DECODIUM_TRX_LAB_UPLOAD_PORT')) or candidate.name == (
                        'de_TEST1-1-image-a.jpg' if args.variant == 'a' else 'de_TEST2-1-image-b.jpg'))):
                received = candidate
        log = (root / 'tx.log').read_text(errors='replace')
        if received and '[LAB][HAMDRM] TX state "Completed"' in log:
            break
        if rx.poll() is not None or tx.poll() is not None:
            raise RuntimeError('a peer exited before completion: ' + str(root))
        time.sleep(.25)
    else:
        raise RuntimeError('transfer timed out: ' + str(root))
    start_marker = '[LAB][HAMDRM] ' + ('BSR TX' if args.bsr else 'TX')
    if start_marker + (' started true "hamdrm-E-2500-%s-%d-QAM-%s"' % (args.protection, args.qam, args.interleaver)) not in log:
        raise RuntimeError('TX profile was not confirmed: ' + str(root))
    if 'lab input failed' in (root / 'rx.log').read_text(errors='replace'):
        raise RuntimeError('RX audio input failed: ' + str(root))
finally:
    for process in (tx, rx):
        if process and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
print('PASS live Decodium to TRXAMADRM', args.variant, 'BSR' if args.bsr else ('hybrid' if args.hybrid else 'image'),
      'exact bytes', len(expected),
      'artifacts', root, flush=True)
