#!/usr/bin/env python3
"""Compare remote monitor missing device behavior in isolated app sessions."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeDecoPortMonitor.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/decoportmonitor_before.inc').read_text();mark='void DecodiumBridge::setDecoPortMonitor(bool on)';head=n.split(mark,1)[0];tail='#undef bridgeLog\n';assert n==head+fixture+tail;head+=tail
probe='''
 property int monitorErrors: 0
 Connections {target:bridge; function onErrorMessage(message){if(message.indexOf("Remote RX audio output")>=0)monitorErrors++;}}
 Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.remoteRxDevice="";bridge.decoPortMonitor=true;let defaultStarted=bridge.decoPortMonitor;bridge.decoPortMonitor=false;monitorErrors=0;bridge.remoteRxDevice="c3ludGhldGljLW1pc3NpbmctZGV2aWNl";bridge.decoPortMonitor=false;bridge.decoPortMonitor=true;let refused=!bridge.decoPortMonitor;bridge.decoPortMonitor=false;console.log("[REMOTE-RX] "+JSON.stringify({defaultStarted:defaultStarted,refused:refused,off:!bridge.decoPortMonitor,errors:monitorErrors,tx:bridge.transmitting}));Qt.quit();}}
''' 
rigs=[];results=[]
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_SPOT_SHARE_INSERTION_POINT';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='DecoPortMonitor'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-audiofreq-runtime-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45);rows=[line.split('[REMOTE-RX] ',1)[1] for line in proc.stdout.splitlines() if '[REMOTE-RX] ' in line]
   if proc.returncode or len(rows)!=1:raise RuntimeError(proc.stdout[-6000:])
   value=json.loads(rows[0]);assert value=={'defaultStarted':True,'refused':True,'off':True,'errors':1,'tx':False},value;results.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert results[0]==results[1]
finally:
 main.write_text(m);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
