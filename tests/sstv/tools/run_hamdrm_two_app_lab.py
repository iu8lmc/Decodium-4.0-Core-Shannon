#!/usr/bin/env python3
"""Manual FAX480 two-app audio proof using BlackHole 2ch on macOS."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import time

variant=sys.argv[1]
if variant not in ('a','b'): raise SystemExit('expected a or b')
if len(sys.argv) > 3 or (len(sys.argv) == 3 and sys.argv[2] not in ('bsr', 'bsr-repeat')):
    raise SystemExit('optional second argument must be bsr or bsr-repeat')
bsr = len(sys.argv) == 3
repeat = bsr and sys.argv[2] == 'bsr-repeat'
profile='hamdrm-E-2500-high-16-QAM-long'
root=Path(tempfile.mkdtemp(prefix='decodium-hamdrm-two-app-'+variant+'-'))
binary=Path('build/decodium').resolve(strict=True)
fixture=Path('tests/sstv/fixtures/qsstv-image-'+variant+'.jpg').resolve(strict=True)
common=['--test-mode','--lab-no-cat','--lab-audio-input','BlackHole 2ch',
        '--lab-audio-output','BlackHole 2ch','--lab-reapply-ms','500']
processes=[]
try:
    for role in ('rx','tx'):
        env=os.environ.copy()
        env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',
                   DECODIUM_LAB_SETTINGS_DIR=str(root/(role+'-settings')))
        if role == 'tx' and bsr:
            env['DECODIUM_LAB_HAMDRM_BSR'] = variant
            if repeat:
                env['DECODIUM_LAB_HAMDRM_BSR_REPEAT'] = '1'
        args=[str(binary),*common,'--rig-name','DRM'+role+secrets.token_hex(4)]
        if role=='rx':
            args+=['--lab-monitor-ms','1800','--lab-sstv-rx-ms','3500',
                   '--lab-hamdrm','--lab-hamdrm-profile',profile,'--lab-sstv-rx-ready-file',str(root/'ready'),
                   '--lab-sstv-rx-image-file',str(root/'received.jpg'),'--lab-quit-ms',str(55000 if repeat else 35000)]
        else:
            args+=['--lab-no-monitor','--lab-sstv-tx-ms','4000','--lab-hamdrm','--lab-hamdrm-profile',profile,
                   '--lab-sstv-tx-wait-file',str(root/'ready'),'--lab-sstv-tx-wait-timeout-ms','20000',
                   '--lab-sstv-image',str(fixture),
                   '--lab-sstv-vox','--lab-quit-ms',str(53000 if repeat else 33000)]
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
    deadline=time.monotonic()+(65 if repeat else 45)
    while any(p.poll() is None for p in processes):
        if any(p.poll() not in (None,0) for p in processes): raise RuntimeError('application failed')
        if time.monotonic()>deadline: raise RuntimeError('applications timed out')
        time.sleep(.5)
    tx=(root/'tx.log').read_text(errors='replace')
    if '[LAB][HAMDRM] TX state "Completed"' not in tx:
        raise RuntimeError('HAMDRM TX did not complete; artifacts '+str(root))
    rx=(root/'rx.log').read_text(errors='replace')
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
    print('PASS two Decodium apps:',variant,'exact bytes',len(received.read_bytes()),'artifacts',root,flush=True)

finally:
    for process in processes:
        if process.poll() is None:
            process.kill()
            process.wait()
