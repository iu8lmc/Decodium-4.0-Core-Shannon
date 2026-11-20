#!/usr/bin/env python3
"""Replay synthetic Decodium references through instrumented QSSTV + local FTP.

Requires pyftpdlib 2.1.0 and QSSTV revision a845842 built with
qsstv-experimental-hybrid-ftp-oracle.patch. The patch restricts FTP to
loopback, the named synthetic fixtures and the selected ephemeral test port.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading

from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer

if len(sys.argv) != 4:
    raise SystemExit('usage: run_qsstv_hybrid_ftp_oracle.py PATCHED_QSSTV A.wav B.wav')
binary, a_wav, b_wav = (Path(arg).resolve(strict=True) for arg in sys.argv[1:])
repo = Path(__file__).resolve().parents[3]
root = Path(tempfile.mkdtemp(prefix='decodium-qsstv-hybrid-ftp-'))
ftp_root = root / 'server'
received = root / 'received'
received.mkdir()
for variant in ('a', 'b'):
    directory = ftp_root / 'radio' / variant / 'HybridFiles1'
    directory.mkdir(parents=True)
    image = repo / 'tests' / 'sstv' / 'fixtures' / ('qsstv-image-' + variant + '.jpg')
    name = 'de_TEST%s-1-image-%s.jpg' % ('1' if variant == 'a' else '2', variant)
    shutil.copyfile(image, directory / name)
auth = DummyAuthorizer()
auth.add_user('operator-a', 'dummy-A', str(ftp_root), perm='elr')
auth.add_user('operator-b', 'dummy-B-1234', str(ftp_root), perm='elr')
sent = []
class Handler(FTPHandler):
    authorizer = auth
    def on_file_sent(self, file):
        sent.append(Path(file).name)
server = FTPServer(('127.0.0.1', 0), Handler)
port = server.socket.getsockname()[1]
stop = threading.Event()
def serve():
    try:
        while not stop.is_set(): server.ioloop.loop(timeout=.1, blocking=False)
    finally:
        server.close_all()
thread = threading.Thread(target=serve, daemon=True)
thread.start()
env = os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen', DECODIUM_QSSTV_HYBRID_FTP_ORACLE='1',
           DECODIUM_QSSTV_HYBRID_FTP_PORT=str(port), DECODIUM_QSSTV_HYBRID_FTP_OUTPUT=str(received))
try:
    for variant, wav in (('a', a_wav), ('b', b_wav)):
        process = subprocess.run([str(binary), '--batch', '--drm', '--timeout', '60', '--out-dir', str(root), str(wav)],
                                 env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
        name = 'de_TEST%s-1-image-%s.jpg' % ('1' if variant == 'a' else '2', variant)
        output = received / name
        original = repo / 'tests' / 'sstv' / 'fixtures' / ('qsstv-image-' + variant + '.jpg')
        if ('[LAB][QSSTV] hybrid FTP file=1' not in process.stdout
            or not output.is_file() or output.read_bytes() != original.read_bytes() or name not in sent):
            raise RuntimeError('QSSTV FTP test failed for ' + variant + ':\n' + process.stdout)
        print('PASS instrumented QSSTV hybrid FTP', variant, 'exit', process.returncode, 'artifacts', root, flush=True)
finally:
    stop.set()
    thread.join(timeout=5)
    if thread.is_alive(): raise RuntimeError('local FTP server did not stop')
