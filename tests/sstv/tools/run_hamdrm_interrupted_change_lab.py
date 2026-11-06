#!/usr/bin/env python3
"""Interrupt one Decodium HAMDRM sender, then receive another on the same RX."""
import json
import os
from pathlib import Path
import re
import secrets
import subprocess
import sys
import tempfile
import time

if len(sys.argv) != 2 or sys.argv[1] not in ('a', 'b'):
    raise SystemExit('expected a or b')
variant = sys.argv[1]
first_depth, second_depth = ('long', 'short') if variant == 'a' else ('short', 'long')
first_image, second_image = ('a', 'b') if variant == 'a' else ('b', 'a')
profile = lambda depth: 'hamdrm-E-2500-high-16-QAM-' + depth
root = Path(tempfile.mkdtemp(prefix='decodium-hamdrm-interrupted-' + variant + '-'))
binary = Path('build/decodium').resolve(strict=True)
fixtures = Path('tests/sstv/fixtures').resolve(strict=True)
processes = []
logs = []
common = ['--test-mode', '--lab-no-cat', '--lab-audio-input', 'BlackHole 2ch',
          '--lab-audio-output', 'BlackHole 2ch', '--lab-reapply-ms', '500']

def start(role, args):
    env = os.environ.copy()
    env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
               DECODIUM_LAB_SETTINGS_DIR=str(root / (role + '-settings')))
    if role == 'rx': env['DECODIUM_HAMDRM_RX_TRACE'] = '1'
    log = (root / (role + '.log')).open('wb')
    logs.append(log)
    process = subprocess.Popen([str(binary), *common, '--rig-name',
                                'DRM' + role + secrets.token_hex(4), *args],
                               env=env, stdout=log, stderr=subprocess.STDOUT)
    processes.append(process)
    return process

def wait_for(predicate, timeout, message, alive):
    deadline = time.monotonic() + timeout
    while not predicate():
        if any(p.poll() is not None for p in alive) or time.monotonic() >= deadline:
            raise RuntimeError(message + '; artifacts ' + str(root))
        time.sleep(0.1)

def rxlog():
    return (root / 'rx.log').read_text(errors='replace')

def saw_depth(depth):
    return bool(re.search(r'\[HAMDRM RX trace\] frames [1-9]\d* FAC interleaver '
                          + depth + r'\b', rxlog()))

def sender(role, depth, image):
    return start(role, ['--lab-no-monitor', '--lab-sstv-tx-ms', '3000', '--lab-hamdrm',
        '--lab-hamdrm-profile', profile(depth), '--lab-sstv-tx-wait-file', str(root / 'ready'),
        '--lab-sstv-tx-wait-timeout-ms', '20000', '--lab-sstv-image',
        str(fixtures / ('qsstv-image-' + image + '.jpg')), '--lab-sstv-vox',
        '--lab-quit-ms', '45000'])

try:
    rx = start('rx', ['--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500', '--lab-hamdrm',
        '--lab-hamdrm-profile', profile(first_depth), '--lab-sstv-rx-ready-file', str(root / 'ready'),
        '--lab-sstv-rx-image-file', str(root / 'received.jpg'), '--lab-quit-ms', '95000'])
    wait_for(lambda: (root / 'ready').exists(), 25, 'RX was not ready', [rx])
    ready = json.loads((root / 'ready').read_text())
    if ready.get('sourceDevice') != 'BlackHole 2ch': raise RuntimeError('wrong RX device')
    first = sender('first', first_depth, first_image)
    wait_for(lambda: saw_depth(first_depth), 25, 'first FAC not received', [rx, first])
    if (root / 'received.jpg').exists(): raise RuntimeError('first image already complete')
    first.terminate()
    first.wait(timeout=10)
    time.sleep(2)
    if (root / 'received.jpg').exists(): raise RuntimeError('interrupted image unexpectedly completed')
    second = sender('second', second_depth, second_image)
    wait_for(lambda: saw_depth(second_depth) and '[LAB][HAMDRM] RX object saved' in rxlog(), 40,
             'second image or FAC not received', [rx, second])
    received = (root / 'received.jpg').read_bytes()
    expected = (fixtures / ('qsstv-image-' + second_image + '.jpg')).read_bytes()
    if received != expected: raise RuntimeError('received image differs from second input')
    wait_for(lambda: '[LAB][HAMDRM] TX state "Completed"' in
             (root / 'second.log').read_text(errors='replace'), 20,
             'second TX did not complete', [rx, second])
    log = rxlog()
    if log.count('[LAB][HAMDRM] RX object saved') != 1:
        raise RuntimeError('unexpected number of completed RX objects; artifacts ' + str(root))
    diagnostics = [json.loads(line.split(' RX diagnostics ', 1)[1])
                   for line in log.splitlines() if '[LAB][SSTV] RX diagnostics ' in line]
    counters = ('discontinuities', 'droppedChunks', 'hamdrmRejectedChunks',
                'hamdrmQueueOverflows', 'processingFailures')
    if not diagnostics or any(row.get('sourceDevice') != 'BlackHole 2ch'
                              or any(row.get(key) != 0 for key in counters)
                              for row in diagnostics):
        raise RuntimeError('RX audio diagnostics failed; artifacts ' + str(root))
    print('PASS interrupted HAMDRM', variant, first_depth, 'to', second_depth,
          'exact bytes', len(received), 'artifacts', root, flush=True)
finally:
    for process in processes:
        if process.poll() is None: process.terminate()
    for process in processes:
        try: process.wait(timeout=10)
        except subprocess.TimeoutExpired: process.kill(); process.wait()
    for log in logs: log.close()
