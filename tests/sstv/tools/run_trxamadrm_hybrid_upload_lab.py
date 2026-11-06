#!/usr/bin/env python3
"""Decodium hybrid upload and reference TX to historical RX over virtual audio.

Requires pyftpdlib 2.1.0, a prepared hash-pinned historical receiver and
BlackHole 2ch. The FTP server, account and payload are local test fixtures.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('variant', choices=('a', 'b'))
parser.add_argument('receiver', type=Path)
parser.add_argument('lowpass', type=Path)
parser.add_argument('--sha256', required=True)
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='decodium-trx-hybrid-upload-' + args.variant + '-'))
folder = root / 'radio' / 'HybridFiles1'
folder.mkdir(parents=True)
auth = DummyAuthorizer()
auth.add_user('operator', 'dummy', str(root), perm='elrw')
class Handler(FTPHandler):
    authorizer = auth
server = FTPServer(('127.0.0.1', 0), Handler)
port = server.socket.getsockname()[1]
stop = threading.Event()
def serve():
    try:
        while not stop.is_set():
            server.ioloop.loop(timeout=.1, blocking=False)
    finally:
        server.close_all()
thread = threading.Thread(target=serve, daemon=True)
thread.start()
try:
    env = os.environ.copy()
    env['DECODIUM_TRX_LAB_UPLOAD_PORT'] = str(port)
    harness = Path(__file__).with_name('run_trxamadrm_live_rx_lab.py')
    result = subprocess.run([sys.executable, str(harness), args.variant, str(args.receiver), str(args.lowpass),
                             '--sha256', args.sha256, '--hybrid'], env=env, check=True, timeout=100,
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(result.stdout, end='', flush=True)
    marker = 'artifacts '
    if marker not in result.stdout:
        raise RuntimeError('historical receiver artifact path missing')
    rxroot = Path(result.stdout.rsplit(marker, 1)[1].strip().splitlines()[0])
    received = [p for p in (rxroot / 'picsrx').iterdir() if p.is_file()]
    uploaded = [p for p in folder.iterdir() if p.is_file()]
    expected = Path('tests/sstv/fixtures/qsstv-image-' + args.variant + '.jpg').read_bytes()
    if len(received) != 1 or len(uploaded) != 1 or received[0].name != uploaded[0].name:
        raise RuntimeError('receiver reference and uploaded filename differ')
    if uploaded[0].read_bytes() != expected:
        raise RuntimeError('uploaded image differs')
    print('PASS Decodium upload and independent historical RX', args.variant,
          'exact image bytes', len(expected), 'server artifacts', root, flush=True)
finally:
    stop.set()
    thread.join(timeout=5)
    if thread.is_alive():
        raise RuntimeError('FTP server did not stop')
