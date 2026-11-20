#!/usr/bin/env python3
"""Exercise RX recording to an isolated Qt test Documents directory."""
import argparse,json,os,secrets,shutil,subprocess,tempfile,wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='Rec'+secrets.token_hex(5);directory=Path.home()/'.qttest/Documents/Decodium/recordings'; before=set(directory.glob('decodium_*.wav')) if directory.exists() else set();created=set(); owned=None
probe='''
 Timer { interval: 12000; running: true; repeat: false; onTriggered: {bridge.recordRxEnabled=true;stopRec.start();} }
 Timer { id: stopRec; interval: 1000; repeat: false; onTriggered: {var started=bridge.recordRxEnabled;var path=bridge.wavManager.lastFilePath;bridge.recordRxEnabled=false;console.log("[RXREC-PROBE] "+JSON.stringify({started:started,stopped:!bridge.recordRxEnabled,path:path,tx:bridge.transmitting}));} }
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-rxrecording-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','30000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=70)
  created=(set(directory.glob('decodium_*.wav')) if directory.exists() else set())-before
  lines=[x.split('[RXREC-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[RXREC-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:raise RuntimeError('probe failed '+result.stdout[-4000:])
  v=json.loads(lines[0]);path=Path(v['path']);owned=path if path in created else None;
  if path not in created or len(created)!=1 or not v['started'] or not v['stopped'] or v['tx']:raise RuntimeError('unexpected '+str(v)+' '+str(created))
  with wave.open(str(path),'rb') as wav:
   if (wav.getnchannels(),wav.getsampwidth(),wav.getframerate())!=(1,2,12000):raise RuntimeError('invalid WAV format')
  print(json.dumps({'started':True,'stopped':True,'wav_pcm_mono_12khz':True,'tx':False},sort_keys=True))
finally:
 qml.write_text(original)
 if owned is not None and owned.parent==directory and owned.name.startswith('decodium_') and owned.suffix=='.wav':owned.unlink(missing_ok=True)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
