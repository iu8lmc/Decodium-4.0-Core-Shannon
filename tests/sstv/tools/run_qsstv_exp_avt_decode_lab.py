#!/usr/bin/env python3
"""Compare a captured Decodium AVT24 transmission with a pinned QSSTV receiver."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

if len(sys.argv) != 3:
    raise SystemExit('expected capture.f32 reference.png')
raw, reference = (Path(value).resolve(strict=True) for value in sys.argv[1:])
qsstv = Path(os.environ.get('QSSTV_EXPERIMENTAL_BINARY',
    '/tmp/decodium-qsstv-experimental/QSSTV-Experimental.app/Contents/MacOS/qsstv-experimental')).resolve(strict=True)
if hashlib.sha256(qsstv.read_bytes()).hexdigest() != 'b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa':
    raise RuntimeError('QSSTV binary does not match pinned 10.0 ARM64 release')
root = Path(tempfile.mkdtemp(prefix='decodium-qsstv-avt24-decode-'))
subprocess.run(['ffmpeg', '-v', 'error', '-f', 'f32le', '-ar', '48000', '-ac', '2', '-i', str(raw),
                '-ar', '12000', '-ac', '1', str(root / 'capture.wav')], check=True, timeout=30)
env = os.environ.copy(); env['QT_QPA_PLATFORM'] = 'cocoa'
with (root / 'qsstv.log').open('wb') as log:
    try:
        result = subprocess.run([str(qsstv), '--batch', '--mode', 'AVT24', '--out-dir', str(root),
                                 str(root / 'capture.wav')], env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError('QSSTV did not finish in 240 seconds; artifacts ' + str(root)) from error
if result.returncode:
    raise RuntimeError('QSSTV failed to decode AVT24; artifacts ' + str(root))
images = list(root.glob('capture_*_AVT24.png'))
if len(images) != 1:
    raise RuntimeError('unexpected QSSTV output count; artifacts ' + str(root))
subprocess.run([sys.executable, 'tests/sstv/tools/compare_sstv_lab_images.py', str(reference), str(images[0]),
                '--max-mae', '5', '--max-p95', '15'], check=True, timeout=30)
print('PASS Decodium AVT24 to QSSTV-Experimental 10.0; artifacts', root, flush=True)
