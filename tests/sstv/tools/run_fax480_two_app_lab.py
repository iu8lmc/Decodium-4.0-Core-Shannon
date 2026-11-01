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

profile=sys.argv[1]
if profile not in ('dayton-480','qsstv-500','libsstv-vis-480'):
    raise SystemExit('expected dayton-480, qsstv-500 or libsstv-vis-480')
root=Path(tempfile.mkdtemp(prefix='decodium-fax480-two-app-'+profile+'-'))
binary=Path('build/decodium').resolve(strict=True)
variant=sys.argv[2] if len(sys.argv)>2 else ('a' if profile=='dayton-480' else 'b')
if variant not in ('a','b'): raise SystemExit('image variant must be a or b')
if len(sys.argv) > 4 or (len(sys.argv) == 4 and sys.argv[3] != 'pattern'):
    raise SystemExit('optional fourth argument must be pattern')
if len(sys.argv) == 4:
    fixture = root / 'input.pgm'
    pixels = bytearray()
    for y in range(480):
        for x in range(512):
            if variant == 'a':
                value = (x // 3 + y // 4 + (45 if (x // 64 + y // 60) % 2 else 0)) % 256
            else:
                value = ((511 - x) // 4 + y // 2 + (70 if (x // 80) % 2 else 0)) % 256
            pixels.append(value)
    fixture.write_bytes(b'P5\n512 480\n255\n' + pixels)
else:
    fixture=Path('tests/sstv/fixtures/qsstv-image-'+variant+'.jpg').resolve(strict=True)
common=['--test-mode','--lab-no-cat','--lab-audio-input','BlackHole 2ch',
        '--lab-audio-output','BlackHole 2ch','--lab-reapply-ms','500']
processes=[]
try:
    for role in ('rx','tx'):
        env=os.environ.copy()
        env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',
                   DECODIUM_LAB_SETTINGS_DIR=str(root/(role+'-settings')))
        args=[str(binary),*common,'--rig-name','FAX'+role+secrets.token_hex(4)]
        if role=='rx':
            args+=['--lab-monitor-ms','1800','--lab-sstv-rx-ms','3500',
                   '--lab-sstv-fax480-rx',profile,'--lab-sstv-rx-ready-file',str(root/'ready'),
                   '--lab-sstv-rx-image-file',str(root/'received.png'),'--lab-quit-ms','165000']
        else:
            args+=['--lab-no-monitor','--lab-sstv-tx-ms','4000','--lab-sstv-fax480-tx',profile,
                   '--lab-sstv-tx-wait-file',str(root/'ready'),'--lab-sstv-tx-wait-timeout-ms','20000',
                   '--lab-sstv-image',str(fixture),'--lab-sstv-tx-image-file',str(root/'reference.png'),
                   '--lab-sstv-vox','--lab-quit-ms','160000']
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
    deadline=time.monotonic()+175
    while any(p.poll() is None for p in processes):
        if any(p.poll() not in (None,0) for p in processes): raise RuntimeError('application failed')
        if time.monotonic()>deadline: raise RuntimeError('applications timed out')
        time.sleep(.5)
    tx=(root/'tx.log').read_text(errors='replace')
    if '[LAB][FAX480] TX state Completed' not in tx:
        raise RuntimeError('TX did not report completion; artifacts '+str(root))
    rx=(root/'rx.log').read_text(errors='replace')
    lines=[json.loads(line.split(' RX diagnostics ',1)[1]) for line in rx.splitlines()
           if '[LAB][SSTV] RX diagnostics ' in line]
    if (not lines or not lines[-1]['imageComplete'] or lines[-1]['processingFailures']
            or lines[-1]['discontinuities'] or lines[-1]['imageCoverage'] < 1.0):
        raise RuntimeError('RX did not report a complete image; artifacts '+str(root))
    if not (root/'received.png').exists(): raise RuntimeError('RX did not save image; artifacts '+str(root))
    subprocess.run([sys.executable,'tests/sstv/tools/compare_sstv_lab_images.py',
                    str(root/'reference.png'),str(root/'received.png'),
                    '--max-mae','8','--max-p95','10'],check=True,timeout=30)
    print('PASS two Decodium apps:',profile,'coverage',lines[-1]['imageCoverage'],
          'discontinuities',lines[-1]['discontinuities'],'artifacts',root,flush=True)
finally:
    for process in processes:
        if process.poll() is None:
            process.kill()
            process.wait()
