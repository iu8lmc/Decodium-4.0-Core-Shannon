#!/usr/bin/env python3
"""Manual macOS BlackHole proof; run once each for 437 and 463 Hz.

Requires the Decodium test binary, PortAudio, ffmpeg and BlackHole 2ch.
The generator runs separately from Decodium and uses the loopback device.
Artifacts are retained in a unique temporary directory printed on success.
"""
import ctypes as c, ctypes.util, json, math, struct, subprocess, os, time, pathlib, sys, tempfile, secrets
freq=int(sys.argv[1]); assert freq in (437, 463); tag=str(freq); root=pathlib.Path(tempfile.mkdtemp(prefix='decodium-shift-stop-'+tag+'-'))
for n in ('ready','received.png'):
 try:(root/n).unlink()
 except FileNotFoundError:pass
binary=pathlib.Path(sys.argv[2] if len(sys.argv)>2 else 'build/decodium').resolve(strict=True); env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=str(root/'settings'))
args=[str(binary),'--test-mode','--rig-name','FAXSHIFT'+tag+secrets.token_hex(4),'--lab-no-cat','--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 2ch','--lab-reapply-ms','500','--lab-monitor-ms','1800','--lab-sstv-rx-ms','3500','--lab-sstv-fax-rx','576,128,32','--lab-sstv-rx-ready-file',str(root/'ready'),'--lab-sstv-rx-image-file',str(root/'received.png'),'--lab-quit-ms','36000']
with (root/'rx.log').open('wb') as f:
 proc=subprocess.Popen(args,env=env,stdout=f,stderr=subprocess.STDOUT)
 try:
  deadline=time.monotonic()+18
  while not (root/'ready').exists():
   if proc.poll() is not None or time.monotonic()>deadline:raise Exception('rx not ready')
   time.sleep(.1)
  ready=json.loads((root/'ready').read_text())
  if ready.get('sourceDevice') != 'BlackHole 2ch':
   raise RuntimeError('unexpected receive device: '+str(ready.get('sourceDevice')))
  sr=48000; seconds=18; frames=seconds*sr; pcm=bytearray(frames*8); phase=0.0
  for i in range(frames):
   t=i/sr
   if t < 2: hz=2300 if int(t*600)%2==0 else 1500
   elif t < 12: hz=2300 if ((i-2*sr)%(sr//4))<sr//8 else 1500
   elif t < 16:
    y=(i-12*sr)//(sr//4); x=((i-12*sr)%(sr//4))*128//(sr//4)
    val=(x*2 if freq==437 else (255-x*2)); hz=1500+800*val/255
   else: hz=freq
   phase += 2*math.pi*hz/sr
   sample=.25*math.sin(phase);struct.pack_into('ff',pcm,i*8,sample,sample)
  class Info(c.Structure): _fields_=[('v',c.c_int),('name',c.c_char_p),('host',c.c_int),('inputs',c.c_int),('outputs',c.c_int),('li',c.c_double),('lo',c.c_double),('hi',c.c_double),('ho',c.c_double),('rate',c.c_double)]
  class Params(c.Structure): _fields_=[('device',c.c_int),('channels',c.c_int),('format',c.c_ulong),('latency',c.c_double),('hostinfo',c.c_void_p)]
  pa=c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib');pa.Pa_Initialize();pa.Pa_GetDeviceInfo.restype=c.POINTER(Info);pa.Pa_OpenStream.argtypes=[c.POINTER(c.c_void_p),c.c_void_p,c.POINTER(Params),c.c_double,c.c_ulong,c.c_ulong,c.c_void_p,c.c_void_p];pa.Pa_WriteStream.argtypes=[c.c_void_p,c.c_void_p,c.c_ulong]
  dev=next(i for i in range(pa.Pa_GetDeviceCount()) if pa.Pa_GetDeviceInfo(i).contents.name==b'BlackHole 2ch'); p=Params(dev,2,1,.05,None);stream=c.c_void_p();code=pa.Pa_OpenStream(c.byref(stream),None,c.byref(p),sr,1024,0,None,None)
  if code:raise Exception('open error '+str(code))
  if pa.Pa_StartStream(stream):raise Exception('start error')
  data=(c.c_char*len(pcm)).from_buffer(pcm); code=pa.Pa_WriteStream(stream,data,frames); print('write',code,flush=True);pa.Pa_StopStream(stream);pa.Pa_CloseStream(stream);pa.Pa_Terminate()
  result=proc.wait(timeout=20)
  if result != 0 or not (root/'received.png').exists():
   raise RuntimeError('receiver exited without a terminal image')
  diagnostics=[]
  for line in (root/'rx.log').read_text(errors='replace').splitlines():
   if '[LAB][SSTV] RX diagnostics ' in line:
    diagnostics.append(json.loads(line.split(' RX diagnostics ',1)[1]))
  if not diagnostics:
   raise RuntimeError('missing receiver diagnostics')
  last=diagnostics[-1]
  if (last.get('sourceDevice') != 'BlackHole 2ch' or not last['imageRadiofaxStopDetected']
      or last['imageLinesPublished'] != 16 or last['processingFailures']):
   raise RuntimeError('unexpected stop or image diagnostics: '+str(last))
  raw=subprocess.check_output(['ffmpeg','-v','error','-i',str(root/'received.png'),
      '-pix_fmt','gray','-f','rawvideo','-'])
  if len(raw)!=128*32:
   raise RuntimeError('unexpected image dimensions')
  errors=[abs(raw[y*128+x]-(x*2 if freq==437 else 255-x*2))
      for y in range(16) for x in range(128)]
  mean=sum(errors)/len(errors)
  if mean>3:
   raise RuntimeError('received pixels differ: mean '+str(mean))
  print('PASS: {} Hz, 16 lines, stop detected, MAE {:.3f}, discontinuities {}, logs {}'.format(
      freq,mean,last['discontinuities'],root),flush=True)
 finally:
  if proc.poll() is None:proc.kill();proc.wait()
