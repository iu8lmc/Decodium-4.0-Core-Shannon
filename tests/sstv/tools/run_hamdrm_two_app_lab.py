#!/usr/bin/env python3
"""Manual HAMDRM two-app audio proof using BlackHole 2ch on macOS."""
import json
import os
from pathlib import Path
import re
import secrets
import subprocess
import sys
import tempfile
import time

variant=sys.argv[1]
if variant not in ('a','b'): raise SystemExit('expected a or b')
if len(sys.argv) > 3 or (len(sys.argv) == 3 and sys.argv[2] not in ('bsr', 'bsr-repeat', 'rs1', 'rs2', 'rs3', 'rs4', 'rs1-loss', 'rs4-loss', 'rs4-final-loss')):
    raise SystemExit('optional second argument must be bsr, bsr-repeat, rs1-rs4 or an RS loss test')
rs_level = int(sys.argv[2][2]) if len(sys.argv) == 3 and sys.argv[2].startswith('rs') else 0
drop_segment = len(sys.argv) == 3 and sys.argv[2].endswith('-loss')
drop_final = len(sys.argv) == 3 and sys.argv[2] == 'rs4-final-loss'
bsr = len(sys.argv) == 3 and sys.argv[2] in ('bsr', 'bsr-repeat')
repeat = bsr and sys.argv[2] == 'bsr-repeat'
legacy_check = os.environ.get('DECODIUM_LAB_VERIFY_LEGACY_RESTORE') == '1'
monitor_off_check = os.environ.get('DECODIUM_LAB_VERIFY_MONITOR_OFF') == '1'
if legacy_check and monitor_off_check: raise SystemExit('choose one monitor restoration check')
if legacy_check and (bsr or drop_segment): raise SystemExit('legacy restoration check requires a normal image transfer')
if monitor_off_check and (bsr or drop_segment): raise SystemExit('monitor-off check requires a normal image transfer')
rx_start_ms = int(os.environ.get('DECODIUM_LAB_HAMDRM_RX_START_MS', '3500'))
if not 2500 <= rx_start_ms <= 12000: raise SystemExit('invalid HAMDRM RX start time')
profile=os.environ.get('DECODIUM_HAMDRM_LAB_PROFILE', 'hamdrm-E-2500-high-16-QAM-long')
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
                   '--lab-hamdrm','--lab-hamdrm-profile',profile,'--lab-sstv-rx-ready-file',str(root/'ready'),
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
    if '[LAB][HAMDRM] TX state "Completed"' not in tx:
        raise RuntimeError('HAMDRM TX did not complete; artifacts '+str(root))
    if rs_level and ('[LAB][HAMDRM] RS level ' + str(rs_level)) not in tx:
        raise RuntimeError('requested RS protection was not selected; artifacts '+str(root))
    rx=(root/'rx.log').read_text(errors='replace')
    if (os.environ.get('DECODIUM_LAB_HAMDRM_USER_TIMING') == '1'
        and '[HAMDRM] operator selected experimental adaptive dual timing' not in rx):
        raise RuntimeError('operator-selected experimental timing was not active; artifacts '+str(root))
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
    if '[LAB][HAMDRM] RX started true "'+profile+'"' not in rx or not received.exists():
        raise RuntimeError('HAMDRM RX did not complete; artifacts '+str(root))
    if bsr:
        lines=received.read_text(encoding='ascii').splitlines()
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
    elif received.read_bytes()!=fixture.read_bytes():
        raise RuntimeError('HAMDRM object bytes differ; artifacts '+str(root))
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
    for process in processes:
        if process.poll() is None:
            process.kill()
            process.wait()
