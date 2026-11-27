#!/usr/bin/env python3
"""Run isolated FLdigi alongside a real Decodium-to-Decodium Radiofax test.

macOS/BlackHole laboratory only; does not use CAT or physical PTT.
"""
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import xmlrpc.client

if len(sys.argv) != 2 or sys.argv[1] not in ('a', 'b'):
    raise SystemExit('expected a or b')
if shutil.disk_usage(tempfile.gettempdir()).free < 1024 ** 3:
    raise SystemExit('at least 1 GiB free temporary storage is required for this live lab')
root = Path(tempfile.mkdtemp(prefix='decodium-fldigi-live-'))
variant = sys.argv[1]
ioc = os.environ.get('DECODIUM_LAB_RADIOFAX_IOC', '576')
if ioc not in ('288', '576'):
    raise SystemExit('IOC must be 288 or 576')
line_rate = '60' if ioc == '288' else '120'
auto_center = os.environ.get('DECODIUM_FLDIGI_AUTOCENTER', '1')
if auto_center not in ('0', '1'):
    raise SystemExit('DECODIUM_FLDIGI_AUTOCENTER must be 0 or 1')
# Asymmetric, two-dimensional fixtures make row/column errors observable.
pixels = bytes(40 + ((x // 16 * (29 if variant == 'a' else 43)
                      + y // 15 * (37 if variant == 'a' else 23)
                      + (x // 16) * (y // 15) * 7) % 176)
               for y in range(120) for x in range(128))
(root / 'source.pgm').write_bytes(b'P5\n128 120\n255\n' + pixels)
(root / 'config').mkdir()
(root / 'home').mkdir()
(root / 'config' / 'fldigi.prefs').write_text('''; FLTK preferences file format 1.0
; vendor: w1hkj.org
; application: fldigi
[.]
version:4.2.11
dual_channels:YES
''')
(root / 'config' / 'fldigi_def.xml').write_text('''<FLDIGI_DEFS>
<AUDIOIO>1</AUDIOIO><PORTINDEVICE>BlackHole 2ch</PORTINDEVICE>
<PORTOUTDEVICE>BlackHole 2ch</PORTOUTDEVICE><PORTININDEX>-1</PORTININDEX>
<PORTOUTINDEX>-1</PORTOUTINDEX><CHKUSEHAMLIBIS>0</CHKUSEHAMLIBIS>
<CHKUSERIGCATIS>0</CHKUSERIGCATIS><CHKUSEXMLRPCIS>0</CHKUSEXMLRPCIS>
<TTYPTT>0</TTYPTT><RTSPTT>0</RTSPTT><DTRPTT>0</DTRPTT>
<CLIENT_TO_FLRIG>0</CLIENT_TO_FLRIG><ENABLE_AUDIO_ALERTS>0</ENABLE_AUDIO_ALERTS>
<WEFAX_AUTOCENTER>{}</WEFAX_AUTOCENTER>
</FLDIGI_DEFS>'''.format(auto_center))
with socket.socket() as listener:
    listener.bind(('127.0.0.1', 0))
    port = listener.getsockname()[1]
binary = os.environ.get('FLDIGI_BINARY', '/Applications/fldigi-4.2.11.app/Contents/MacOS/fldigi')
with (root / 'fldigi.log').open('wb') as log:
    proc = subprocess.Popen([binary, '--home-dir', str(root / 'home'), '--config-dir', str(root / 'config'),
                             '--xmlrpc-server-address', '127.0.0.1', '--xmlrpc-server-port', str(port),
                             '--debug-level', '5'], stdout=log, stderr=subprocess.STDOUT)
    child = None
    try:
        proxy = xmlrpc.client.ServerProxy('http://127.0.0.1:%d' % port, allow_none=True)
        deadline = time.monotonic() + 25
        while True:
            if proc.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError('isolated FLdigi did not start')
            try:
                proxy.fldigi.version()
                if Path(proxy.fldigi.config_dir()).resolve() != (root / 'config').resolve():
                    raise RuntimeError('unexpected FLdigi instance')
                break
            except RuntimeError:
                raise
            except Exception:
                time.sleep(.2)
        mode = next(x for x in proxy.modem.get_names() if 'WEFAX' in x and ioc in x)
        proxy.modem.set_by_name(mode)
        time.sleep(2)
        proxy.modem.set_carrier(1900)
        proxy.main.set_afc(False)
        env = os.environ.copy()
        env['DECODIUM_LAB_RADIOFAX_WMO_DEAD_SECTOR'] = '1'
        env['DECODIUM_LAB_RADIOFAX_IMAGE'] = str(root / 'source.pgm')
        env.pop('DECODIUM_LAB_FAX_CAPTURE', None)
        print('FLdigi WEFAX'+ioc+', 1900 Hz; artifacts', root, flush=True)
        with (root / 'decodium.log').open('wb') as child_log, (root / 'states.txt').open('w') as states:
            child = subprocess.Popen([sys.executable, 'tests/sstv/tools/run_radiofax_centered_two_app_lab.py',
                                      sys.argv[1], line_rate], env=env, stdout=child_log, stderr=subprocess.STDOUT,
                                     start_new_session=True)
            last = None
            while child.poll() is None:
                if proc.poll() is not None:
                    raise RuntimeError('FLdigi exited during Decodium test')
                state = proxy.wefax.state_string()
                if state != last:
                    states.write('%.3f %s\n' % (time.monotonic(), state))
                    states.flush()
                    last = state
                time.sleep(.2)
            if child.returncode:
                raise RuntimeError('Decodium A/B test failed; see ' + str(root / 'decodium.log'))
            time.sleep(3)
    finally:
        if child and child.poll() is None:
            os.killpg(child.pid, signal.SIGTERM)
            child.wait(timeout=5)
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        try:
            (root / 'fldigi-exit.txt').write_text(str(proc.returncode) + '\n')
        except OSError as error:
            print('could not save FLdigi exit code:', error, file=sys.stderr, flush=True)
        print('artifacts', root, flush=True)
