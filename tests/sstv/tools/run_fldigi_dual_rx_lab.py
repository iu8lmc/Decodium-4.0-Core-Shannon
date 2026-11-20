#!/usr/bin/env python3
"""Compare FLdigi and Decodium receiving the same recorded FLdigi transmission.

macOS/BlackHole laboratory only; does not use CAT or physical PTT.
"""
import os
import json
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import xmlrpc.client

from fldigi_radiofax_metadata import has_radiofax_stop_metadata

if len(sys.argv) != 3:
    raise SystemExit('expected CAPTURE.f32 REFERENCE.png')
capture = Path(sys.argv[1]).resolve(strict=True)
reference = Path(sys.argv[2]).resolve(strict=True)
if shutil.disk_usage(tempfile.gettempdir()).free < 1024 ** 3:
    raise SystemExit('at least 1 GiB free temporary storage is required for this live lab')
root = Path(tempfile.mkdtemp(prefix='decodium-fldigi-dual-rx-'))
(root / 'config').mkdir()
(root / 'home').mkdir()
(root / 'config' / 'fldigi_def.xml').write_text('''<FLDIGI_DEFS>
<AUDIOIO>1</AUDIOIO><PORTINDEVICE>BlackHole 2ch</PORTINDEVICE>
<PORTOUTDEVICE>BlackHole 2ch</PORTOUTDEVICE><PORTININDEX>-1</PORTININDEX>
<PORTOUTINDEX>-1</PORTOUTINDEX><CHKUSEHAMLIBIS>0</CHKUSEHAMLIBIS>
<CHKUSERIGCATIS>0</CHKUSERIGCATIS><CHKUSEXMLRPCIS>0</CHKUSEXMLRPCIS>
<TTYPTT>0</TTYPTT><RTSPTT>0</RTSPTT><DTRPTT>0</DTRPTT>
<CLIENT_TO_FLRIG>0</CLIENT_TO_FLRIG><ENABLE_AUDIO_ALERTS>0</ENABLE_AUDIO_ALERTS>
</FLDIGI_DEFS>''')
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
        mode = next(x for x in proxy.modem.get_names() if 'WEFAX' in x and '576' in x)
        proxy.modem.set_by_name(mode)
        time.sleep(2)
        proxy.modem.set_carrier(1900)
        proxy.main.set_afc(False)
        env = os.environ.copy()
        print('FLdigi WEFAX576, 1900 Hz; artifacts', root, flush=True)
        with (root / 'decodium.log').open('wb') as child_log, (root / 'states.txt').open('w') as states:
            child = subprocess.Popen([sys.executable, 'tests/sstv/tools/run_fldigi_radiofax_replay_lab.py',
                                      str(capture), str(reference)], env=env, stdout=child_log, stderr=subprocess.STDOUT,
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
                raise RuntimeError('Decodium replay failed; see ' + str(root / 'decodium.log'))
            time.sleep(3)
        images = list((root / 'config' / 'images').glob('*.png'))
        if len(images) != 1:
            raise RuntimeError('expected exactly one FLdigi image; artifacts ' + str(root))
        raw = images[0].read_bytes()
        if not has_radiofax_stop_metadata(raw, 120):
            raise RuntimeError('FLdigi image lacks expected line rate or stop metadata; artifacts ' + str(root))
        analysis = json.loads(subprocess.check_output([
            sys.executable, 'tests/sstv/tools/analyze_radiofax_2d_alignment.py',
            str(reference), str(images[0]), '--max-rows', '8', '--max-columns', '32'
        ], timeout=60))
        (root / 'fldigi-alignment.json').write_text(json.dumps(analysis, indent=2) + '\n')
        direct = analysis['direct_full_raster']
        print('FLdigi direct raster MAE', direct['mae'], 'p95', direct['p95'], flush=True)
        if direct['mae'] > 8 or direct['p95'] > 4:
            raise RuntimeError('FLdigi raster failed unaligned quality limits; artifacts ' + str(root))
        print('PASS simultaneous Decodium and FLdigi RX with direct raster quality; FLdigi image', images[0], flush=True)
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
