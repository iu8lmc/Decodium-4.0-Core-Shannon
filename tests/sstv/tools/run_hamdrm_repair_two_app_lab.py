#!/usr/bin/env python3
"""Local HAMDRM selective-repair A/B laboratory over virtual audio."""
import json
import ctypes as c
import ctypes.util
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import time
from run_hamdrm_two_app_echo_lab import Info, Params

variant = sys.argv[1] if len(sys.argv) in (2, 3) else ''
if variant not in ('a', 'b'):
    raise SystemExit('expected a or b')
if len(sys.argv) == 3 and sys.argv[2] not in ('air', 'live'):
    raise SystemExit('optional second argument must be air or live')
live = len(sys.argv) == 3 and sys.argv[2] == 'live'
air = len(sys.argv) == 3
audio_loss = os.environ.get('DECODIUM_LAB_REPAIR_AUDIO_LOSS') == '1'
missing_final = os.environ.get('DECODIUM_LAB_DROP_MOT_FINAL') == '1'
external_wav = os.environ.get('DECODIUM_LAB_REPAIR_EXTERNAL_WAV')
external_qsstv = os.environ.get('DECODIUM_LAB_REPAIR_QSSTV_BINARY')
external_bsr = os.environ.get('DECODIUM_LAB_REPAIR_EXTERNAL_BSR_FILE')
qsstv_live_rx = os.environ.get('DECODIUM_LAB_REPAIR_QSSTV_LIVE_RX') == '1'
qsstv_live_repair = os.environ.get('DECODIUM_LAB_REPAIR_QSSTV_LIVE_REPAIR') == '1'
qsstv_single = os.environ.get('DECODIUM_LAB_REPAIR_QSSTV_SINGLE') == '1'
external_mode = bool(external_wav or external_qsstv)
if audio_loss and not live:
    raise SystemExit('audio loss requires live mode')
if missing_final and (not live or audio_loss):
    raise SystemExit('final segment injection requires live mode without relay audio loss')
if (external_wav and external_qsstv) or (qsstv_single and not qsstv_live_repair) or (qsstv_live_repair and not qsstv_live_rx) or (qsstv_live_rx and (not external_qsstv or external_bsr)) or (external_bsr and not external_mode) or (external_mode and not (external_bsr or qsstv_live_rx)) or (external_mode and (not live or not audio_loss)):
    raise SystemExit('external waveform and BSR require the live audio-loss laboratory')
if external_wav:
    external_wav = Path(external_wav).resolve(strict=True)
if external_qsstv:
    external_qsstv = Path(external_qsstv).resolve(strict=True)
if external_bsr:
    external_bsr = Path(external_bsr).resolve(strict=True)
root = Path(tempfile.mkdtemp(prefix='decodium-hamdrm-repair-' + variant + '-'))
if qsstv_live_rx:
    external_bsr = root / 'qsstv-live.bsr'
binary = Path('build/decodium').resolve(strict=True)
fixture = root / ('repair-' + variant + '.jpg')
fixture.write_bytes(Path('tests/sstv/fixtures/qsstv-image-' + variant + '.jpg').read_bytes() + bytes(2500))
profile = 'hamdrm-E-2500-high-16-QAM-long'
common = ['--test-mode', '--lab-no-cat', '--lab-audio-input', 'BlackHole 2ch',
          '--lab-audio-output', 'BlackHole 2ch', '--lab-reapply-ms', '500',
          '--lab-hamdrm', '--lab-hamdrm-profile', profile]
processes = []
qsstv_rx = None

def launch(role, args, extra):
    env = os.environ.copy()
    env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
               DECODIUM_LAB_SETTINGS_DIR=str(root / (role + '-settings')), **extra)
    if air:
        env['DECODIUM_LAB_AUDIO_LIFECYCLE'] = '1'
    if external_mode:
        env['DECODIUM_LAB_REPAIR_EXTERNAL_BSR'] = '1'
        env['DECODIUM_LAB_REPAIR_EXTERNAL_DONE'] = str(root / 'external-bsr-done')
    with (root / (role + '.log')).open('wb') as log:
        role_common = common.copy()
        if audio_loss and role == 'tx':
            role_common[role_common.index('--lab-audio-output') + 1] = 'BlackHole 16ch'
        p = subprocess.Popen([str(binary), *role_common, '--rig-name', 'REPAIR' + role + secrets.token_hex(4),
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
    initial_rx_ready = root / 'initial-rx-ready'
    if live:
        launch('rx', ['--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500',
                      '--lab-sstv-rx-ready-file', str(initial_rx_ready),
                      '--lab-sstv-rx-image-file', str(received), '--lab-quit-ms', '165000'],
               {'DECODIUM_LAB_REPAIR_LIVE': variant,
                'DECODIUM_LAB_DROP_MOT_UNTIL': '' if audio_loss else str(initial),
                'DECODIUM_LAB_REPAIR_INITIAL': str(initial),
                'DECODIUM_LAB_REPAIR_READY': str(ready)})
        wait_for(initial_rx_ready.exists, 30, 'initial receiver did not become ready')
        if json.loads(initial_rx_ready.read_text()).get('sourceDevice') != 'BlackHole 2ch':
            raise RuntimeError('initial RX is not using BlackHole; artifacts ' + str(root))
    if qsstv_live_rx:
        qsstv_rx_root = root / 'qsstv-rx-settings'
        qsstv_rx_root.mkdir()
        env = os.environ.copy()
        env.update(QT_QPA_PLATFORM='offscreen', QSSTV_BSR_LAB_ROOT=str(qsstv_rx_root),
                   QSSTV_BSR_TX_LIVE='1', QSSTV_BSR_RX_LIVE='1',
                   QSSTV_BSR_RX_READY=str(root / 'qsstv-rx-ready'),
                   QSSTV_BSR_RX_TRIGGER=str(initial), QSSTV_BSR_RX_OUTPUT=str(external_bsr),
                   QSSTV_BSR_RX_COMPACT='1' if variant == 'b' else '0',
                   QSSTV_BSR_RX_KEEP='1' if qsstv_live_repair else '0',
                   QSSTV_BSR_RX_IMAGE=str(root / 'qsstv-repaired.jpg') if qsstv_live_repair else '',
                   QSSTV_BSR_RX_SINGLE='1' if qsstv_single else '0',
                   QSSTV_BSR_RX_SEND=str(root / 'qsstv-send'),
                   QSSTV_BSR_RX_TX_DONE=str(root / 'qsstv-tx-done'))
        with (root / 'qsstv-rx.log').open('wb') as log:
            qsstv_rx = subprocess.Popen([str(external_qsstv)], env=env, stdout=log, stderr=subprocess.STDOUT)
        wait_for(lambda: (root / 'qsstv-rx-ready').exists() or qsstv_rx.poll() is not None,
                 25, 'QSSTV live receiver did not become ready')
        if qsstv_rx.poll() is not None or 'LAB live audio input=BlackHole 2ch output=BlackHole 2ch' not in (root / 'qsstv-rx.log').read_text(errors='replace'):
            raise RuntimeError('QSSTV live receiver failed or opened wrong audio device; artifacts ' + str(root))
    launch('tx', (['--lab-monitor-ms', '1800'] if air else ['--lab-no-monitor'])
           + ['--lab-sstv-tx-ms', '4000', '--lab-sstv-image', str(fixture),
                  '--lab-sstv-vox', '--lab-quit-ms', '150000']
           + (['--lab-sstv-tx-wait-file', str(initial_rx_ready), '--lab-sstv-tx-wait-timeout-ms', '20000'] if live else []),
           {'DECODIUM_LAB_REPAIR_VARIANT': variant, 'DECODIUM_LAB_REPAIR_INITIAL': str(initial),
            'DECODIUM_LAB_REPAIR_READY': str(ready), 'DECODIUM_LAB_REPAIR_AIR': '1' if air else '0'})
    wait_for(initial.exists, 90, 'initial image TX did not complete')
    if qsstv_live_rx:
        wait_for(lambda: external_bsr.exists() or qsstv_rx.poll() is not None,
                 20, 'QSSTV did not generate a live BSR')
        if ((not qsstv_live_repair and qsstv_rx.wait(timeout=5) != 0)
                or (qsstv_live_repair and qsstv_rx.poll() is not None)
                or not external_bsr.is_file() or not external_bsr.stat().st_size):
            raise RuntimeError('QSSTV live BSR generation failed; artifacts ' + str(root))
    if not live:
        launch('rx', ['--lab-monitor-ms', '1800', '--lab-sstv-rx-ms', '3500',
                      '--lab-sstv-rx-ready-file', str(ready), '--lab-sstv-rx-image-file', str(received),
                      '--lab-quit-ms', '100000'], {'DECODIUM_LAB_REPAIR_FIXTURE': str(fixture),
                       'DECODIUM_LAB_REPAIR_VARIANT': variant, 'DECODIUM_LAB_REPAIR_READY': str(ready),
                       'DECODIUM_LAB_REPAIR_AIR': '1' if air else '0'})
    wait_for(ready.exists, 45 if air else 20, 'receiver did not become ready')
    if json.loads(ready.read_text()).get('sourceDevice') != 'BlackHole 2ch':
        raise RuntimeError('receiver is not using BlackHole; artifacts ' + str(root))
    if qsstv_single:
        (root / 'qsstv-send').touch()
        wait_for(lambda: ((root / 'qsstv-tx-done').exists()
                          and 'LAB single process BSR TX stopped and RX restarted'
                          in (root / 'qsstv-rx.log').read_text(errors='replace'))
                         or qsstv_rx.poll() is not None,
                 90, 'QSSTV single process did not finish BSR transmission')
        log = (root / 'qsstv-rx.log').read_text(errors='replace')
        if (qsstv_rx.poll() is not None or 'LAB single process sending BSR' not in log
                or 'LAB single process BSR TX stopped and RX restarted' not in log):
            raise RuntimeError('QSSTV single process TX/RX transition failed; artifacts ' + str(root))
        (root / 'external-bsr-done').touch()
    elif external_qsstv:
        qsstv_root = root / 'qsstv-settings'
        qsstv_root.mkdir()
        env = os.environ.copy()
        env.update(QT_QPA_PLATFORM='offscreen', QSSTV_BSR_LAB_ROOT=str(qsstv_root),
                   QSSTV_BSR_TX_LIVE='1', QSSTV_BSR_TX_FILE=str(external_bsr))
        command = [str(external_qsstv), '--encode', str(external_bsr), '--drm', '--drm-mode', 'E',
                   '--drm-bw', '2.5', '--drm-qam', '16', '--drm-prot', 'high', '--drm-interleave', 'short']
        with (root / 'qsstv.log').open('wb') as log:
            qsstv = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
        qsstv_log = (root / 'qsstv.log').read_text(errors='replace')
        if qsstv.returncode or 'LAB live audio input=BlackHole 2ch output=BlackHole 2ch' not in qsstv_log:
            raise RuntimeError('QSSTV live BSR transmission failed; artifacts ' + str(root))
        (root / 'external-bsr-done').touch()
    elif external_wav:
        pcm = root / 'external-bsr.f32'
        subprocess.run(['ffmpeg', '-v', 'error', '-i', str(external_wav), '-f', 'f32le',
                        '-ar', '48000', '-ac', '2', str(pcm)], check=True, timeout=30)
        pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
        pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
        pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.c_void_p, c.POINTER(Params), c.c_double,
                                     c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
        pa.Pa_WriteStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
        if pa.Pa_Initialize(): raise RuntimeError('external BSR PortAudio initialization failed')
        stream = c.c_void_p()
        try:
            device = next(i for i in range(pa.Pa_GetDeviceCount())
                          if pa.Pa_GetDeviceInfo(i).contents.name == b'BlackHole 2ch')
            params = Params(device, 2, 1, .05, None)
            if pa.Pa_OpenStream(c.byref(stream), None, c.byref(params), 48000, 1024, 0, None, None):
                raise RuntimeError('cannot open external BSR audio')
            if pa.Pa_StartStream(stream): raise RuntimeError('cannot start external BSR audio')
            with pcm.open('rb') as source:
                while chunk := source.read(8192 * 8):
                    data = c.create_string_buffer(chunk)
                    if pa.Pa_WriteStream(stream, data, len(chunk) // 8):
                        raise RuntimeError('external BSR audio playback failed')
            pa.Pa_StopStream(stream)
        finally:
            if stream: pa.Pa_CloseStream(stream)
            pa.Pa_Terminate()
        (root / 'external-bsr-done').touch()
    wait_for(lambda: received.exists() and received.read_bytes() == fixture.read_bytes(), 70, 'repaired image not received')
    if received.read_bytes() != fixture.read_bytes():
        raise RuntimeError('repaired image differs; artifacts ' + str(root))
    tx = (root / 'tx.log').read_text(errors='replace')
    rx = (root / 'rx.log').read_text(errors='replace')
    if (os.environ.get('DECODIUM_LAB_HAMDRM_USER_TIMING') == '1'
            and '[HAMDRM] operator selected experimental adaptive dual timing' not in tx):
        raise RuntimeError('sender did not use selected adaptive timing; artifacts '+str(root))
    if (os.environ.get('DECODIUM_LAB_HAMDRM_USER_TIMING') == '1'
            and ('[HAMDRM] operator selected temporal pilot candidate' not in tx
                 or '[HAMDRM] operator selected temporal pilot candidate' not in rx)):
        raise RuntimeError('temporal candidate missing from repair session; artifacts '+str(root))
    rx_marker = '[LAB][REPAIR] live partial received' if live else '[LAB][REPAIR] RX partial seeded'
    if '[LAB][REPAIR] TX started true' not in tx or rx_marker not in rx:
        raise RuntimeError('repair path not confirmed; artifacts ' + str(root))
    if (os.environ.get('DECODIUM_LAB_BSR_SEPARATORS') == '1'
            and '[LAB][HAMDRM] BSR separator compatibility control enabled' not in rx):
        raise RuntimeError('formatted BSR control did not run; artifacts ' + str(root))
    expected_missing = (len(fixture.read_bytes()) + 1023) // 1024 - 1 if missing_final else 1
    expected_drop = '[LAB][HAMDRM] discarded initial MOT body segment ' + str(expected_missing) + (' final true' if missing_final else ' final false')
    if live and not audio_loss and expected_drop not in rx:
        raise RuntimeError('initial segment loss was not injected; artifacts ' + str(root))
    if missing_final and ('QVariant(int, ' + str(expected_missing) + ')') not in rx:
        raise RuntimeError('receiver did not request the expected final segment; artifacts ' + str(root))
    if air and ('[LAB][REPAIR] over-audio BSR received' not in tx
                or (not external_mode and '[LAB][REPAIR] over-audio BSR TX started true' not in rx)):
        raise RuntimeError('BSR did not travel through audio; artifacts ' + str(root))
    if external_mode:
        marker = '[LAB][REPAIR] received BSR '
        if tx.count(marker) != 1 or '[LAB][REPAIR] waiting for external BSR waveform' not in rx:
            raise RuntimeError('external BSR flow was not observed; artifacts ' + str(root))
        expected = external_bsr.read_text().rstrip('\n')
        if not tx.split(marker, 1)[1].startswith(expected + '\n'):
            raise RuntimeError('external BSR bytes differ; artifacts ' + str(root))
    external_bsr = os.environ.get('DECODIUM_LAB_BSR_UNKNOWN_FILE')
    if external_bsr:
        marker = '[LAB][REPAIR] received BSR '
        if ('transmitting matched external BSR with unknown segment size' not in rx
                or tx.count(marker) != 1):
            raise RuntimeError('external BSR path not observed; artifacts ' + str(root))
        expected = Path(external_bsr).read_text().rstrip('\n')
        if not tx.split(marker, 1)[1].startswith(expected + '\n'):
            raise RuntimeError('received BSR differs from independent source; artifacts ' + str(root))
    def repair_tx_completed():
        log = (root / 'tx.log').read_text(errors='replace')
        marker = '[LAB][REPAIR] TX started true'
        return marker in log and '[LAB][HAMDRM] TX state "Completed"' in log.split(marker, 1)[1]
    wait_for(repair_tx_completed, 15, 'repair TX did not complete')
    if qsstv_live_repair:
        qsstv_image = root / 'qsstv-repaired.jpg'
        wait_for(lambda: (qsstv_image.exists()
                          and 'LAB live RX completed' in (root / 'qsstv-rx.log').read_text(errors='replace'))
                         or qsstv_rx.poll() is not None,
                 20, 'QSSTV did not complete the repaired image')
        if (not qsstv_image.exists() or qsstv_image.read_bytes() != fixture.read_bytes()
                or 'LAB live RX completed' not in (root / 'qsstv-rx.log').read_text(errors='replace')):
            raise RuntimeError('QSSTV repaired image differs or receiver failed; artifacts ' + str(root))
    if external_qsstv:
        tx = (root / 'tx.log').read_text(errors='replace')
        rx = (root / 'rx.log').read_text(errors='replace')
        if (rx.count('[LAB][HAMDRM] RX BSR saved') != 1
                or tx.count('[LAB][REPAIR] over-audio BSR received') != 1):
            raise RuntimeError('repeated BSR notification or missing BSR; artifacts ' + str(root))
        if 'HAMDRM transport ID already has a terminal inbox object' in rx:
            raise RuntimeError('repeated BSR caused a terminal inbox error; artifacts ' + str(root))
    if live:
        rx = (root / 'rx.log').read_text(errors='replace')
        diagnostics = [json.loads(line.split('[LAB][SSTV] RX diagnostics ', 1)[1])
                       for line in rx.splitlines() if '[LAB][SSTV] RX diagnostics ' in line]
        active = [row for row in diagnostics if row.get('hamdrmActive')]
        counters = ('discontinuities', 'droppedChunks', 'hamdrmRejectedChunks',
                    'hamdrmQueueOverflows', 'processingFailures')
        if not active or any(row.get('sourceDevice') != 'BlackHole 2ch'
                             or any(row.get(name) != 0 for name in counters)
                             for row in active):
            raise RuntimeError('live RX audio diagnostics failed; artifacts ' + str(root))
    print('PASS two Decodium apps repair', variant, 'exact bytes', len(received.read_bytes()),
          ('(audio loss; single QSSTV RX/TX/RX and exact QSSTV/Decodium repair on BlackHole)' if qsstv_single
           else '(audio loss; live QSSTV BSR and exact QSSTV/Decodium repair on BlackHole)' if qsstv_live_repair
           else '(audio loss; live QSSTV BSR and Decodium repair on BlackHole)' if external_qsstv
           else '(audio loss; external QSSTV BSR waveform and Decodium repair on BlackHole)' if external_wav
           else '(audio loss on initial TX; BSR and repair on BlackHole)' if audio_loss
           else '(initial TX, BSR and repair on BlackHole; decoded segment dropped)') if live
          else '(BSR and repair on BlackHole)' if air else '(BSR injected; PCM on BlackHole)',
          'artifacts', root, flush=True)
finally:
    if qsstv_rx and qsstv_rx.poll() is None:
        qsstv_rx.terminate()
        try: qsstv_rx.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qsstv_rx.kill()
            qsstv_rx.wait()
    for process in processes:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
