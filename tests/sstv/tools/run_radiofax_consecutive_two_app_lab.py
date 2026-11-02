#!/usr/bin/env python3
"""Prove consecutive Radiofax images arrive during one continuous RX session (macOS)."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import time

if len(sys.argv) not in (2, 3) or sys.argv[1] not in ('a', 'b') or (len(sys.argv) == 3 and sys.argv[2] != 'complete'):
    raise SystemExit('expected a|b [complete]')
full_canvas = len(sys.argv) == 3
order = ('a', 'b') if sys.argv[1] == 'a' else ('b', 'a')
mixed_rate = os.environ.get('DECODIUM_LAB_RADIOFAX_MIXED_RATE', '0')
if mixed_rate not in ('0', '1'):
    raise SystemExit('DECODIUM_LAB_RADIOFAX_MIXED_RATE must be 0 or 1')
rates = ((120, 240) if sys.argv[1] == 'a' else (240, 120)) if mixed_rate == '1' else (240, 240)
root = Path(tempfile.mkdtemp(prefix='decodium-radiofax-consecutive-' + sys.argv[1] + '-'))
binary = Path('build/decodium').resolve(strict=True)
common = ['--test-mode', '--lab-no-cat', '--lab-audio-input', 'BlackHole 2ch',
          '--lab-audio-output', 'BlackHole 2ch', '--lab-reapply-ms', '500']
processes = []

def launch(role, args, sequence=False):
    env = os.environ.copy()
    env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
               DECODIUM_LAB_SETTINGS_DIR=str(root / (role + '-settings')))
    if sequence:
        env['DECODIUM_LAB_RX_IMAGE_SEQUENCE'] = '1'
    with (root / (role + '.log')).open('wb') as log:
        process = subprocess.Popen([str(binary), *common, '--rig-name',
                                    'RFAX' + role + secrets.token_hex(4), *args],
                                   env=env, stdout=log, stderr=subprocess.STDOUT)
    processes.append(process)
    return process

try:
    rx = launch('rx', ['--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500',
        '--lab-sstv-fax-rx', '576,128,' + ('120' if full_canvas else '150') + ',1', '--lab-sstv-rx-ready-file', str(root/'ready'),
        '--lab-sstv-rx-image-file', str(root/'received'), '--lab-quit-ms', '300000'], True)
    deadline = time.monotonic() + 20
    while not (root/'ready').exists():
        if rx.poll() is not None or time.monotonic() > deadline:
            raise RuntimeError('RX did not become ready')
        time.sleep(.1)
    if json.loads((root/'ready').read_text()).get('sourceDevice') != 'BlackHole 2ch':
        raise RuntimeError('incorrect RX audio device')
    for index, variant in enumerate(order):
        tx = launch('tx' + str(index), ['--lab-no-monitor', '--lab-sstv-tx-ms', '4000',
            '--lab-sstv-mode', 'avt-24', '--lab-sstv-fax-tx', '576,' + str(rates[index]) + ',1',
            '--lab-sstv-tx-wait-file', str(root/'ready'), '--lab-sstv-tx-wait-timeout-ms', '20000',
            '--lab-sstv-image', str(Path('tests/sstv/fixtures/qsstv-image-' + variant + '.jpg').resolve(strict=True)),
            '--lab-sstv-tx-image-file', str(root/('reference'+str(index)+'.png')),
            '--lab-sstv-vox', '--lab-quit-ms', '140000'])
        # A completed transmission can be followed by slow legacy/QML teardown.
        # Wait for the actual completion record, then terminate only the lab
        # sender if it has not exited, keeping the continuous receiver alive.
        deadline = time.monotonic() + 145
        log_path = root/('tx'+str(index)+'.log')
        while True:
            txlog = log_path.read_text(errors='replace')
            complete = ('tx.completed ' in txlog and
                        '"modeId":"hffax-experimental","state":"Completed"' in txlog)
            if complete:
                break
            if tx.poll() is not None or rx.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError('TX completion missing or RX stopped')
            time.sleep(.5)
        if tx.poll() is not None and tx.returncode != 0:
            raise RuntimeError('TX process failed after completion')
        if tx.poll() is None:
            tx.terminate()
            try:
                tx.wait(timeout=5)
            except subprocess.TimeoutExpired:
                tx.kill()
                tx.wait(timeout=5)
        if rx.poll() is not None:
            raise RuntimeError('RX stopped after TX')
        print('TX', index + 1, 'completed; artifacts', root, flush=True)
    deadline = time.monotonic() + 10
    while len(list(root.glob('received.*.png'))) < 2 and time.monotonic() < deadline and rx.poll() is None:
        time.sleep(.5)
    images = sorted(root.glob('received.*.png'), key=lambda p: int(p.name.split('.')[1]))
    if len(images) != 2:
        raise RuntimeError('expected two distinct received images')
    diagnostics = [json.loads(line.split(' RX diagnostics ', 1)[1])
        for line in (root/'rx.log').read_text(errors='replace').splitlines()
        if '[LAB][SSTV] RX diagnostics ' in line]
    ids = {d.get('imageAcquisitionId') for d in diagnostics if d.get('imageLinesPublished') == 120
           and (d.get('imageComplete') if full_canvas else
                (d.get('imageRadiofaxStopDetected') and d.get('imagePartial')))}
    if len(ids) < 2 or any(d.get('processingFailures') or d.get('discontinuities') or d.get('droppedChunks')
                           for d in diagnostics):
        raise RuntimeError('invalid RX stop diagnostics or audio discontinuities')
    for index, image in enumerate(images):
        cropped = root/('cropped'+str(index)+'.png')
        subprocess.run(['ffmpeg', '-v', 'error', '-i', str(image), '-vf', 'crop=128:120:0:0',
                        '-frames:v', '1', str(cropped)], check=True, timeout=30)
        subprocess.run([sys.executable, 'tests/sstv/tools/compare_sstv_lab_images.py',
                        str(root/('reference'+str(index)+'.png')), str(cropped),
                        '--max-mae', '8', '--max-p95', '15'], check=True, timeout=30)
    print('PASS continuous RX with two distinct images:', order, 'rates', rates, 'acquisitions', sorted(ids),
          'artifacts', root, flush=True)
finally:
    for process in processes:
        if process.poll() is None:
            process.kill()
            process.wait()
    print('Artifacts:', root, flush=True)
