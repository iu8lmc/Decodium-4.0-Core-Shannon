#!/usr/bin/env python3
"""Compare manual NTP requests in isolated app instances and local-only network."""
import os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeTimeSync.cpp';qml=r/'build/qml/decodium/Main.qml'
m=main.read_text();n=module.read_text();q=qml.read_text();old=(r/'tests/bridge_refactor_fixtures/ntp_manual_before.inc').read_text();mark='void DecodiumBridge::syncNtpNow()';end='\n#define bridgeLog decodiumBridgeNtpLog\nvoid DecodiumBridge::initialiseTimeSync()';assert n[n.index(mark):n.index(end)].replace('decodiumBridgeNtpLog(', 'bridgeLog(').rstrip()==old.rstrip()
probe='''
 property var ntpManualMessages: []
 Connections { target: bridge; function onStatusMessage(message) { if (message.indexOf("NTP ") === 0) ntpManualMessages.push(message); } }
 Timer { interval: 11000; running: true; repeat: false; onTriggered: {
  bridge.setSetting("DecoSyncTimeEnabled", false); bridge.setSetting("NTPEnabled", false);
  bridge.syncNtpNow();
  bridge.setSetting("NTPCustomServer", "127.0.0.1");
  bridge.setSetting("DecoSyncTimeEnabled", true); bridge.setSetting("NTPEnabled", true);
  bridge.syncNtpNow();
  console.log("[NTP-MANUAL] " + JSON.stringify(ntpManualMessages)); Qt.quit();
 } }
'''
rigs=[];outputs=[]
try:
 for label in ('before','after'):
  if label=='before':
   anchor='// BRIDGE_CONSTRUCTOR_INSERTION_POINT';assert m.count(anchor)==1;main.write_text(m.replace(anchor,old+anchor));module.write_text(n[:n.index(mark)]+n[n.index(end):])
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='NtpManual'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-ntp-manual-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:*"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[NTP-MANUAL] ',1)[1] for line in result.stdout.splitlines() if '[NTP-MANUAL] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   import json
   value=json.loads(rows[0]);assert any('disabilitato: abilitalo' in x for x in value) and any('sync in corso su 127.0.0.1' in x for x in value),value
   outputs.append(value);print(label,rows[0],flush=True)
 assert outputs[0]==outputs[1]
finally:
 main.write_text(m);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
