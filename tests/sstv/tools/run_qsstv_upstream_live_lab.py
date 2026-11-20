#!/usr/bin/env python3
"""Experimental upstream QSSTV -> Decodium live-audio test; requires isolated PulseAudio."""
import ctypes as c, ctypes.util, json, os, queue, secrets, subprocess, sys, tempfile, threading, time
from pathlib import Path
if len(sys.argv) != 2 or sys.argv[1] not in ('a', 'b'): raise SystemExit('expected a or b')
repo=Path(__file__).resolve().parents[3]; lab=Path(os.environ.get('DECODIUM_QSSTV_PULSE_ROOT', '/tmp/decodium-qsstv-upstream-run')).resolve(strict=True);root=Path(tempfile.mkdtemp(prefix='decodium-qsstv-upstream-live-'+sys.argv[1]+'-'));image=root/'input.png'
pattern='smptebars=size=320x240:rate=1' if sys.argv[1]=='a' else 'smptehdbars=size=320x240:rate=1'
subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i',pattern,'-frames:v','1',str(image)],check=True)
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
rx=tx=None
try:
 env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=str(root/'rx-settings'))
 args=[str(repo/'build/decodium'),'--test-mode','--rig-name','UPQS'+secrets.token_hex(4),'--lab-no-cat','--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 2ch','--lab-reapply-ms','500','--lab-monitor-ms','1800','--lab-sstv-rx-ms','3500','--lab-hamdrm','--lab-hamdrm-profile','hamdrm-E-2500-high-16-QAM-long','--lab-sstv-rx-ready-file',str(root/'ready'),'--lab-sstv-rx-image-file',str(root/'received.jp2'),'--lab-quit-ms','90000']
 with (root/'rx.log').open('wb') as log:rx=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT)
 deadline=time.monotonic()+25
 while not (root/'ready').exists():
  if rx.poll() is not None or time.monotonic()>deadline:raise RuntimeError('RX not ready')
  time.sleep(.1)
 if json.loads((root/'ready').read_text()).get('sourceDevice')!='BlackHole 2ch':raise RuntimeError('wrong input device')
 txenv=pe.copy();txenv.update(DECODIUM_QSSTV_LAB_ROOT=str(root),DECODIUM_QSSTV_LAB_IMAGE=str(image),DECODIUM_QSSTV_LAB_READY=str(root/'ready'),DECODIUM_QSSTV_LAB_ORIGINAL=str(root/'original.jp2'),DYLD_FRAMEWORK_PATH='/tmp/decodium-qwt-qt5/lib',QT_QPA_PLATFORM='offscreen')
 with (root/'tx.log').open('wb') as log:tx=subprocess.Popen([os.environ.get('DECODIUM_QSSTV_UPSTREAM_BINARY', '/tmp/decodium-qsstv-upstream-isolated-build/qsstv.app/Contents/MacOS/qsstv')],env=txenv,stdout=log,stderr=subprocess.STDOUT)
 deadline=time.monotonic()+75
 while not (root/'received.jp2').exists():
  if errors or tx.poll() is not None or rx.poll() is not None or time.monotonic()>deadline:raise RuntimeError('image not received '+repr(errors))
  time.sleep(.2)
 time.sleep(2)
 if not (root/'original.jp2').exists() or (root/'original.jp2').read_bytes()!=(root/'received.jp2').read_bytes():raise RuntimeError('received bytes mismatch')
 if not (root/'original.jp2').read_bytes().startswith(bytes.fromhex('89504e470d0a1a0a')):raise RuntimeError('expected small PNG mislabelled as JP2')
 if errors or stats['underflow'] or stats['active']<100:raise RuntimeError('relay invalid '+repr((errors,stats)))
 rows=[]
 for line in (root/'rx.log').read_text(errors='replace').splitlines():
  if '[LAB][SSTV] RX diagnostics ' in line:
   rows.append(json.loads(line.split('RX diagnostics ',1)[1]))
 if not rows or any(row.get('sourceDevice')!='BlackHole 2ch' for row in rows):raise RuntimeError('missing or incorrect RX diagnostics')
 for name in ('discontinuities','droppedChunks','processingFailures','hamdrmRejectedChunks','hamdrmQueueOverflows'):
  if any(row.get(name)!=0 for row in rows):raise RuntimeError('RX diagnostics indicate '+name)
 if not any(row.get('hamdrmDeliveredGroups',0)>0 for row in rows):raise RuntimeError('no delivered HAMDRM groups recorded')
 print('PASS',sys.argv[1], 'bytes', (root/'original.jp2').stat().st_size, 'stats',stats,'artifacts',root,flush=True)
finally:
 stop.set();rec.terminate();rec.wait(timeout=5)
 for t in threads:t.join(timeout=5)
 for proc in (tx,rx):
  if proc and proc.poll() is None:
   proc.terminate()
   try:proc.wait(timeout=5)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
 if started:pa.Pa_StopStream(out)
 pa.Pa_CloseStream(out);pa.Pa_Terminate()
 print('artifacts',root,'stats',stats,'errors',errors,flush=True)
