#!/usr/bin/env python3
"""Capture an isolated FLdigi WEFAX transmission through BlackHole on macOS."""
import ctypes as c
import ctypes.util
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time
import xmlrpc.client

if len(sys.argv) != 2:
    raise SystemExit('expected an image path')
source = Path(sys.argv[1]).resolve(strict=True)
binary = Path(os.environ.get('FLDIGI_BINARY', '/Applications/fldigi-4.2.11.app/Contents/MacOS/fldigi')).resolve(strict=True)
root = Path(tempfile.mkdtemp(prefix='decodium-fldigi-tx-capture-'))
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
with (root / 'fldigi.log').open('wb') as log:
    process = subprocess.Popen([str(binary), '--home-dir', str(root / 'home'),
        '--config-dir', str(root / 'config'), '--xmlrpc-server-address', '127.0.0.1',
        '--xmlrpc-server-port', str(port), '--debug-level', '5'], stdout=log, stderr=subprocess.STDOUT)

stop = threading.Event()
thread = None
stream = c.c_void_p()
pa = None
errors = []
try:
    proxy = xmlrpc.client.ServerProxy('http://127.0.0.1:%d' % port, allow_none=True)
    deadline = time.monotonic() + 30
    while True:
        if process.poll() is not None or time.monotonic() > deadline:
            raise RuntimeError('FLdigi did not start; artifacts ' + str(root))
        try:
            proxy.fldigi.version()
            if Path(proxy.fldigi.config_dir()).resolve() != (root / 'config').resolve():
                raise RuntimeError('unexpected FLdigi instance on laboratory port')
            break
        except RuntimeError:
            raise
        except Exception:
            time.sleep(.2)
    name = next(x for x in proxy.modem.get_names() if 'WEFAX' in x and '576' in x)
    proxy.modem.set_by_name(name)
    time.sleep(2)
    proxy.modem.set_carrier(1900)
    proxy.main.set_afc(False)

    class Info(c.Structure):
        _fields_ = [('version', c.c_int), ('name', c.c_char_p), ('host', c.c_int),
            ('inputs', c.c_int), ('outputs', c.c_int), ('lowin', c.c_double),
            ('lowout', c.c_double), ('highin', c.c_double), ('highout', c.c_double), ('rate', c.c_double)]
    class Params(c.Structure):
        _fields_ = [('device', c.c_int), ('channels', c.c_int), ('format', c.c_ulong),
                    ('latency', c.c_double), ('hostinfo', c.c_void_p)]
    pa = c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib')
    pa.Pa_GetDeviceInfo.restype = c.POINTER(Info)
    pa.Pa_OpenStream.argtypes = [c.POINTER(c.c_void_p), c.POINTER(Params), c.c_void_p,
                                 c.c_double, c.c_ulong, c.c_ulong, c.c_void_p, c.c_void_p]
    pa.Pa_ReadStream.argtypes = [c.c_void_p, c.c_void_p, c.c_ulong]
    if pa.Pa_Initialize(): raise RuntimeError('PortAudio initialization failed')
    device = next(i for i in range(pa.Pa_GetDeviceCount()) if pa.Pa_GetDeviceInfo(i).contents.name == b'BlackHole 2ch')
    params = Params(device, 2, 1, .05, None)
    if pa.Pa_OpenStream(c.byref(stream), c.byref(params), None, 48000, 1024, 0, None, None):
        raise RuntimeError('cannot open BlackHole input')
    if pa.Pa_StartStream(stream): raise RuntimeError('cannot start BlackHole input')
    def capture():
        buffer = (c.c_float * 2048)()
        with (root / 'capture.f32').open('wb') as out:
            while not stop.is_set():
                result = pa.Pa_ReadStream(stream, buffer, 1024)
                if result:
                    errors.append(result)
                    break
                out.write(bytes(buffer))
    thread = threading.Thread(target=capture)
    thread.start()
    time.sleep(1)
    started = time.monotonic()
    result = proxy.wefax.send_file(str(source), 150)
    (root / 'transmission.txt').write_text('elapsed_seconds={:.3f}\nresult={!r}\n'.format(time.monotonic() - started, result))
    if result:
        raise RuntimeError('FLdigi send_file failed: ' + str(result))
    time.sleep(1)
    if errors: raise RuntimeError('PortAudio capture failed: ' + str(errors))
    print('PASS isolated FLdigi TX capture:', source.name, 'artifacts', root, flush=True)
finally:
    stop.set()
    if thread: thread.join(timeout=5)
    if pa and stream.value:
        pa.Pa_StopStream(stream)
        pa.Pa_CloseStream(stream)
        pa.Pa_Terminate()
    if process.poll() is None:
        process.terminate()
        try: process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    (root / 'fldigi-exit.txt').write_text(str(process.returncode) + '\n')
