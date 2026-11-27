#!/usr/bin/env python3
"""Compare CAT connection signal routing in isolated apps."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeConstructor.cpp'; mod=r/'src/bridge/DecodiumBridgeDecodeClusterConnections.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,n,s,q=(p.read_text() for p in (main,mod,sstv,qml)); fixture=(r/'tests/bridge_refactor_fixtures/decode_cluster_connections_before.inc').read_text();assert n.split('void DecodiumBridge::initialiseDecodeClusterConnections()\n{\n',1)[1]==fixture+'}\n'
labFixture=fixture.replace('usingLegacyBackendForTx()', 'property("labLegacy").toBool()')
for name in ("m_decodeList", "m_rxDecodeList"):
 labFixture=labFixture.replace('publishRemoteActivityEntries('+name+');', 'setProperty("labPublish", property("labPublish").toInt()+1); publishRemoteActivityEntries('+name+');')
 labFixture=labFixture.replace('replayPskReporterRecentEntries('+name+',', 'setProperty("labPsk", property("labPsk").toInt()+1); replayPskReporterRecentEntries('+name+',')
labModule=n.replace(fixture,labFixture,1)
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);m_txEnabled=false;m_transmitting=false;m_monitoring=false;setProperty("labLegacy",false);setProperty("labPublish",0);setProperty("labPsk",0);int connected=0,spots=0;auto c1=connect(this,&DecodiumBridge::dxClusterConnectedChanged,this,[&](){++connected;});auto c2=connect(this,&DecodiumBridge::dxClusterSpotsChanged,this,[&](){++spots;});emit decodeListChanged();emit rxDecodeListChanged();bool feed=property("labPublish").toInt()==2&&property("labPsk").toInt()==2;setProperty("labLegacy",true);emit decodeListChanged();emit rxDecodeListChanged();bool legacy=property("labPublish").toInt()==2&&property("labPsk").toInt()==2;emit m_dxCluster->connectedChanged();bool connection=connected==1;emit m_dxCluster->spotsChanged();QEventLoop wait;QTimer::singleShot(1200,&wait,&QEventLoop::quit);wait.exec();bool spot=spots>=1;disconnect(c1);disconnect(c2);bool ok=feed&&legacy&&connection&&spot&&!m_catConnected&&!m_transmitting;return {{"wasShutdown",!ok},{"shutdown",ok},{"feed",feed},{"legacy",legacy},{"connection",connection},{"spot",spot},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 

probe='''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 sstv.write_text(s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QBuffer>\n#include <QEventLoop>\n#include <QTimer>\n#include "DecodiumDxCluster.h"',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='    initialiseDecodeClusterConnections();\n'; assert m.count(point)==1; main.write_text(m.replace(point,labFixture,1));mod.write_text(labModule)
  else:main.write_text(m);mod.write_text(labModule)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='DecodeCluster'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   sockets=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sockets.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[DISPLAY-FREQ-LAB] ',1)[1] for line in result.stdout.splitlines() if '[DISPLAY-FREQ-LAB] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-8000:])
   val=json.loads(rows[0]);assert not val['wasShutdown'] and val['shutdown'] and not val['cat'] and not val['tx'],val
   readProbe="\n Timer {interval:9000;running:true;repeat:false;onTriggered:{console.log('[SHUTDOWN-READ] '+String(bridge.getSetting('ShutdownSynthetic','missing')));Qt.quit();}}\n"
   qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+readProbe,1))
   reopened=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   persisted=[line.split('[SHUTDOWN-READ] ',1)[1] for line in reopened.stdout.splitlines() if '[SHUTDOWN-READ] ' in line]
   if reopened.returncode or persisted!=['persisted']:raise RuntimeError('persistence readback failed: '+reopened.stdout[-8000:])
   val['readback']=persisted[0];results.append(val);print(label,json.dumps(val,sort_keys=True),flush=True)
 assert results[0]==results[1]
finally:
 for p,content in ((main,m),(mod,n),(sstv,s),(qml,q)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
