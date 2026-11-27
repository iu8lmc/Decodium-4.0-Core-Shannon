#!/usr/bin/env python3
"""Replay pinned QSSTV-stage PCM over BlackHole into a full Decodium app.

Usage: python3 tests/sstv/tools/run_qsstv_pcm_app_lab.py a|b [binary]
Requires macOS, BlackHole 2ch and PortAudio. This is a manual real-time lab,
not a full QSSTV application or RF test. Artifacts are retained and printed.
"""
import ctypes as c, ctypes.util, json, os, pathlib, secrets, struct, subprocess, sys, tempfile, time
which=sys.argv[1]; assert which in ('a','b')
root=pathlib.Path(tempfile.mkdtemp(prefix='decodium-qsstv-live-'+which+'-')); profile='hamdrm-E-2500-high-16-QAM-short'
binary=pathlib.Path(sys.argv[2] if len(sys.argv)>2 else 'build/decodium').resolve(strict=True); src=pathlib.Path('tests/sstv/fixtures/qsstv-image-'+which+'-8c27d6d.pcm'); expected=pathlib.Path('tests/sstv/fixtures/qsstv-image-'+which+'.jpg').read_bytes()
env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=str(root/'settings'))
args=[str(binary),'--test-mode','--rig-name','QSSTVLIVE'+which+secrets.token_hex(4),'--lab-no-cat','--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 2ch','--lab-reapply-ms','500','--lab-monitor-ms','1800','--lab-sstv-rx-ms','3500','--lab-hamdrm','--lab-hamdrm-profile',profile,'--lab-sstv-rx-ready-file',str(root/'ready'),'--lab-sstv-rx-image-file',str(root/'received.jpg'),'--lab-quit-ms','20000']
with (root/'rx.log').open('wb') as log:
 proc=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT)
 try:
  deadline=time.monotonic()+18
  while not (root/'ready').exists():
   if proc.poll() is not None or time.monotonic()>deadline:raise Exception('rx not ready, log '+str(root))
   time.sleep(.1)
  ready=json.loads((root/'ready').read_text()); assert ready.get('sourceDevice')=='BlackHole 2ch',ready.get('sourceDevice')
  text=(root/'rx.log').read_text(errors='replace');assert '[LAB][HAMDRM] RX started true "'+profile+'"' in text
  samples=src.read_bytes(); pcm=bytearray(48000*8//2+len(samples)*4); offset=48000*8//2
  for i in range(0,len(samples),2):
   v=struct.unpack_from('<h',samples,i)[0]/32768.0
   struct.pack_into('ff',pcm,offset+(i//2)*8,v,v)
  class Info(c.Structure): _fields_=[('v',c.c_int),('name',c.c_char_p),('host',c.c_int),('inputs',c.c_int),('outputs',c.c_int),('li',c.c_double),('lo',c.c_double),('hi',c.c_double),('ho',c.c_double),('rate',c.c_double)]
  class Params(c.Structure): _fields_=[('device',c.c_int),('channels',c.c_int),('format',c.c_ulong),('latency',c.c_double),('hostinfo',c.c_void_p)]
  pa=c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib');pa.Pa_Initialize();pa.Pa_GetDeviceInfo.restype=c.POINTER(Info);pa.Pa_OpenStream.argtypes=[c.POINTER(c.c_void_p),c.c_void_p,c.POINTER(Params),c.c_double,c.c_ulong,c.c_ulong,c.c_void_p,c.c_void_p];pa.Pa_WriteStream.argtypes=[c.c_void_p,c.c_void_p,c.c_ulong]
  dev=next(i for i in range(pa.Pa_GetDeviceCount()) if pa.Pa_GetDeviceInfo(i).contents.name==b'BlackHole 2ch'); p=Params(dev,2,1,.05,None);stream=c.c_void_p();code=pa.Pa_OpenStream(c.byref(stream),None,c.byref(p),48000,1024,0,None,None)
  if code:raise Exception('open error '+str(code))
  if pa.Pa_StartStream(stream):raise Exception('start error')
  data=(c.c_char*len(pcm)).from_buffer(pcm); code=pa.Pa_WriteStream(stream,data,len(pcm)//8); print('write',code,flush=True);pa.Pa_StopStream(stream);pa.Pa_CloseStream(stream);pa.Pa_Terminate()
  if code:raise RuntimeError('PortAudio playback failed: '+str(code))
  proc.wait(timeout=25); received=root/'received.jpg'
  if proc.returncode or not received.exists() or received.read_bytes()!=expected:raise RuntimeError('fixture not reconstructed exactly: '+str(root))
  print('PASS image',which,'exact bytes',len(expected),'artifacts',root,flush=True)
 finally:
  if proc.poll() is None:proc.kill();proc.wait()
