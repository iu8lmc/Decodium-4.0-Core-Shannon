#!/usr/bin/env python3
"""Run original and extracted DX Cluster commands against a local TCP peer."""
import json, os, secrets, shutil, socket, subprocess, tempfile, threading
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; module=r/'src/bridge/DecodiumBridgeDxClusterCommands.cpp'; qml=r/'build/qml/decodium/Main.qml'
m=main.read_text(); n=module.read_text(); q=qml.read_text()
fixture=(r/'tests/bridge_refactor_fixtures/dxcluster_commands_before.inc').read_text()
mark='void DecodiumBridge::connectDxCluster()'; head=n.split(mark,1)[0]
assert (mark+n.split(mark,1)[1]).replace('decodiumBridgeDxClusterLog(', 'bridgeLog(')==fixture
rigs=[]; results=[]
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_CONSTRUCTOR_INSERTION_POINT'; assert m.count(point)==1
   main.write_text(m.replace(point,fixture+point)); module.write_text(head)
  else: main.write_text(m); module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  server=socket.socket(); server.bind(('127.0.0.1',0)); server.listen(1); server.settimeout(25); port=server.getsockname()[1]; received=[]
  def peer():
   try:
    conn,_=server.accept(); received.append('connected');conn.settimeout(8)
    try:
     while conn.recv(4096): pass
    except (socket.timeout,ConnectionResetError): pass
    conn.close()
   except OSError: pass
  thread=threading.Thread(target=peer,daemon=True);thread.start()
  probe='''\n Timer { interval: 10000; running: true; onTriggered: { bridge.callsign="9H1SR"; bridge.autoSpotEnabled=true; bridge.connectDxCluster("  127.0.0.1  ", PORT); } }\n Timer { interval: 12000; running: true; onTriggered: { console.log("[DX-PROBE] "+JSON.stringify({connected:bridge.dxClusterConnected,host:bridge.dxClusterHost,port:bridge.dxClusterPort,autospot:bridge.autoSpotEnabled})); bridge.disconnectDxCluster(); bridge.autoSpotEnabled=false; } }\n Timer { interval: 13000; running: true; onTriggered: { console.log("[DX-END] "+JSON.stringify({connected:bridge.dxClusterConnected,autospot:bridge.autoSpotEnabled})); Qt.quit(); } }\n'''.replace('PORT',str(port))
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='DxCommands'+secrets.token_hex(5);rigs.append(rig)
  try:
   with tempfile.TemporaryDirectory(prefix='decodium-dxcluster-runtime-') as tmp:
    env=os.environ.copy(); env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
    sockets=[]
    for udp in (2237,2239):
     sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',udp));sockets.append(sock)
    policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:*"))'
    cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','22000']
    proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=35)
    def row(tag):
     rows=[line.split(tag,1)[1] for line in proc.stdout.splitlines() if tag in line]
     if len(rows)!=1: raise RuntimeError(proc.stdout[-9000:])
     return json.loads(rows[0])
    a=row('[DX-PROBE] '); b=row('[DX-END] ')
    assert proc.returncode==0 and a=={'connected':True,'host':'127.0.0.1','port':port,'autospot':True} and b=={'connected':False,'autospot':False} and received==['connected'],(a,b,received)
    a['port']='local'; result={'connected':a,'afterDisconnect':b,'tcpAccepted':True};results.append(result);print(label,json.dumps(result,sort_keys=True),flush=True)
  finally: server.close();thread.join(timeout=2)
 assert results[0]==results[1]
finally:
 main.write_text(m); module.write_text(n); qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir(): shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
