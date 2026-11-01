#!/usr/bin/env python3
"""Replay a recorded Radiofax signal into isolated FLdigi and trace its state."""
import ctypes as c, ctypes.util, os, socket, subprocess, sys, tempfile, threading, time, xmlrpc.client
from pathlib import Path
raw=Path(sys.argv[1]).resolve(strict=True)
if raw.stat().st_size <= 0 or raw.stat().st_size % 8:
 raise SystemExit('expected a non-empty stereo float32 capture')
duration=int(os.environ.get('DECODIUM_FLDIGI_LAB_SECONDS','50'))
if duration < 1 or duration > 180:
 raise SystemExit('laboratory playback duration must be between 1 and 180 seconds')
root=Path(tempfile.mkdtemp(prefix='decodium-fldigi-phase-'))
(root/'config').mkdir();(root/'home').mkdir()
(root/'config'/'fldigi_def.xml').write_text('''<FLDIGI_DEFS>
<AUDIOIO>1</AUDIOIO><PORTINDEVICE>BlackHole 2ch</PORTINDEVICE>
<PORTOUTDEVICE>BlackHole 2ch</PORTOUTDEVICE><PORTININDEX>-1</PORTININDEX>
<PORTOUTINDEX>-1</PORTOUTINDEX><CHKUSEHAMLIBIS>0</CHKUSEHAMLIBIS>
<CHKUSERIGCATIS>0</CHKUSERIGCATIS><CHKUSEXMLRPCIS>0</CHKUSEXMLRPCIS>
<TTYPTT>0</TTYPTT><RTSPTT>0</RTSPTT><DTRPTT>0</DTRPTT>
<CLIENT_TO_FLRIG>0</CLIENT_TO_FLRIG><ENABLE_AUDIO_ALERTS>0</ENABLE_AUDIO_ALERTS>
</FLDIGI_DEFS>''')
with socket.socket() as listener:
 listener.bind(('127.0.0.1',0)); port=listener.getsockname()[1]
fldigi=os.environ.get('FLDIGI_BINARY','/Applications/fldigi-4.2.11.app/Contents/MacOS/fldigi')
log=(root/'fldigi.log').open('wb')
proc=subprocess.Popen([fldigi,'--home-dir',str(root/'home'),'--config-dir',str(root/'config'), '--xmlrpc-server-address','127.0.0.1','--xmlrpc-server-port',str(port),'--debug-level','5'],stdout=log,stderr=subprocess.STDOUT)
stop=threading.Event(); thread=None
try:
 proxy=xmlrpc.client.ServerProxy('http://127.0.0.1:%d'%port,allow_none=True)
 deadline=time.monotonic()+25
 while True:
  if proc.poll() is not None or time.monotonic()>deadline: raise RuntimeError('fldigi did not start')
  try:
   proxy.fldigi.version()
   if Path(proxy.fldigi.config_dir()).resolve() != (root/'config').resolve():
    raise RuntimeError('unexpected FLdigi instance on laboratory port')
   break
  except RuntimeError: raise
  except Exception: time.sleep(.2)
 modes=proxy.modem.get_names(); name=next(x for x in modes if 'WEFAX' in x and '576' in x)
 proxy.modem.set_by_name(name);time.sleep(2);proxy.modem.set_carrier(1900);proxy.main.set_afc(False)
 print('mode',proxy.modem.get_name(),'carrier',proxy.modem.get_carrier(),'artifacts',root,flush=True)
 def trace():
  p=xmlrpc.client.ServerProxy('http://127.0.0.1:%d'%port,allow_none=True); last=None
  with (root/'states.txt').open('w') as f:
   while not stop.wait(.1):
    try:
     current='{} | status1={} | status2={}'.format(
         p.wefax.state_string(), p.main.get_status1(), p.main.get_status2())
     if current!=last:
      f.write('%.3f %s\n'%(time.monotonic(),current));f.flush();last=current
    except Exception as e: f.write('ERROR %s\n'%e);break
 thread=threading.Thread(target=trace);thread.start()
 class Info(c.Structure):
  _fields_=[('v',c.c_int),('name',c.c_char_p),('host',c.c_int),('inputs',c.c_int),('outputs',c.c_int),('li',c.c_double),('lo',c.c_double),('hi',c.c_double),('ho',c.c_double),('rate',c.c_double)]
 class Params(c.Structure):
  _fields_=[('device',c.c_int),('channels',c.c_int),('format',c.c_ulong),('latency',c.c_double),('hostinfo',c.c_void_p)]
 pa=c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib');pa.Pa_GetDeviceInfo.restype=c.POINTER(Info)
 pa.Pa_OpenStream.argtypes=[c.POINTER(c.c_void_p),c.c_void_p,c.POINTER(Params),c.c_double,c.c_ulong,c.c_ulong,c.c_void_p,c.c_void_p];pa.Pa_WriteStream.argtypes=[c.c_void_p,c.c_void_p,c.c_ulong]
 if pa.Pa_Initialize():raise RuntimeError('pa init')
 dev=next(i for i in range(pa.Pa_GetDeviceCount()) if pa.Pa_GetDeviceInfo(i).contents.name==b'BlackHole 2ch')
 params=Params(dev,2,1,.05,None);stream=c.c_void_p()
 if pa.Pa_OpenStream(c.byref(stream),None,c.byref(params),48000,1024,0,None,None):raise RuntimeError('pa open')
 if pa.Pa_StartStream(stream):raise RuntimeError('pa start')
 (root/'playback-start.txt').write_text('%.6f'%time.monotonic())
 with raw.open('rb') as src:
  remaining=duration*48000*8
  while remaining:
   chunk=src.read(min(8192*8,remaining))
   if not chunk:break
   buf=c.create_string_buffer(chunk)
   if pa.Pa_WriteStream(stream,buf,len(chunk)//8):raise RuntimeError('pa write')
   remaining-=len(chunk)
 pa.Pa_StopStream(stream);pa.Pa_CloseStream(stream);pa.Pa_Terminate()
 time.sleep(1)
finally:
 stop.set()
 if thread:thread.join(timeout=3)
 proc.terminate()
 try:proc.wait(timeout=5)
 except subprocess.TimeoutExpired:proc.kill();proc.wait()
 log.close()
 print('artifacts',root,flush=True)
