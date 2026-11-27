#!/usr/bin/env python3
"""Exercise recorded synthetic audio through BlackHole without CAT."""
import argparse,json,os,secrets,shutil,subprocess,tempfile,wave,time,socket
import numpy as np
import sounddevice as sd
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='Rec'+secrets.token_hex(5);directory=Path.home()/'.qttest/Documents/Decodium/recordings'; before=set(directory.glob('decodium_*.wav')) if directory.exists() else set();created=set(); owned=None
probe='''
 Timer { interval: 12000; running: true; repeat: false; onTriggered: {bridge.recordRxEnabled=true;stopRec.start();} }
 Timer { id: stopRec; interval: 7000; repeat: false; onTriggered: {var started=bridge.recordRxEnabled;var path=bridge.wavManager.lastFilePath;var rxStarted=bridge.monitoring;bridge.recordRxEnabled=false;bridge.monitoring=false;console.log("[RXREC-PROBE] "+JSON.stringify({started:started,stopped:!bridge.recordRxEnabled,path:path,rxStarted:rxStarted,rxStopped:!bridge.monitoring,tx:bridge.transmitting,cat:bridge.catConnected}));} }
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-rxrecording-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 16ch','--lab-monitor-ms','1500','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','30000']
  output=Path(tmp)/'application.log'
  device=next(i for i,d in enumerate(sd.query_devices()) if d['name']=='BlackHole 2ch' and d['max_output_channels']>=2)
  with output.open('w') as stream:
   socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   process=subprocess.Popen(['sandbox-exec','-p',policy]+cmd,env=env,stdout=stream,stderr=subprocess.STDOUT,text=True)
   try:
    time.sleep(8);t=np.arange(18*48000)/48000;audio=(0.20*np.sin(2*np.pi*1000*t)).astype(np.float32);sd.play(np.column_stack((audio,audio)),samplerate=48000,device=device,blocking=True);code=process.wait(timeout=50)
   finally:
    sd.stop()
    if process.poll() is None:process.terminate();process.wait(timeout=10)
  class Result:pass
  result=Result();result.returncode=code;result.stdout=output.read_text(errors='replace')
  created=(set(directory.glob('decodium_*.wav')) if directory.exists() else set())-before
  lines=[x.split('[RXREC-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[RXREC-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:raise RuntimeError('probe failed '+result.stdout[-4000:])
  v=json.loads(lines[0]);path=Path(v['path']);owned=path if path in created else None;
  if path not in created or len(created)!=1 or not v['started'] or not v['stopped'] or v['tx'] or v['cat'] or not v['rxStarted'] or not v['rxStopped']:raise RuntimeError('unexpected '+str(v)+' '+str(created))
  with wave.open(str(path),'rb') as wav:
   if (wav.getnchannels(),wav.getsampwidth(),wav.getframerate())!=(1,2,12000):raise RuntimeError('invalid WAV format')
   samples=np.frombuffer(wav.readframes(wav.getnframes()),dtype='<i2').astype(np.float64)
   if len(samples)<12000 or np.sqrt(np.mean(samples*samples))<100:raise RuntimeError('no significant synthetic audio in WAV')
  print(json.dumps({'started':True,'stopped':True,'wav_pcm_mono_12khz_with_audio':True,'rxStarted':True,'rxStopped':True,'tx':False},sort_keys=True))
finally:
 qml.write_text(original)
 if owned is not None and owned.parent==directory and owned.name.startswith('decodium_') and owned.suffix=='.wav':owned.unlink(missing_ok=True)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
