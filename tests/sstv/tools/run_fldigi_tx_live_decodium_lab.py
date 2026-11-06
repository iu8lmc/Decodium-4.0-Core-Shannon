#!/usr/bin/env python3
"""Live FLdigi TX to Decodium RX over BlackHole; macOS laboratory only."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import time

if len(sys.argv) != 2 or sys.argv[1] not in ('a', 'b'):
    raise SystemExit('expected a or b')
variant = sys.argv[1]
root = Path(tempfile.mkdtemp(prefix='decodium-fldigi-tx-live-'))
repo = Path.cwd()
binary = (repo / 'build/decodium').resolve(strict=True)
pixels = bytes(40 + ((x // 16 * (29 if variant == 'a' else 43)
                      + y // 15 * (37 if variant == 'a' else 23)
                      + (x // 16) * (y // 15) * 7) % 176)
               for y in range(120) for x in range(128))
source = root / 'reference.pgm'
source.write_bytes(b'P5\n128 120\n255\n' + pixels)
env = os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
           QML_DISABLE_DISK_CACHE='1', DECODIUM_LAB_FAX_DIAGNOSTICS='1',
           DECODIUM_LAB_SETTINGS_DIR=str(root / 'settings'))
args = [str(binary), '--test-mode', '--rig-name', 'FLFAXLIVE' + secrets.token_hex(4),
        '--lab-no-cat', '--lab-audio-input', 'BlackHole 2ch', '--lab-audio-output', 'BlackHole 2ch',
        '--lab-reapply-ms', '500', '--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500',
        '--lab-sstv-fax-rx', '576,128,150,1', '--lab-sstv-rx-ready-file', str(root / 'ready'),
        '--lab-sstv-rx-image-file', str(root / 'received.png'), '--lab-quit-ms', '180000']
print('artifacts', root, flush=True)
process = None
try:
    with (root / 'rx.log').open('wb') as log:
        process = subprocess.Popen(args, env=env, stdout=log, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + 25
    while not (root / 'ready').exists():
        if process.poll() is not None or time.monotonic() > deadline:
            raise RuntimeError('Decodium receiver did not become ready')
        time.sleep(.1)
    if json.loads((root / 'ready').read_text()).get('sourceDevice') != 'BlackHole 2ch':
        raise RuntimeError('unexpected receive device')
    with (root / 'tx.log').open('wb') as log:
        tx = subprocess.run([sys.executable, str(repo / 'tests/sstv/tools/run_fldigi_tx_capture_lab.py'),
                             str(source)], env=env, stdout=log, stderr=subprocess.STDOUT, timeout=150)
    if tx.returncode:
        raise RuntimeError('FLdigi transmitter failed')
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        log = (root / 'rx.log').read_text(errors='replace')
        lines = [json.loads(line.split(' RX diagnostics ', 1)[1]) for line in log.splitlines()
                 if '[LAB][SSTV] RX diagnostics ' in line]
        if (root / 'received.png').exists() and lines and lines[-1].get('imageRadiofaxStopDetected'):
            break
        if process.poll() is not None:
            raise RuntimeError('Decodium receiver stopped before image completion')
        time.sleep(.2)
    else:
        raise RuntimeError('Decodium did not save an image with a detected stop')
    if (lines[-1].get('imageLinesPublished') != 120
            or any(item.get('sourceDevice') != 'BlackHole 2ch'
                   or item.get('discontinuities') != 0 or item.get('droppedChunks') != 0
                   or item.get('processingFailures') != 0 for item in lines)):
        raise RuntimeError('unexpected line count or audio discontinuity')
    cropped = root / 'first-120.png'
    subprocess.run(['ffmpeg', '-v', 'error', '-i', str(root / 'received.png'), '-vf', 'crop=128:120:0:0',
                    '-frames:v', '1', str(cropped)], check=True, timeout=30)
    subprocess.run([sys.executable, str(repo / 'tests/sstv/tools/compare_sstv_lab_images.py'),
                    str(source), str(cropped), '--max-mae', '8', '--max-p95', '4'], check=True, timeout=30)
    print('PASS live independent FLdigi TX to Decodium RX:', variant, 'artifacts', root, flush=True)
finally:
    if process and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
