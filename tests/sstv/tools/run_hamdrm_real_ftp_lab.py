#!/usr/bin/env python3
"""A/B two-Decodium hybrid proof using an independently implemented local FTP server.

Run with a Python environment containing pyftpdlib 2.1.0. No public server,
real credentials or privileged port is used.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading

from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer

if len(sys.argv) != 2 or sys.argv[1] not in ('a', 'b'):
    raise SystemExit('expected a or b')
root = Path(tempfile.mkdtemp(prefix='decodium-real-hybrid-ftp-'))
(root / 'radio' / 'HybridFiles1').mkdir(parents=True)
auth = DummyAuthorizer()
auth.add_user('operator', 'dummy', str(root), perm='elrw')
handler = type('LabFtpHandler', (FTPHandler,), {'authorizer': auth})
server = FTPServer(('127.0.0.1', 0), handler)
stop = threading.Event()
def serve():
    try:
        while not stop.is_set():
            server.ioloop.loop(timeout=.1, blocking=False)
    finally:
        server.close_all()
thread = threading.Thread(target=serve, daemon=True)
thread.start()
env = os.environ.copy()
env.update(DECODIUM_LAB_EXTERNAL_FTP_PORT=str(server.socket.getsockname()[1]),
           DECODIUM_LAB_EXTERNAL_FTP_ROOT=str(root), DECODIUM_LAB_HAMDRM_USER_TIMING='1')
try:
    harness = Path(__file__).with_name('run_hamdrm_two_app_lab.py')
    subprocess.run([sys.executable, str(harness), sys.argv[1], 'hybrid-upload'], env=env, check=True)
    print('PASS independent pyftpdlib FTP server:', sys.argv[1], 'root', root, flush=True)
finally:
    stop.set()
    thread.join(timeout=5)
    if thread.is_alive():
        raise RuntimeError('local FTP server did not stop')
