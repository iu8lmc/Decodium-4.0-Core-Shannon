#!/usr/bin/env python3
"""Experimental live QSSTV TX -> Decodium RX/BSR -> QSSTV repair over BlackHole."""
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
defer_busy = os.environ.get('DECODIUM_LAB_QSSTV_REVERSE_DEFER') == '1'
bypass_busy = os.environ.get('DECODIUM_LAB_QSSTV_REVERSE_BYPASS_BUSY') == '1'
expect_busy = os.environ.get('DECODIUM_LAB_QSSTV_REVERSE_EXPECT_BUSY') == '1'
expect_rx_busy = os.environ.get('DECODIUM_LAB_QSSTV_REVERSE_EXPECT_RX_BUSY') == '1'
if expect_rx_busy and not expect_busy:
    raise SystemExit('RX busy observation requires expected busy rejection')
if sum((defer_busy, expect_busy, bypass_busy)) > 1:
    raise SystemExit('busy rejection, deferral and bypass are mutually exclusive')
root = Path(tempfile.mkdtemp(prefix='decodium-qsstv-reverse-' + variant + '-'))
qsstv = Path(os.environ['DECODIUM_LAB_REPAIR_QSSTV_BINARY']).resolve(strict=True)
decodium = Path('build/decodium').resolve(strict=True)
image = root / 'input.png'
pattern = 'testsrc2=size=320x240:rate=1' if variant == 'a' else 'smptehdbars=size=320x240:rate=1'
subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i', pattern,
                '-frames:v', '1', str(image)], check=True, timeout=15)
initial = root / 'initial'
ready = root / 'ready'
rx_ready = root / 'rx-ready'
received = root / 'received.bin'
original = root / 'qsstv-original.bin'
processes = []

def wait_for(predicate, seconds, message):
    deadline = time.monotonic() + seconds
    while not predicate():
        qlog = root / 'qsstv.log'
        if qlog.exists():
            for line in qlog.read_text(errors='replace').splitlines():
                if not defer_busy and not expect_busy and not bypass_busy and 'LAB reverse prepareFix busy=' in line and 'busy=-1 ' not in line:
                    raise RuntimeError('QSSTV discarded BSR while busy: ' + line + '; artifacts ' + str(root))
        if any(p.poll() is not None for p in processes) or time.monotonic() > deadline:
            raise RuntimeError(message + '; artifacts ' + str(root))
        time.sleep(.2)

try:
    env = os.environ.copy()
    env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
               DECODIUM_LAB_SETTINGS_DIR=str(root / 'rx-settings'),
               DECODIUM_LAB_REPAIR_LIVE=variant, DECODIUM_LAB_REPAIR_INITIAL=str(initial),
               DECODIUM_LAB_REPAIR_READY=str(ready), DECODIUM_LAB_DROP_MOT_UNTIL=str(initial))
    args = [str(decodium), '--test-mode', '--rig-name', 'QREV' + secrets.token_hex(4),
            '--lab-no-cat', '--lab-audio-input', 'BlackHole 2ch', '--lab-audio-output', 'BlackHole 2ch',
            '--lab-reapply-ms', '500', '--lab-hamdrm', '--lab-hamdrm-profile', 'hamdrm-E-2500-high-16-QAM-long',
            '--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500', '--lab-sstv-rx-ready-file', str(rx_ready),
            '--lab-sstv-rx-image-file', str(received), '--lab-quit-ms', '160000']
    with (root / 'rx.log').open('wb') as log:
        processes.append(subprocess.Popen(args, env=env, stdout=log, stderr=subprocess.STDOUT))
    wait_for(rx_ready.exists, 25, 'Decodium RX not ready')
    if json.loads(rx_ready.read_text()).get('sourceDevice') != 'BlackHole 2ch':
        raise RuntimeError('wrong RX device; artifacts ' + str(root))
    settings = root / 'qsstv-settings'
    settings.mkdir()
    env = os.environ.copy()
    env.update(QT_QPA_PLATFORM='offscreen', QSSTV_BSR_LAB_ROOT=str(settings),
               QSSTV_BSR_TX_LIVE='1', QSSTV_REVERSE_LAB='1', QSSTV_REVERSE_RX_READY=str(rx_ready),
               QSSTV_REVERSE_DEFER='1' if defer_busy else '0',
               QSSTV_REVERSE_BYPASS_BUSY='1' if bypass_busy else '0',
               QSSTV_REVERSE_IMAGE=str(image), QSSTV_REVERSE_ORIGINAL=str(original),
               QSSTV_REVERSE_RECEIVED_BSR=str(root / 'qsstv-received.bsr'),
               QSSTV_REVERSE_INITIAL_DONE=str(initial), QSSTV_REVERSE_REPAIR_DONE=str(root / 'repair-done'))
    with (root / 'qsstv.log').open('wb') as log:
        processes.append(subprocess.Popen([str(qsstv)], env=env, stdout=log, stderr=subprocess.STDOUT))
    if expect_busy:
        def observed_busy_request():
            path = root / 'qsstv.log'
            if not path.exists():
                return False
            log = path.read_text(errors='replace')
            return ('LAB reverse prepareFix busy=1 ' in log
                    and (not expect_rx_busy or 'LAB reverse busy flags tx=0 rx=1' in log))
        wait_for(observed_busy_request, 135, 'QSSTV busy rejection not observed')
        qlog = (root / 'qsstv.log').read_text(errors='replace')
        rlog = (root / 'rx.log').read_text(errors='replace')
        if expect_rx_busy and 'LAB reverse busy flags tx=0 rx=1' not in qlog:
            raise RuntimeError('QSSTV RX-only busy state not observed; artifacts ' + str(root))
        for marker in ('LAB live audio input=BlackHole 2ch output=BlackHole 16ch',
                       'LAB reverse initial image TX started 1',
                       'LAB reverse TX stopped 1 and RX restarted',
                       'LAB reverse RX complete BSR bytes=',
                       'LAB reverse prepareFix busy=1 '):
            if marker not in qlog:
                raise RuntimeError('missing QSSTV marker ' + marker + '; artifacts ' + str(root))
        for marker in ('[LAB][REPAIR] live partial received',
                       '[LAB][REPAIR] over-audio BSR TX started true',
                       '[LAB][HAMDRM] discarded initial MOT body segment 1 final false'):
            if marker not in rlog:
                raise RuntimeError('missing Decodium marker ' + marker + '; artifacts ' + str(root))
        marker = '[LAB][REPAIR] generated BSR '
        if marker not in rlog:
            raise RuntimeError('missing generated BSR; artifacts ' + str(root))
        generated = rlog.split(marker, 1)[1].split('\n\n', 1)[0].encode().rstrip(b'\n')
        received_bsr = (root / 'qsstv-received.bsr').read_bytes().rstrip(b'\n')
        if generated != received_bsr:
            raise RuntimeError('QSSTV BSR differs from Decodium request; artifacts ' + str(root))
        diagnostics = [json.loads(line.split('[LAB][SSTV] RX diagnostics ', 1)[1])
                       for line in rlog.splitlines() if '[LAB][SSTV] RX diagnostics ' in line]
        active = [row for row in diagnostics if row.get('hamdrmActive')]
        counters = ('discontinuities', 'droppedChunks', 'hamdrmRejectedChunks', 'hamdrmQueueOverflows', 'processingFailures')
        if not active or any(row.get('sourceDevice') != 'BlackHole 2ch' or any(row.get(k) != 0 for k in counters) for row in active):
            raise RuntimeError('Decodium audio diagnostics failed; artifacts ' + str(root))
        if received.exists() or (root / 'repair-done').exists():
            raise RuntimeError('unexpected repaired image; artifacts ' + str(root))
        print('PASS expected QSSTV busy rejection', variant, 'matching BSR bytes', len(generated), 'artifacts', root, flush=True)
        raise SystemExit(0)
    wait_for(lambda: received.exists() and original.exists() and received.read_bytes() == original.read_bytes(),
             135, 'QSSTV image not repaired by Decodium')
    wait_for(lambda: (root / 'repair-done').exists(), 15, 'QSSTV repair did not complete')
    qlog = (root / 'qsstv.log').read_text(errors='replace')
    rlog = (root / 'rx.log').read_text(errors='replace')
    for marker in ('LAB live audio input=BlackHole 2ch output=BlackHole 16ch',
                   'LAB reverse initial image TX started 1', 'LAB reverse BSR matched',
                   'LAB reverse session found=1',
                   'LAB reverse TX stopped 1 and RX restarted', 'LAB reverse TX stopped 2 and RX restarted'):
        if marker not in qlog: raise RuntimeError('missing QSSTV marker ' + marker + '; artifacts ' + str(root))
    if bypass_busy and ('LAB reverse bypass busy guard' not in qlog or 'LAB reverse busy flags tx=0 rx=1' not in qlog):
        raise RuntimeError('QSSTV RX-busy guard bypass not observed; artifacts ' + str(root))
    if defer_busy and ('LAB reverse prepareFix busy=1 ' not in qlog or 'LAB reverse prepareFix busy=-1 ' not in qlog):
        raise RuntimeError('QSSTV busy-to-idle deferral not observed; artifacts ' + str(root))
    if ('[LAB][REPAIR] over-audio BSR TX started true' not in rlog
            or '[LAB][REPAIR] live partial received' not in rlog
            or '[LAB][HAMDRM] discarded initial MOT body segment 1 final false' not in rlog):
        raise RuntimeError('missing Decodium BSR flow; artifacts ' + str(root))
    generated_marker = '[LAB][REPAIR] generated BSR '
    if generated_marker not in rlog:
        raise RuntimeError('missing generated BSR; artifacts ' + str(root))
    generated = rlog.split(generated_marker, 1)[1].split('\n\n', 1)[0].encode().rstrip(b'\n')
    if (root / 'qsstv-received.bsr').read_bytes().rstrip(b'\n') != generated:
        raise RuntimeError('QSSTV BSR differs from Decodium request; artifacts ' + str(root))
    diagnostics = [json.loads(line.split('[LAB][SSTV] RX diagnostics ', 1)[1])
                   for line in rlog.splitlines() if '[LAB][SSTV] RX diagnostics ' in line]
    active = [row for row in diagnostics if row.get('hamdrmActive')]
    counters = ('discontinuities', 'droppedChunks', 'hamdrmRejectedChunks', 'hamdrmQueueOverflows', 'processingFailures')
    if not active or any(row.get('sourceDevice') != 'BlackHole 2ch' or any(row.get(k) != 0 for k in counters) for row in active):
        raise RuntimeError('Decodium audio diagnostics failed; artifacts ' + str(root))
    print('PASS QSSTV TX -> Decodium BSR -> QSSTV repair', variant, 'exact bytes', len(original.read_bytes()), 'artifacts', root, flush=True)
finally:
    for process in processes:
        if process.poll() is None:
            process.terminate()
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
