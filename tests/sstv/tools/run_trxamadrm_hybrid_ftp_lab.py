#!/usr/bin/env python3
"""Independent historical hybrid transmitter to Decodium and loopback FTP.

Requires pyftpdlib 2.1.0, a prepared hash-pinned hybrid transmitter and
BlackHole 2ch. All FTP credentials, paths and payloads are synthetic.
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
parser.add_argument('transmitter', type=Path)
parser.add_argument('--sha256', required=True)
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='decodium-trx-hybrid-ftp-' + args.variant + '-'))
folder = root / 'radio' / args.variant / 'HybridFiles1'
folder.mkdir(parents=True)
filename = 'de_TEST1-1-image-a.jpg' if args.variant == 'a' else 'de_TEST2-1-image-b.jpg'
source = Path('tests/sstv/fixtures/qsstv-image-' + args.variant + '.jpg').resolve(strict=True)
(folder / filename).write_bytes(source.read_bytes())
auth = DummyAuthorizer()
auth.add_user('operator-a', 'dummy-A', str(root), perm='elr')
auth.add_user('operator-b', 'dummy-B-1234', str(root), perm='elr')
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
    env['DECODIUM_TRX_LAB_FTP_PORT'] = str(port)
    harness = Path(__file__).with_name('run_trxamadrm_live_tx_lab.py')
    subprocess.run([sys.executable, str(harness), args.variant, str(args.transmitter),
                    '--sha256', args.sha256, '--hybrid'], env=env, check=True, timeout=130)
    print('PASS independent TRXAMADRM hybrid reference and FTP download', args.variant,
          'server artifacts', root, flush=True)
finally:
    stop.set()
    thread.join(timeout=5)
    if thread.is_alive():
        raise RuntimeError('FTP server did not stop')
