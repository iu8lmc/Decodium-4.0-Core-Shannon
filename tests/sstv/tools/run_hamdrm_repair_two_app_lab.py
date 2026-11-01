#!/usr/bin/env python3
"""Local HAMDRM selective-repair A/B proof; BSR is injected, PCM is real."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import time

variant = sys.argv[1] if len(sys.argv) in (2, 3) else ''
if variant not in ('a', 'b'):
    raise SystemExit('expected a or b')
if len(sys.argv) == 3 and sys.argv[2] != 'air':
    raise SystemExit('optional second argument must be air')
air = len(sys.argv) == 3
root = Path(tempfile.mkdtemp(prefix='decodium-hamdrm-repair-' + variant + '-'))
binary = Path('build/decodium').resolve(strict=True)
fixture = root / ('repair-' + variant + '.jpg')
fixture.write_bytes(Path('tests/sstv/fixtures/qsstv-image-' + variant + '.jpg').read_bytes() + bytes(2500))
profile = 'hamdrm-E-2500-high-16-QAM-long'
common = ['--test-mode', '--lab-no-cat', '--lab-audio-input', 'BlackHole 2ch',
          '--lab-audio-output', 'BlackHole 2ch', '--lab-reapply-ms', '500',
          '--lab-hamdrm', '--lab-hamdrm-profile', profile]
processes = []

def launch(role, args, extra):
    env = os.environ.copy()
    env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
               DECODIUM_LAB_SETTINGS_DIR=str(root / (role + '-settings')), **extra)
    if air:
        env['DECODIUM_LAB_AUDIO_LIFECYCLE'] = '1'
    with (root / (role + '.log')).open('wb') as log:
        p = subprocess.Popen([str(binary), *common, '--rig-name', 'REPAIR' + role + secrets.token_hex(4),
                              *args], env=env, stdout=log, stderr=subprocess.STDOUT)
    processes.append(p)
    return p

def wait_for(predicate, seconds, message):
    deadline = time.monotonic() + seconds
    while not predicate():
        if any(p.poll() is not None for p in processes) or time.monotonic() > deadline:
            raise RuntimeError(message + '; artifacts ' + str(root))
        time.sleep(.2)

try:
    initial, ready, received = root / 'initial', root / 'ready', root / 'received.jpg'
    launch('tx', (['--lab-monitor-ms', '1800'] if air else ['--lab-no-monitor'])
           + ['--lab-sstv-tx-ms', '4000', '--lab-sstv-image', str(fixture),
                  '--lab-sstv-vox', '--lab-quit-ms', '140000'],
           {'DECODIUM_LAB_REPAIR_VARIANT': variant, 'DECODIUM_LAB_REPAIR_INITIAL': str(initial),
            'DECODIUM_LAB_REPAIR_READY': str(ready), 'DECODIUM_LAB_REPAIR_AIR': '1' if air else '0'})
    wait_for(initial.exists, 90, 'initial image TX did not complete')
    launch('rx', ['--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500',
                  '--lab-sstv-rx-ready-file', str(ready), '--lab-sstv-rx-image-file', str(received),
                  '--lab-quit-ms', '100000'], {'DECODIUM_LAB_REPAIR_FIXTURE': str(fixture),
                   'DECODIUM_LAB_REPAIR_VARIANT': variant, 'DECODIUM_LAB_REPAIR_READY': str(ready),
                   'DECODIUM_LAB_REPAIR_AIR': '1' if air else '0'})
    wait_for(ready.exists, 45 if air else 20, 'receiver did not become ready')
    if json.loads(ready.read_text()).get('sourceDevice') != 'BlackHole 2ch':
        raise RuntimeError('receiver is not using BlackHole; artifacts ' + str(root))
    wait_for(received.exists, 70, 'repaired image not received')
    if received.read_bytes() != fixture.read_bytes():
        raise RuntimeError('repaired image differs; artifacts ' + str(root))
    tx = (root / 'tx.log').read_text(errors='replace')
    rx = (root / 'rx.log').read_text(errors='replace')
    if '[LAB][REPAIR] TX started true' not in tx or '[LAB][REPAIR] RX partial seeded' not in rx:
        raise RuntimeError('repair path not confirmed; artifacts ' + str(root))
    if air and ('[LAB][REPAIR] over-audio BSR received' not in tx
                or '[LAB][REPAIR] over-audio BSR TX started true' not in rx):
        raise RuntimeError('BSR did not travel through audio; artifacts ' + str(root))
    def repair_tx_completed():
        log = (root / 'tx.log').read_text(errors='replace')
        marker = '[LAB][REPAIR] TX started true'
        return marker in log and '[LAB][HAMDRM] TX state "Completed"' in log.split(marker, 1)[1]
    wait_for(repair_tx_completed, 15, 'repair TX did not complete')
    print('PASS two Decodium apps repair', variant, 'exact bytes', len(received.read_bytes()),
          '(BSR and repair on BlackHole)' if air else '(BSR injected; PCM on BlackHole)',
          'artifacts', root, flush=True)
finally:
    for process in processes:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
