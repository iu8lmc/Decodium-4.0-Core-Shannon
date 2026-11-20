#!/usr/bin/env python3
"""Compare audio device recovery in isolated app sessions."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeAudioRecovery.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/audiodevicerecovery_before.inc').read_text();start='#define bridgeLog decodiumBridgeAudioLevelLog\n';end='#undef bridgeLog\n';assert fixture in n and start+fixture+end in n;head=n.replace(start+fixture+end,'')
probe="""\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.refreshAudioDevices();let ok=bridge.restoreAudioDefaults();console.log('[AUDIO-RECOVERY] '+JSON.stringify({ok:ok,input:bridge.audioInputDevice,output:bridge.audioOutputDevice,tx:bridge.transmitting,monitor:bridge.monitoring}));Qt.quit();}}\n"""
rigs=[];results=[]
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_SHUTDOWN_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='AudioRecovery'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-audiorecovery-runtime-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45);rows=[line.split('[AUDIO-RECOVERY] ',1)[1] for line in proc.stdout.splitlines() if '[AUDIO-RECOVERY] ' in line]
   if proc.returncode or len(rows)!=1:raise RuntimeError(proc.stdout[-6000:])
   value=json.loads(rows[0]);assert not value['tx'] and not value['monitor'] and isinstance(value['ok'],bool),value;results.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert results[0]==results[1]
finally:
 main.write_text(m);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
