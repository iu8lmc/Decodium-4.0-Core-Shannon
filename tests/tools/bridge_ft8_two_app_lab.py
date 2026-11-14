#!/usr/bin/env python3
"""Opt-in macOS lab: two isolated Decodium sessions over BlackHole, CAT disabled.

Requires numpy, sounddevice, a local build/decodium with offscreen Qt, and
BlackHole 2ch/16ch. Run with argument aligned or immediate; optional --mode
FT4, FT2 or Q65-15A. Aligned FT4/Q65 use a temporary UTC-triggered QML probe,
restored on exit.
No RF proof.
"""
import os,sys,subprocess,time,json,sqlite3,secrets,tempfile,atexit,shutil,socket,argparse,hashlib
from measure_runtime_baseline import LAB_SANDBOX_POLICY as POLICY
from pathlib import Path
import numpy as np
import sounddevice as sd
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('role',nargs='?',default='aligned',choices=('aligned','immediate'))
parser.add_argument('--mode',default='FT8',choices=('FT8','FT4','FT2','Q65-15A'))
args=parser.parse_args();role=args.role;mode=args.mode
if subprocess.run(['pgrep','-x','decodium.*|Decodium4'],capture_output=True).returncode==0:
 raise SystemExit('Another Decodium instance is running')
legacy=Path.home()/'.qttest/Library/Preferences/decodium4.ini'
legacy_files=(legacy,legacy.with_name('ft2.ini'))
original_legacy={path:path.read_bytes() if path.exists() else None for path in legacy_files}
owned=[];sinks=[];qml=None;original_qml=None
def cleanup():
 sd.stop()
 if qml is not None and original_qml is not None:
  qml.write_bytes(original_qml)
  assert qml.read_bytes()==original_qml
 for path,data in original_legacy.items():
  if data is None:path.unlink(missing_ok=True)
  else:path.write_bytes(data)
  assert (path.read_bytes() if path.exists() else None)==data
 for directory in owned:
  if directory.is_dir():shutil.rmtree(directory)
 for sink in sinks:sink.close()
atexit.register(cleanup)
for port in (2237,2239):
 sink=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sinks.append(sink);sink.bind(('127.0.0.1',port))
legacy.parent.mkdir(parents=True,exist_ok=True)
for path in legacy_files:path.write_text('')
root=Path(tempfile.mkdtemp(prefix='decodium-bridge-'+mode.lower()+'-'+role+'-'));settings=root/'settings';settings.mkdir()
suffix=secrets.token_hex(4)
def env(which):
 e={k:v for k,v in os.environ.items() if not k.startswith(('DECODIUM_','QT_','QSG_','QML_','OMP_'))};p=settings/which;p.mkdir(exist_ok=True);e.update(QT_QPA_PLATFORM=os.environ.get('DECODIUM_LAB_QPA_PLATFORM','offscreen'),QT_QUICK_BACKEND='software',DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_SETTINGS_DIR=str(p),DECODIUM_DISABLE_QML_CACHE='1')
 if which=='tx' and role=='aligned':e['DECODIUM_LAB_FTX_SLOT_TIMING']='1'
 else:e.pop('DECODIUM_LAB_FTX_SLOT_TIMING',None)
 return e
rxname='BridgePair'+role+'RX'+suffix;txname='BridgePair'+role+'TX'+suffix
for name in (rxname,txname):
 for category in ('Application Support','Caches'):
  owned.append(Path.home()/'.qttest/Library'/category/'IU8LMC'/('Decodium - '+name+' - test'))
common=['sandbox-exec','-p',POLICY,os.environ.get('DECODIUM_LAB_BINARY','./build/decodium'),'--test-mode','--lab-no-cat','--lab-mode',mode,'--lab-grid','JM75']
rxargs=common+['--rig-name',rxname,'--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 16ch','--lab-callsign','IU8LMC','--lab-monitor-ms','2500','--lab-quit-ms','85000']
txargs=common+['--rig-name',txname,'--lab-audio-input','BlackHole 16ch','--lab-audio-output','BlackHole 2ch','--lab-callsign','9H1SR','--lab-tx-output-level','70','--lab-monitor-ms','2500','--lab-tx-period','0','--lab-sendtx-ms',os.environ.get('DECODIUM_LAB_SENDTX_MS','10000'),'--lab-sendtx-slot','6','--lab-quit-ms','65000']
if mode in ('FT4','Q65-15A') and role=='aligned':
 # An arbitrary elapsed startup timer can hit a late-slot rejection.
 # Request normal TX from the actual GUI at an even UTC slot boundary.
 index=txargs.index('--lab-sendtx-ms');del txargs[index:index+2]
 qml=Path(os.environ.get('DECODIUM_LAB_BINARY','./build/decodium')).resolve().parent/'qml/decodium/Main.qml'
 original_qml=qml.read_bytes();marker=b'GlassApplicationWindow {'
 assert original_qml.count(marker)==1
 probe='''
 Timer { interval:20;running:true;repeat:true;property double began:Date.now();onTriggered:{
   if(bridge.callsign!=="9H1SR" || bridge.mode!=="__MODE__" || Date.now()-began<12000)return;
   if(Date.now()%__PAIR_PERIOD__<100){stop();bridge.resetStandardTxMessages();bridge.sendTx(6);
     console.log("[PAIR-FTX] UTC-aligned TX requested "+Date.now());}
 }}
'''.replace('__MODE__',mode).replace('__PAIR_PERIOD__','15000' if mode=='FT4' else '30000').encode()
 qml.write_bytes(original_qml.replace(marker,marker+probe,1))
device=next(i for i,d in enumerate(sd.query_devices()) if d['name']=='BlackHole 2ch' and d['max_input_channels']>=2)
with (root/('pair-'+role+'-rx.log')).open('w') as rxlog,(root/('pair-'+role+'-tx.log')).open('w') as txlog:
 rx=subprocess.Popen(rxargs,env=env('rx'),stdout=rxlog,stderr=subprocess.STDOUT)
 try:
  time.sleep(7);started=time.time();audio=sd.rec(70*48000,samplerate=48000,channels=2,device=device,dtype='float32')
  tx=subprocess.Popen(txargs,env=env('tx'),stdout=txlog,stderr=subprocess.STDOUT)
  try:txcode=tx.wait(timeout=100);sd.wait();rxcode=rx.wait(timeout=40)
  finally:
   if tx.poll() is None:tx.terminate();tx.wait(timeout=10)
 finally:
  if rx.poll() is None:rx.terminate();rx.wait(timeout=10)
step=4800;w=np.sqrt(np.mean(np.square(audio[:len(audio)//step*step].reshape(-1,step,2)),axis=(1,2)));mask=w>0.003
path=Path.home()/'.qttest/Library/Application Support/IU8LMC'/('Decodium - '+rxname+' - test')/'db.sqlite'
messages=[]
if path.exists():
 with sqlite3.connect('file:'+str(path)+'?mode=ro',uri=True) as db:messages=[r[0] for r in db.execute("select message from decodes where message like '%9H1SR%' order by id")]
out={'role':role,'mode':mode,'txcode':txcode,'rxcode':rxcode,'peak':float(np.max(np.abs(audio))),'active_windows':int(mask.sum()),'first_active_mod15':float((started+np.flatnonzero(mask)[0]/10)%15) if mask.any() else None,'matching_messages':messages}
udp_failure=any('QNativeSocketEngine::writeDatagram() was called not in' in p.read_text() for p in root.glob('pair-*.log'))
out['udp_failure']=udp_failure
out['binary_sha256']=hashlib.sha256(Path(common[3]).read_bytes()).hexdigest()
out['mode_verified']=all(('[Bridge] RX avviato - '+mode) in p.read_text() for p in root.glob('pair-*.log'))
out['artifacts']=str(root)
(root/'result.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out),flush=True)
if udp_failure:raise SystemExit('UDP error invalidates the loopback')
if txcode or rxcode or not mask.any() or not messages or not out['mode_verified']:raise SystemExit('loopback did not complete successfully')
