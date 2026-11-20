#!/usr/bin/env python3
"""Compare manual NTP requests in isolated app instances and local-only network."""
import os,secrets,shutil,socket,subprocess,tempfile
import json
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeOffline.cpp';qml=r/'build/qml/decodium/Main.qml'
m=main.read_text();n=module.read_text();q=qml.read_text();start='bool DecodiumBridge::offlineMode() const'
old=subprocess.run(['git','show','v1.0.737:src/bridge/DecodiumBridge.cpp'],cwd=r,check=True,text=True,stdout=subprocess.PIPE).stdout;old=old[old.index(start):old.index('QObject * DecodiumBridge::callsignIntelligence() const')]
assert n[n.index(start):]==old
probe="""
 Timer { interval: 11000; running: true; repeat: false; onTriggered: {
  var svc=bridge.mapIntelligenceService; svc.setNetworkOfflineMode(true); var before=bridge.offlineMode;
  bridge.resumeOnlineServices(); var after=bridge.offlineMode;
  console.log('[OFFLINE-AB] ' + JSON.stringify({before:before,after:after})); Qt.quit();
 } }
"""

rigs=[];outputs=[]
try:
 for label in ('before','after'):
  if label=='before':
   anchor='// BRIDGE_CONSTRUCTOR_INSERTION_POINT';assert m.count(anchor)==1;main.write_text(m.replace(anchor,old+anchor));module.write_text(n[:n.index(start)])
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='OfflineActions'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-ntp-manual-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:*"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[OFFLINE-AB] ',1)[1] for line in result.stdout.splitlines() if '[OFFLINE-AB] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   import json
   value=json.loads(rows[0]);assert value=={'before':True,'after':False},value
   outputs.append(value);print(label,rows[0],flush=True)
 assert outputs[0]==outputs[1]
finally:
 main.write_text(m);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
