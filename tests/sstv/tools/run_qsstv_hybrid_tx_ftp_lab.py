#!/usr/bin/env python3
"""Instrumented QSSTV hybrid TX -> Decodium RX over virtual audio + local FTP.

QSSTV must be built from a845842 with qsstv-experimental-hybrid-tx-oracle.patch;
the small hybrid codec oracle uses the same corrected source. pyftpdlib 2.1.0
is required. Every server, path and account is synthetic and loopback-only.
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

if len(sys.argv) != 4 or sys.argv[3] not in ('a', 'b'):
    raise SystemExit('usage: run_qsstv_hybrid_tx_ftp_lab.py PATCHED_QSSTV CODEC_ORACLE a|b')
qsstv, oracle = (Path(arg).resolve(strict=True) for arg in sys.argv[1:3])
variant = sys.argv[3]
repo = Path(__file__).resolve().parents[3]
root = Path(tempfile.mkdtemp(prefix='decodium-qsstv-hybrid-tx-'))
ftp_root = root / 'server'
folder = ftp_root / 'radio' / variant / 'HybridFiles1'
folder.mkdir(parents=True)
auth = DummyAuthorizer()
auth.add_user('operator-a', 'dummy-A', str(ftp_root), perm='elrw')
auth.add_user('operator-b', 'dummy-B-1234', str(ftp_root), perm='elrw')
class Handler(FTPHandler):
    authorizer = auth
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
env.update(QT_QPA_PLATFORM='offscreen', DECODIUM_QSSTV_HYBRID_TX_ORACLE='1',
           DECODIUM_QSSTV_HYBRID_FTP_PORT=str(port), DECODIUM_QSSTV_HYBRID_VARIANT=variant)
try:
    source = root / ('input-' + variant + '.png')
    pattern = 'smptebars=size=320x240:rate=1' if variant == 'a' else 'smptehdbars=size=320x240:rate=1'
    subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i', pattern, '-frames:v', '1', str(source)], check=True)
    wav = root / 'hybrid.wav'
    result = subprocess.run([str(qsstv), '--encode', str(source), '--drm', '--drm-mode', 'E', '--drm-bw', '2.5',
                             '--drm-qam', '4', '--drm-prot', 'high', '--drm-interleave', 'long', '--wav-out', str(wav)],
                            env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
    (root / 'qsstv.log').write_text(result.stdout)
    files = list(folder.iterdir())
    if result.returncode or not wav.is_file() or len(files) != 1 or not files[0].stat().st_size:
        raise RuntimeError('QSSTV hybrid TX or upload failed; artifacts ' + str(root))
    expected = root / 'expected-reference.bin'
    ref_env = env.copy()
    ref_env['DECODIUM_QSSTV_ORACLE_LOOPBACK'] = '1'
    subprocess.run([str(oracle), variant, str(expected)], env=ref_env, check=True)
    rx_env = env.copy()
    rx_env.update(DECODIUM_LAB_HAMDRM_HYBRID=variant, DECODIUM_LAB_HAMDRM_USER_TIMING='1',
                  DECODIUM_LAB_HYBRID_FTP_PORT=str(port), DECODIUM_HAMDRM_LAB_DURATION_MS='80000',
                  DECODIUM_HAMDRM_LAB_PROFILE='hamdrm-E-2500-high-4-QAM-long')
    harness = repo / 'tests' / 'sstv' / 'tools' / 'run_qsstv_exp_drm_app_lab.py'
    replay = subprocess.run([sys.executable, str(harness), str(wav), str(expected)], cwd=repo, env=rx_env,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=150)
    (root / 'replay.log').write_text(replay.stdout)
    if replay.returncode:
        raise RuntimeError('Decodium did not receive QSSTV reference; artifacts ' + str(root) + '\n' + replay.stdout)
    # The replay harness reports its own artifact directory. The lab RX hook
    # saves the FTP result next to the reference using the .download suffix.
    marker = 'artifacts '
    if marker not in replay.stdout: raise RuntimeError('missing replay artifact path')
    replay_root = Path(replay.stdout.rsplit(marker, 1)[1].strip().splitlines()[0])
    download = replay_root / 'received.jp2.download'
    if not download.is_file() or download.read_bytes() != files[0].read_bytes():
        raise RuntimeError('Decodium FTP download did not match QSSTV upload; artifacts ' + str(root))
    print('PASS instrumented QSSTV TX -> Decodium hybrid', variant, 'image bytes', files[0].stat().st_size,
          'artifacts', root, 'replay', replay_root, flush=True)
finally:
    stop.set()
    thread.join(timeout=5)
    if thread.is_alive(): raise RuntimeError('local FTP server did not stop')
