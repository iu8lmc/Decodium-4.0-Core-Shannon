#!/usr/bin/env python3
"""Bridge upstream QSSTV PulseAudio output during the two-Decodium repair lab."""
import ctypes as c, ctypes.util, os, queue, subprocess, sys, threading, time
from pathlib import Path
if len(sys.argv)!=2 or sys.argv[1] not in ('a','b'): raise SystemExit('expected a or b')
repo=Path(__file__).resolve().parents[3]
lab=Path(os.environ.get('DECODIUM_QSSTV_PULSE_ROOT','/tmp/decodium-qsstv-upstream-run')).resolve(strict=True)
root=lab / ('bridge-'+sys.argv[1]);root.mkdir(exist_ok=True)
class Info(c.Structure):
 _fields_=[('v',c.c_int),('name',c.c_char_p),('host',c.c_int),('inputs',c.c_int),('outputs',c.c_int),('li',c.c_double),('lo',c.c_double),('hi',c.c_double),('ho',c.c_double),('rate',c.c_double)]
class Params(c.Structure):
 _fields_=[('device',c.c_int),('channels',c.c_int),('format',c.c_ulong),('latency',c.c_double),('hostinfo',c.c_void_p)]
pa=c.CDLL(ctypes.util.find_library('portaudio') or '/opt/homebrew/lib/libportaudio.dylib');pa.Pa_GetDeviceInfo.restype=c.POINTER(Info);pa.Pa_OpenStream.argtypes=[c.POINTER(c.c_void_p),c.c_void_p,c.c_void_p,c.c_double,c.c_ulong,c.c_ulong,c.c_void_p,c.c_void_p];pa.Pa_WriteStream.argtypes=[c.c_void_p,c.c_void_p,c.c_ulong];assert pa.Pa_Initialize()==0
idx=next(i for i in range(pa.Pa_GetDeviceCount()) if pa.Pa_GetDeviceInfo(i).contents.name==b'BlackHole 2ch');param=Params(idx,2,1,.05,None);out=c.c_void_p();assert pa.Pa_OpenStream(c.byref(out),None,c.byref(param),48000,480,0,None,None)==0
pe=os.environ.copy();pe.update(PULSE_SERVER='unix:'+str(lab/'pulse/native'),XDG_CONFIG_HOME=str(lab/'config'))
rec=subprocess.Popen(['/opt/homebrew/opt/pulseaudio/bin/parec','--device=qsstv-output.monitor','--latency-msec=80','--process-time-msec=10','--format=float32le','--channels=2','--rate=48000','--raw'],env=pe,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,bufsize=0)
stop=threading.Event();q=queue.Queue(maxsize=200);errors=[];stats={'chunks':0,'active':0,'underflow':0};started=False

def producer():
 try:
  while not stop.is_set():
   data=b''
   while len(data)<3840 and not stop.is_set():
    b=rec.stdout.read(3840-len(data))
    if not b:break
    data+=b
   if len(data)!=3840:break
   try:q.put(data,timeout=.3)
   except queue.Full:errors.append('queue full');return
 except Exception as e:errors.append(str(e))

def bridge():
 global started
 try:
  while not stop.is_set() and q.qsize()<20:time.sleep(.01)
  if stop.is_set():return
  if pa.Pa_StartStream(out):errors.append('start failed');return
  started=True
  with (root/'relay.f32').open('wb') as f:
   while not stop.is_set():
    try: data=q.get(timeout=.3)
    except queue.Empty:errors.append('queue empty');return
    buf=(c.c_float*960).from_buffer_copy(data);stats['chunks']+=1;stats['active']+=max(abs(v) for v in buf)>.005; f.write(data)
    err=pa.Pa_WriteStream(out,buf,480)
    if err:stats['underflow']+=1;errors.append('write '+str(err));return
 except Exception as e:errors.append(str(e))
threads=[threading.Thread(target=producer,daemon=True),threading.Thread(target=bridge,daemon=True)]
for t in threads:t.start()
try:
 env=os.environ.copy()
 env.update(DECODIUM_LAB_REPAIR_QSSTV_BINARY='/tmp/decodium-qsstv-upstream-isolated-build/qsstv.app/Contents/MacOS/qsstv',
   DECODIUM_LAB_REPAIR_QSSTV_LIVE_RX='1',DECODIUM_LAB_REPAIR_QSSTV_LIVE_REPAIR='1',
   DECODIUM_LAB_REPAIR_QSSTV_SINGLE='1',DECODIUM_QSSTV_LAB_ROOT=str(lab),
   DYLD_FRAMEWORK_PATH='/tmp/decodium-qwt-qt5/lib',PULSE_SERVER='unix:'+str(lab/'pulse/native'),
   XDG_CONFIG_HOME=str(lab/'config'))
 result=subprocess.run([sys.executable,str(repo/'tests/sstv/tools/run_hamdrm_repair_audio_loss_lab.py'),sys.argv[1]],env=env,capture_output=True,text=True,timeout=190)
 print(result.stdout, end='',flush=True)
 if result.returncode: raise RuntimeError('repair harness failed: '+result.stderr)
 if errors or stats['underflow'] or stats['active']<100:raise RuntimeError('pulse relay invalid '+repr((errors,stats)))
 print('PASS upstream PulseAudio bridge',stats,flush=True)
finally:
 stop.set();rec.terminate();rec.wait(timeout=5)
 for thread in threads:thread.join(timeout=5)
 if started:pa.Pa_StopStream(out)
 pa.Pa_CloseStream(out);pa.Pa_Terminate()
 print('pulse bridge final',stats,'errors',errors,flush=True)
