#!/usr/bin/env python3
"""Manual HAMDRM two-app audio proof using BlackHole 2ch on macOS."""
import json
import os
from pathlib import Path
import re
import secrets
import socket
import socketserver
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time

variant=sys.argv[1]
if variant not in ('a','b'): raise SystemExit('expected a or b')
if len(sys.argv) > 3 or (len(sys.argv) == 3 and sys.argv[2] not in ('hybrid', 'hybrid-ftp', 'hybrid-upload', 'hybrid-dot', 'hybrid-cancel-upload', 'hybrid-cancel-download', 'bsr', 'bsr-repeat', 'rs1', 'rs2', 'rs3', 'rs4', 'rs1-loss', 'rs4-loss', 'rs4-final-loss')):
    raise SystemExit('optional second argument must be bsr, bsr-repeat, rs1-rs4 or an RS loss test')
rs_level = int(sys.argv[2][2]) if len(sys.argv) == 3 and sys.argv[2].startswith('rs') else 0
drop_segment = len(sys.argv) == 3 and sys.argv[2].endswith('-loss')
drop_final = len(sys.argv) == 3 and sys.argv[2] == 'rs4-final-loss'
bsr = len(sys.argv) == 3 and sys.argv[2] in ('bsr', 'bsr-repeat')
repeat = bsr and sys.argv[2] == 'bsr-repeat'
hybrid = len(sys.argv) == 3 and sys.argv[2] in ('hybrid', 'hybrid-ftp', 'hybrid-upload', 'hybrid-dot', 'hybrid-cancel-upload', 'hybrid-cancel-download')
hybrid_ftp = len(sys.argv) == 3 and sys.argv[2] in ('hybrid-ftp', 'hybrid-upload', 'hybrid-dot', 'hybrid-cancel-upload', 'hybrid-cancel-download')
hybrid_upload = len(sys.argv) == 3 and sys.argv[2] in ('hybrid-upload', 'hybrid-cancel-upload')
hybrid_dot = len(sys.argv) == 3 and sys.argv[2] == 'hybrid-dot'
cancel_upload = len(sys.argv) == 3 and sys.argv[2] == 'hybrid-cancel-upload'
cancel_download = len(sys.argv) == 3 and sys.argv[2] == 'hybrid-cancel-download'
external_ftp_port = os.environ.get('DECODIUM_LAB_EXTERNAL_FTP_PORT') if hybrid_upload else None
legacy_check = os.environ.get('DECODIUM_LAB_VERIFY_LEGACY_RESTORE') == '1'
monitor_off_check = os.environ.get('DECODIUM_LAB_VERIFY_MONITOR_OFF') == '1'
if legacy_check and monitor_off_check: raise SystemExit('choose one monitor restoration check')
if (legacy_check or monitor_off_check) and os.environ.get('DECODIUM_LAB_CHECK_COMPLETED_INBOX') == '1':
    raise SystemExit('post-completion group checks require RX to remain active; run monitor restoration separately')
if legacy_check and (bsr or drop_segment): raise SystemExit('legacy restoration check requires a normal image transfer')
if monitor_off_check and (bsr or drop_segment): raise SystemExit('monitor-off check requires a normal image transfer')
rx_start_ms = int(os.environ.get('DECODIUM_LAB_HAMDRM_RX_START_MS', '3500'))
if not 2500 <= rx_start_ms <= 12000: raise SystemExit('invalid HAMDRM RX start time')
profile=os.environ.get('DECODIUM_HAMDRM_LAB_PROFILE', 'hamdrm-E-2500-high-16-QAM-long')
rx_profile=os.environ.get('DECODIUM_HAMDRM_LAB_RX_PROFILE', profile)
expected_fac_interleaver=os.environ.get('DECODIUM_LAB_EXPECT_FAC_INTERLEAVER')
if expected_fac_interleaver not in (None, 'short', 'long'):
    raise SystemExit('expected FAC interleaver must be short or long')
tx_output = os.environ.get('DECODIUM_LAB_HAMDRM_TX_OUTPUT', 'BlackHole 2ch')
if tx_output not in ('BlackHole 2ch', 'BlackHole 16ch'):
    raise SystemExit('unsupported laboratory TX output')
root=Path(tempfile.mkdtemp(prefix='decodium-hamdrm-two-app-'+variant+'-'))
binary=Path('build/decodium').resolve(strict=True)
image_override = os.environ.get('DECODIUM_LAB_HAMDRM_IMAGE')
duration_override = os.environ.get('DECODIUM_LAB_HAMDRM_DURATION_MS')
if (image_override or duration_override) and (rs_level or bsr):
    raise SystemExit('custom image and duration are only supported for ordinary image transfer')
fixture=Path(image_override or 'tests/sstv/fixtures/qsstv-image-'+variant+'.jpg').resolve(strict=True)
ftp_server = None
epsv_delimiter = os.environ.get('DECODIUM_LAB_FTP_EPSV_DELIMITER', '|')
if len(epsv_delimiter) != 1 or not 33 <= ord(epsv_delimiter) <= 126:
    raise SystemExit('EPSV delimiter must be one printable non-space ASCII character')
slow_ftp_interval_ms = int(os.environ.get('DECODIUM_LAB_FTP_SLOW_INTERVAL_MS', '0'))
slow_upload_ack_ms = int(os.environ.get('DECODIUM_LAB_FTP_UPLOAD_ACK_DELAY_MS', '0'))
if slow_ftp_interval_ms and (not hybrid_ftp or external_ftp_port or not 100 <= slow_ftp_interval_ms <= 10000):
    raise SystemExit('slow FTP fixture requires the internal hybrid FTP server and a 100–10000 ms interval')
if slow_upload_ack_ms and (not hybrid_upload or external_ftp_port or not 100 <= slow_upload_ack_ms <= 120000):
    raise SystemExit('delayed upload acknowledgement requires the internal hybrid upload server')
if hybrid_ftp and not external_ftp_port:
    class Handler(socketserver.StreamRequestHandler):
        def handle(self):
            self.wfile.write(b'220 local fixture\r\n')
            data_socket = None
            while True:
                line = self.rfile.readline(4096)
                if not line:
                    break
                verb, _, argument = line.rstrip(b'\r\n').partition(b' ')
                self.server.commands.append(verb)
                if verb == b'USER' and cancel_upload: continue
                if verb == b'USER': self.wfile.write(b'331 password\r\n')
                elif verb == b'PASS': self.wfile.write(b'230 authenticated\r\n')
                elif verb == b'TYPE': self.wfile.write(b'200 binary\r\n')
                elif verb == b'CWD': self.wfile.write(b'250 directory\r\n')
                elif verb == b'EPSV':
                    data_socket = socket.socket()
                    data_socket.bind(('127.0.0.1', 0))
                    data_socket.listen(1)
                    self.wfile.write(('229 entering (%s%s%s%d%s)\r\n' %
                                      (epsv_delimiter, epsv_delimiter, epsv_delimiter,
                                       data_socket.getsockname()[1], epsv_delimiter)).encode('ascii'))
                elif verb == b'RETR' and data_socket:
                    if cancel_download: continue
                    expected = (('%sde_TEST%s-1-image-%s.jpg' % ('.' if hybrid_dot else '', '1' if variant == 'a' else '2', variant))).encode()
                    if hybrid_upload and argument in self.server.files:
                        content = self.server.files[argument]
                    elif not hybrid_upload and argument == expected:
                        content = fixture.read_bytes()
                    else:
                        self.wfile.write(b'550 unknown file\r\n')
                        continue
                    self.wfile.write(b'150 opening\r\n')
                    connection, _ = data_socket.accept()
                    with connection:
                        if slow_ftp_interval_ms:
                            step = (len(content) + 7) // 8
                            started = time.monotonic()
                            for offset in range(0, len(content), step):
                                if offset: time.sleep(slow_ftp_interval_ms / 1000)
                                connection.sendall(content[offset:offset+step])
                            self.server.retrieval_seconds.append(time.monotonic() - started)
                        else:
                            connection.sendall(content)
                    data_socket.close()
                    self.wfile.write(b'226 complete\r\n')
                elif verb == b'STOR' and data_socket and hybrid_upload:
                    self.wfile.write(b'150 opening\r\n')
                    connection, _ = data_socket.accept()
                    chunks = []
                    with connection:
                        while True:
                            chunk = connection.recv(65536)
                            if not chunk: break
                            chunks.append(chunk)
                    self.server.files[argument] = b''.join(chunks)
                    data_socket.close()
                    if slow_upload_ack_ms:
                        before = time.monotonic()
                        time.sleep(slow_upload_ack_ms / 1000)
                        self.server.upload_ack_seconds.append(time.monotonic() - before)
                    self.wfile.write(b'226 complete\r\n')
                else: self.wfile.write(b'500 unsupported\r\n')
    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address = True
        daemon_threads = True
    ftp_server = Server(('127.0.0.1', 0), Handler)
    ftp_server.files = {}
    ftp_server.commands = []
    ftp_server.retrieval_seconds = []
    ftp_server.upload_ack_seconds = []
    threading.Thread(target=ftp_server.serve_forever, daemon=True).start()
duration_ms = int(duration_override) if duration_override else None
if duration_ms is not None and not 35000 <= duration_ms <= 600000:
    raise SystemExit('invalid custom duration')
if drop_segment:
    expanded = root / ('image-' + variant + '.jpg')
    expanded.write_bytes(fixture.read_bytes() + bytes(2500))
    fixture = expanded
common=['--test-mode','--lab-no-cat','--lab-audio-input','BlackHole 2ch',
        '--lab-reapply-ms','500']
processes=[]
try:
    for role in ('rx','tx'):
        env=os.environ.copy()
        env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',
                   DECODIUM_LAB_SETTINGS_DIR=str(root/(role+'-settings')))
        if role == 'rx' and (expected_fac_interleaver
                             or os.environ.get('DECODIUM_LAB_CHECK_COMPLETED_INBOX') == '1'
                             or os.environ.get('DECODIUM_LAB_EXPECT_TEMPORAL_GROUPS') == '1'):
            env['DECODIUM_HAMDRM_RX_TRACE'] = '1'
        if hybrid:
            env['DECODIUM_LAB_HAMDRM_HYBRID'] = variant
        if hybrid_ftp:
            env['DECODIUM_LAB_HYBRID_FTP_PORT'] = external_ftp_port or str(ftp_server.server_address[1])
        if hybrid_dot:
            env['DECODIUM_LAB_HAMDRM_HYBRID_DOT'] = '1'
        if hybrid_upload and role == 'tx':
            env['DECODIUM_LAB_HYBRID_UPLOAD'] = '1'
        if cancel_upload and role == 'tx': env['DECODIUM_LAB_CANCEL_HYBRID_UPLOAD'] = '1'
        if cancel_download and role == 'rx': env['DECODIUM_LAB_CANCEL_HYBRID_DOWNLOAD'] = '1'
        if role == 'tx' and rs_level:
            env['DECODIUM_LAB_HAMDRM_RS_LEVEL'] = str(rs_level)
        if role == 'rx' and drop_segment:
            env['DECODIUM_LAB_DROP_RS_SEGMENT'] = '2' if drop_final else '1'
            if drop_final:
                env['DECODIUM_LAB_RS_RECOVERY_TRIGGER'] = str(root/'tx-complete')
        if role == 'tx' and bsr:
            env['DECODIUM_LAB_HAMDRM_BSR'] = variant
            if repeat:
                env['DECODIUM_LAB_HAMDRM_BSR_REPEAT'] = '1'
        args=[str(binary),*common,'--lab-audio-output',tx_output if role == 'tx' else 'BlackHole 2ch',
              '--rig-name','DRM'+role+secrets.token_hex(4)]
        if role=='rx':
            args+= (['--lab-no-monitor'] if monitor_off_check else ['--lab-monitor-ms','1800'])
            args+=['--lab-sstv-rx-ms',str(rx_start_ms),
                   '--lab-hamdrm','--lab-hamdrm-profile',rx_profile,'--lab-sstv-rx-ready-file',str(root/'ready'),
                   '--lab-sstv-rx-image-file',str(root/'received.jpg'),'--lab-quit-ms',str(duration_ms+2000 if duration_ms else 75000 if drop_segment else 55000 if repeat else 35000)]
        else:
            args+=['--lab-no-monitor','--lab-sstv-tx-ms','4000','--lab-hamdrm','--lab-hamdrm-profile',profile,
                   '--lab-sstv-tx-wait-file',str(root/'ready'),'--lab-sstv-tx-wait-timeout-ms','20000',
                   '--lab-sstv-image',str(fixture),
                   '--lab-sstv-vox','--lab-quit-ms',str(duration_ms if duration_ms else 73000 if drop_segment else 53000 if repeat else 33000)]
        with (root/(role+'.log')).open('wb') as log:
            processes.append(subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT))
        if role=='rx':
            deadline=time.monotonic()+20
            while not (root/'ready').exists():
                if processes[0].poll() is not None or time.monotonic()>deadline:
                    raise RuntimeError('RX did not become ready; artifacts '+str(root))
                time.sleep(.1)
            ready=json.loads((root/'ready').read_text())
            if ready.get('sourceDevice')!='BlackHole 2ch':
                raise RuntimeError('wrong RX audio device: '+str(ready.get('sourceDevice')))
            if os.environ.get('DECODIUM_LAB_FAIL_STORAGE_INIT') == '1':
                if int(os.environ.get('DECODIUM_LAB_SSTV_STORAGE_DELAY_MS', '0')) < 10000:
                    raise RuntimeError('storage failure injection needs an initialization delay')
                storage_root = Path(ready['labStorageRoot'])
                storage_root.parent.mkdir(parents=True, exist_ok=True)
                with storage_root.open('x') as blocker:
                    blocker.write('isolated test fixture: not a directory\n')
            if monitor_off_check:
                lines = (root/'rx.log').read_text(errors='replace').splitlines()
                initial = [line for line in lines if '[LAB][HAMDRM] legacy before RX' in line]
                if len(initial) != 1:
                    raise RuntimeError('monitor-off baseline missing; artifacts '+str(root))
                before = json.loads(initial[0][initial[0].index('{'):])
                if before.get('monitoring') or before.get('legacyBackendMonitoring') or before.get('legacyBackendStartPending'):
                    raise RuntimeError('monitor was not off before HAMDRM: '+str(before)+'; artifacts '+str(root))
            if legacy_check and not (ready.get('legacyRxSelected') and ready.get('legacyBackendAvailable')
                                     and ready.get('sstvRestoresLegacyMonitoring')
                                     and ready.get('localAudioTapConnected')):
                raise RuntimeError('RX did not hand legacy monitoring to native SSTV capture: '+str(ready)+'; artifacts '+str(root))
    deadline=time.monotonic()+(duration_ms/1000+15 if duration_ms else 85 if drop_segment else 65 if repeat else 45)
    while any(p.poll() is None for p in processes):
        if drop_final and not (root/'tx-complete').exists():
            tx_log = (root/'tx.log').read_text(errors='replace')
            if '[LAB][HAMDRM] TX state "Completed"' in tx_log:
                (root/'tx-complete').touch()
        if any(p.poll() not in (None,0) for p in processes): raise RuntimeError('application failed')
        if time.monotonic()>deadline: raise RuntimeError('applications timed out')
        time.sleep(.5)
    tx=(root/'tx.log').read_text(errors='replace')
    rx=(root/'rx.log').read_text(errors='replace')
    if (os.environ.get('DECODIUM_LAB_HAMDRM_TEMPORAL_PILOTS') == '1'
        and '[LAB][HAMDRM] experimental temporal pilot estimator' not in rx):
        raise RuntimeError('temporal pilot estimator did not start; artifacts '+str(root))
    if cancel_upload:
        if ('[LAB][HAMDRM] hybrid upload cancelAll true' not in tx
            or '[LAB][HAMDRM] TX state "Cancelled"' not in tx
            or '[LAB][HAMDRM] TX state "Completed"' in tx
            or (root/'received.jpg').exists() or ftp_server.files
            or b'USER' not in ftp_server.commands or b'STOR' in ftp_server.commands):
            raise RuntimeError('upload cancellation failed; artifacts '+str(root))
        print('PASS two Decodium apps: cancelled hybrid upload',variant,'artifacts',root,flush=True)
        sys.exit(0)
    if cancel_download:
        if ('[LAB][HAMDRM] hybrid download cancelAll true' not in rx
            or '[LAB][HAMDRM] hybrid FTP finished false' not in rx
            or not (root/'received.jpg').exists() or (root/'received.jpg.download').exists()
            or b'RETR' not in ftp_server.commands):
            raise RuntimeError('download cancellation failed; artifacts '+str(root))
        print('PASS two Decodium apps: cancelled hybrid download',variant,'artifacts',root,flush=True)
        sys.exit(0)
    if '[LAB][HAMDRM] TX state "Completed"' not in tx:
        raise RuntimeError('HAMDRM TX did not complete; artifacts '+str(root))
    if rs_level and ('[LAB][HAMDRM] RS level ' + str(rs_level)) not in tx:
        raise RuntimeError('requested RS protection was not selected; artifacts '+str(root))
    if (os.environ.get('DECODIUM_LAB_HAMDRM_USER_TIMING') == '1'
        and '[HAMDRM] operator selected experimental adaptive dual timing' not in rx):
        raise RuntimeError('operator-selected experimental timing was not active; artifacts '+str(root))
    if (os.environ.get('DECODIUM_LAB_HAMDRM_USER_TIMING') == '1'
        and '[HAMDRM] operator selected fractional timing candidate' not in rx):
        raise RuntimeError('operator-selected fractional timing was not active; artifacts '+str(root))
    if (os.environ.get('DECODIUM_LAB_HAMDRM_USER_TIMING') == '1'
        and '[HAMDRM] operator selected CRC-gated packet retries' not in rx):
        raise RuntimeError('operator-selected packet retries were not active; artifacts '+str(root))
    if (os.environ.get('DECODIUM_LAB_HAMDRM_USER_TIMING') == '1'
        and '[HAMDRM] operator selected temporal pilot candidate' not in rx):
        raise RuntimeError('operator-selected temporal receiver was not active; artifacts '+str(root))
    if (os.environ.get('DECODIUM_LAB_EXPECT_TEMPORAL_GROUPS') == '1'
        and not re.search(r'\[HAMDRM RX temporal timing\] groups\s+[1-9]\d*', rx)):
        raise RuntimeError('temporal receiver produced no validated groups; artifacts '+str(root))
    if (os.environ.get('DECODIUM_LAB_TEMPORAL_FRAME_LIMIT')
        and ('[LAB][HAMDRM] temporal frame limit' not in rx
             or '[HAMDRM] temporal pilot candidate stopped; other receivers continue:' not in rx
             or '[Bridge] HAMDRM rx: HAMDRM receive frame limit exceeded' in rx)):
        raise RuntimeError('temporal failure was not isolated; artifacts '+str(root))
    if (os.environ.get('DECODIUM_LAB_AUTO_EARLY_FRAME_LIMIT')
        and ('[LAB][HAMDRM] automatic early frame limit' not in rx
             or '[HAMDRM] automatic early receiver stopped; primary continues:' not in rx
             or '[Bridge] HAMDRM rx: HAMDRM receive frame limit exceeded' in rx)):
        raise RuntimeError('automatic early receiver failure was not isolated; artifacts '+str(root))
    if (os.environ.get('DECODIUM_LAB_HAMDRM_USER_TIMING') != '1'
        and '[HAMDRM] operator selected fractional timing candidate' in rx):
        raise RuntimeError('operator-selected timing unexpectedly active; artifacts '+str(root))
    if image_override or os.environ.get('DECODIUM_LAB_HAMDRM_USER_TIMING') == '1':
        diagnostics = [json.loads(line.split('[LAB][SSTV] RX diagnostics ', 1)[1])
                       for line in rx.splitlines() if '[LAB][SSTV] RX diagnostics ' in line]
        if not diagnostics or any(item.get(key) != 0 for item in diagnostics for key in
                                  ('discontinuities', 'droppedChunks', 'hamdrmRejectedChunks',
                                   'hamdrmQueueOverflows', 'processingFailures')):
            raise RuntimeError('HAMDRM audio diagnostics report loss; artifacts '+str(root))
    if (os.environ.get('DECODIUM_LAB_HAMDRM_SELECTIVE') == '1'
        and os.environ.get('DECODIUM_LAB_HAMDRM_FLAT_FALLBACK') == '1'
        and '[LAB][HAMDRM] experimental CRC-gated flat packet fallback' not in rx):
        raise RuntimeError('requested fallback was not active; artifacts '+str(root))
    if legacy_check:
        if '[Bridge] Reception started via legacy backend - FT8' not in rx:
            raise RuntimeError('legacy FT8 monitor was not observed before HAMDRM; artifacts '+str(root))
        initial = [line for line in rx.splitlines() if '[LAB][HAMDRM] legacy before RX' in line]
        if len(initial) != 1:
            raise RuntimeError('legacy state before RX missing; artifacts '+str(root))
        before = json.loads(initial[0][initial[0].index('{'):])
        if not (before.get('legacyRxSelected') and before.get('legacyBackendAvailable')
                and before.get('monitoring') and (before.get('legacyBackendMonitoring')
                                                  or before.get('legacyBackendStartPending'))):
            raise RuntimeError('legacy monitor was neither active nor pending before RX: '+str(before)+'; artifacts '+str(root))
        expected_state = os.environ.get('DECODIUM_LAB_EXPECT_LEGACY_STATE')
        if expected_state == 'pending' and not before.get('legacyBackendStartPending'):
            raise RuntimeError('legacy pending startup was not exercised; artifacts '+str(root))
        if expected_state == 'active' and not before.get('legacyBackendMonitoring'):
            raise RuntimeError('active legacy monitor was not exercised; artifacts '+str(root))
        during = []
        for line in rx.splitlines():
            if '[LAB][SSTV] RX diagnostics ' in line:
                snapshot = json.loads(line.split('[LAB][SSTV] RX diagnostics ', 1)[1])
                if snapshot.get('requested') and snapshot.get('hamdrmActive'):
                    during.append(snapshot)
        if not during or any(item.get('legacyBackendMonitoring') or item.get('legacyBackendStartPending') for item in during):
            raise RuntimeError('legacy audio became active while HAMDRM owned capture; artifacts '+str(root))
        markers = [line for line in rx.splitlines() if '[LAB][HAMDRM] legacy restore' in line]
        if len(markers) != 1 or 'cancelled= true' not in markers[0]:
            raise RuntimeError('legacy restoration result missing; artifacts '+str(root))
        restored = json.loads(markers[0][markers[0].index('{'):])
        if not (restored.get('legacyRxSelected') and restored.get('legacyBackendAvailable')
                and restored.get('legacyBackendMonitoring') and restored.get('monitoring')
                and not restored.get('requested') and not restored.get('hamdrmActive')):
            raise RuntimeError('legacy monitor did not resume after HAMDRM: '+str(restored)+'; artifacts '+str(root))
        if not (restored.get('legacyPcmSpectrumFeed')
                and restored.get('legacyPcmLastSampleMs', 0) >= restored.get('legacyTestRestoreStartedMs', 1)):
            raise RuntimeError('legacy PCM did not resume after HAMDRM: '+str(restored)+'; artifacts '+str(root))
    if monitor_off_check:
        initial = [line for line in rx.splitlines() if '[LAB][HAMDRM] legacy before RX' in line]
        if len(initial) != 1:
            raise RuntimeError('monitor-off baseline missing; artifacts '+str(root))
        before = json.loads(initial[0][initial[0].index('{'):])
        if before.get('monitoring') or before.get('legacyBackendMonitoring') or before.get('legacyBackendStartPending'):
            raise RuntimeError('monitor was not off before HAMDRM: '+str(before)+'; artifacts '+str(root))
        markers = [line for line in rx.splitlines() if '[LAB][HAMDRM] legacy restore' in line]
        if len(markers) != 1 or 'cancelled= true' not in markers[0]:
            raise RuntimeError('monitor-off result missing; artifacts '+str(root))
        restored = json.loads(markers[0][markers[0].index('{'):])
        if restored.get('monitoring') or restored.get('legacyBackendMonitoring') or restored.get('requested') or restored.get('hamdrmActive'):
            raise RuntimeError('monitor did not remain off after HAMDRM: '+str(restored)+'; artifacts '+str(root))
    if drop_segment and '[LAB][HAMDRM] discarded RS body segment ' not in rx:
        raise RuntimeError('RS loss injection was not observed; artifacts '+str(root))
    if drop_final and '[LAB][HAMDRM] manual RS recovery true true' not in rx:
        raise RuntimeError('manual RS recovery did not succeed; artifacts '+str(root))
    received=root/'received.jpg'
    if expected_fac_interleaver and not re.search(
            r'\[HAMDRM RX trace\] frames \d+ FAC interleaver '+expected_fac_interleaver+r'\b', rx):
        raise RuntimeError('expected validated FAC interleaver was not observed; artifacts '+str(root))
    if '[LAB][HAMDRM] RX started true "'+rx_profile+'"' not in rx or not received.exists():
        raise RuntimeError('HAMDRM RX did not complete; artifacts '+str(root))
    if bsr:
        payload=received.read_bytes()
        separator_check = os.environ.get('DECODIUM_LAB_BSR_SEPARATORS') == '1'
        if separator_check and (b'\r\n\r\n' not in payload
                                or '[LAB][HAMDRM] BSR separator compatibility control enabled' not in tx):
            raise RuntimeError('BSR separator control was not transmitted; artifacts '+str(root))
        lines=[line for line in payload.decode('ascii').splitlines() if line]
        expected_id=0x2451 if variant=='a' else 0x5372
        missing = (['2','3'] if variant=='a' else ['2','-1','3']) if repeat else (['1','2','3'] if variant=='a' else ['1','-1','3'])
        expected=[str(expected_id),'H_OK','64'] + missing + ['-99']
        if lines[:len(expected)] != expected:
            raise RuntimeError('HAMDRM BSR requested unexpected blocks; artifacts '+str(root))
        if variant=='a' and (len(lines)!=len(expected)+2 or lines[-2]!='probe-a.jpg'):
            raise RuntimeError('HAMDRM extended BSR metadata differs; artifacts '+str(root))
        if variant=='b' and len(lines)!=len(expected):
            raise RuntimeError('HAMDRM compact BSR has unexpected metadata; artifacts '+str(root))
        if '[LAB][HAMDRM] RX BSR saved' not in rx or '[LAB][HAMDRM] BSR TX started true' not in tx:
            raise RuntimeError('HAMDRM BSR did not use the application path; artifacts '+str(root))
        if repeat and (rx.count('[LAB][HAMDRM] RX BSR saved') != 2
                       or '[LAB][HAMDRM] repeated BSR TX started true' not in tx):
            raise RuntimeError('two BSR requests were not received in one session; artifacts '+str(root))
    elif hybrid:
        expected = Path('tests/sstv/fixtures/qsstv-hybrid-server-'+variant+'.bin').read_bytes()
        if ((not hybrid_ftp and received.read_bytes() != expected)
            or (not hybrid_upload and '[LAB][HAMDRM] synthetic hybrid reference selected' not in tx)
            or (hybrid_upload and '[LAB][HAMDRM] hybrid upload and TX started true' not in tx)
            or '[LAB][HAMDRM] hybrid reference saved' not in rx
            or '[LAB][HAMDRM] RX object saved' in rx):
            raise RuntimeError('hybrid reference did not arrive intact as a reference; artifacts '+str(root))
        if hybrid_ftp and (not (root/'received.jpg.download').exists()
                           or (root/'received.jpg.download').read_bytes() != fixture.read_bytes()
                           or '[LAB][HAMDRM] hybrid FTP started true' not in rx
                           or '[LAB][HAMDRM] hybrid FTP finished true' not in rx):
            raise RuntimeError('hybrid FTP image download failed; artifacts '+str(root))
        if slow_ftp_interval_ms:
            if len(ftp_server.retrieval_seconds) != 1 or ftp_server.retrieval_seconds[0] < 30:
                raise RuntimeError('slow FTP transfer did not exceed 30 seconds; artifacts '+str(root))
            print('slow hybrid FTP elapsed seconds', round(ftp_server.retrieval_seconds[0], 2), flush=True)
        if slow_upload_ack_ms:
            if len(ftp_server.upload_ack_seconds) != 1 or ftp_server.upload_ack_seconds[0] < 30:
                raise RuntimeError('FTP upload acknowledgement was not delayed over 30 seconds; artifacts '+str(root))
            print('hybrid FTP upload acknowledgement seconds', round(ftp_server.upload_ack_seconds[0], 2), flush=True)
        if epsv_delimiter != '|':
            if not ftp_server or ftp_server.commands.count(b'EPSV') < (2 if hybrid_upload else 1):
                raise RuntimeError('alternative EPSV response was not used for both transfers; artifacts '+str(root))
            print('hybrid FTP EPSV delimiter', repr(epsv_delimiter), flush=True)
        if os.environ.get('DECODIUM_LAB_HYBRID_REDOWNLOAD') == '1':
            second = root/'received.jpg.download2'
            if ('[LAB][HAMDRM] hybrid FTP second download true' not in rx
                or rx.count('[LAB][HAMDRM] hybrid FTP finished true') != 2
                or not second.exists() or second.read_bytes() != fixture.read_bytes()):
                raise RuntimeError('hybrid repeat download failed; artifacts '+str(root))
        if hybrid_upload:
            if external_ftp_port:
                uploads = list(Path(os.environ['DECODIUM_LAB_EXTERNAL_FTP_ROOT']).glob('radio/HybridFiles1/*'))
                if len(uploads) != 1 or uploads[0].read_bytes() != fixture.read_bytes():
                    raise RuntimeError('independent FTP upload differs from original; artifacts '+str(root))
            elif len(ftp_server.files) != 1 or next(iter(ftp_server.files.values())) != fixture.read_bytes():
                raise RuntimeError('hybrid upload differs from original; artifacts '+str(root))
    elif os.environ.get('DECODIUM_LAB_HAMDRM_PNG_AS_JP2') == '1' and '[LAB][HAMDRM] PNG declared as JP2 for interoperability test' not in tx:
        raise RuntimeError('mislabelled PNG was not transmitted; artifacts '+str(root))
    elif os.environ.get('DECODIUM_LAB_HAMDRM_STRUCTURAL_ONLY') == variant:
        width = 16 if variant == 'a' else 17
        expected = bytes([0xff,0xd8,0xff,0xc0,0x00,0x11,0x08,0x00,width,0x00,width,0x03,
                          0x01,0x11,0x00,0x02,0x11,0x00,0x03,0x11,0x00,0xff,0xd9]).ljust(160, b'\0')
        if (received.read_bytes() != expected
            or '[LAB][HAMDRM] structural-only image fixture selected' not in tx
            or '[LAB][HAMDRM] image structural-only true' not in rx):
            raise RuntimeError('structural-only control did not remain distinguishable; artifacts '+str(root))
    elif received.read_bytes()!=fixture.read_bytes():
        raise RuntimeError('HAMDRM object bytes differ; artifacts '+str(root))
    if os.environ.get('DECODIUM_LAB_VERIFY_HAMDRM_SAVE') == '1':
        saved = root/'received.jpg.user-saved'
        if ('[LAB][HAMDRM] completed image saved by controller true' not in rx
            or not saved.exists() or saved.read_bytes() != fixture.read_bytes()):
            raise RuntimeError('controller did not save the received image exactly; artifacts '+str(root))
    if os.environ.get('DECODIUM_LAB_VERIFY_HAMDRM_GALLERY') == '1':
        rows = [json.loads(line.split('[LAB][HAMDRM] received Gallery result ', 1)[1])
                for line in rx.splitlines() if '[LAB][HAMDRM] received Gallery result ' in line]
        if os.environ.get('DECODIUM_LAB_FAIL_STORAGE_INIT') == '1':
            if (rows or '[LAB][HAMDRM] Gallery deferred until storage initialization' not in rx
                or '[LAB][HAMDRM] deferred Gallery storage failed' not in rx
                or not Path(ready['labStorageRoot']).is_file()):
                raise RuntimeError('storage failure was not safely reported; artifacts '+str(root))
        elif os.environ.get('DECODIUM_LAB_EXPECT_NO_HAMDRM_GALLERY') == '1':
            storage = Path(ready['labStorageRoot']).resolve(strict=True)
            with sqlite3.connect(storage/'sstv-index.sqlite3') as db:
                count = db.execute("SELECT COUNT(*) FROM sstv_images WHERE category=1 AND source='hamdrm'").fetchone()[0]
            if rows or count:
                raise RuntimeError('incomplete hybrid reference was archived; artifacts '+str(root))
        else:
            if len(rows) != 1 or not rows[0].get('ok'):
                raise RuntimeError('Gallery did not store exactly one received image; artifacts '+str(root))
            if int(os.environ.get('DECODIUM_LAB_SSTV_STORAGE_DELAY_MS', '0')) > 0:
                queued = rx.find('[LAB][HAMDRM] Gallery deferred until storage initialization')
                stored = rx.find('[LAB][HAMDRM] received Gallery result ')
                if queued < 0 or stored <= queued:
                    raise RuntimeError('image was not deferred before storage became ready; artifacts '+str(root))
            entry = rows[0]
            storage = Path(entry['storageRoot']).resolve(strict=True)
            if storage != Path(ready['labStorageRoot']).resolve(strict=True):
                raise RuntimeError('Gallery result used an unexpected storage root; artifacts '+str(root))
            image_path = Path(entry['imagePath']).resolve(strict=True)
            if not image_path.is_relative_to(storage / 'images' / 'received'):
                raise RuntimeError('received Gallery image path is invalid; artifacts '+str(root))
            with sqlite3.connect(storage/'sstv-index.sqlite3') as db:
                record = db.execute('SELECT category,source,mode,complete,digital,image_path FROM sstv_images WHERE id=?',
                                    (entry['id'],)).fetchone()
            if record != (1, 'hamdrm', profile, 1, 1, str(image_path)):
                raise RuntimeError('received Gallery database record differs: '+str(record)+'; artifacts '+str(root))
            from PIL import Image, ImageChops
            with Image.open(fixture) as original, Image.open(image_path) as stored:
                first, second = original.convert('RGB'), stored.convert('RGB')
                if first.size != second.size or ImageChops.difference(first, second).getbbox() is not None:
                    raise RuntimeError('Gallery image pixels differ; artifacts '+str(root))
    if os.environ.get('DECODIUM_LAB_CHECK_COMPLETED_INBOX') == '1':
        saved = rx.find('[LAB][HAMDRM] RX object saved')
        snapshots = [json.loads(line.split('[LAB][HAMDRM] RX objects ', 1)[1])
                     for line in rx[saved:].splitlines()
                     if '[LAB][HAMDRM] RX objects ' in line]
        before = [int(n) for n in re.findall(r'\[HAMDRM RX trace\][^\n]*\bgroups\s+(\d+)', rx[:saved])]
        after = [int(n) for n in re.findall(r'\[HAMDRM RX trace\][^\n]*\bgroups\s+(\d+)', rx[saved:])]
        if (saved < 0 or not snapshots or not before or not after or max(after) <= max(before)
            or any(item.get('error') or not any(row.get('state') == 'complete'
                                                for row in item.get('inbox', []))
                   for item in snapshots)):
            raise RuntimeError('completed inbox did not remain error-free; artifacts '+str(root))
    print('PASS two Decodium apps:',variant,'exact bytes',len(received.read_bytes()),'artifacts',root,flush=True)

finally:
    if ftp_server:
        ftp_server.shutdown()
        ftp_server.server_close()
    for process in processes:
        if process.poll() is None:
            process.kill()
            process.wait()
