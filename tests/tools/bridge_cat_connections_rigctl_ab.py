#!/usr/bin/env python3
"""Compare CAT connection signal routing in isolated apps."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeConstructor.cpp'; mod=r/'src/bridge/DecodiumBridgeCatConnections.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,n,s,q=(p.read_text() for p in (main,mod,sstv,qml)); fixture=(r/'tests/bridge_refactor_fixtures/cat_connections_before.inc').read_text();assert n.split('void DecodiumBridge::initialiseCatConnections()\n{\n',1)[1]==fixture+'}\n'
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);m_catBackend=QStringLiteral("hamlib");m_monitoring=false;m_transmitting=false;m_tuning=false;m_startupModeAutoPending=false;m_hamlibCat->setRigName(QStringLiteral("Hamlib NET rigctl"));m_hamlibCat->setNetworkPort(QStringLiteral("127.0.0.1:%1").arg(qEnvironmentVariableIntValue("LAB_DUMMY_RIGCTLD_PORT")));m_hamlibCat->setPttMethod(QStringLiteral("CAT"));QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);auto c=connect(m_hamlibCat,&DecodiumTransceiverManager::connectedChanged,&loop,[&](){if(m_hamlibCat->connected())loop.quit();});qunsetenv("DECODIUM_DISABLE_CAT");m_hamlibCat->connectRig();qputenv("DECODIUM_DISABLE_CAT", "1");timeout.start(8000);if(!m_hamlibCat->connected())loop.exec();bool connected=m_hamlibCat->connected()&&m_catConnected;disconnect(c);QEventLoop settle;QTimer::singleShot(1200,&settle,&QEventLoop::quit);settle.exec();bool frequency=m_frequency==145000000.0&&m_hamlibCat->frequency()==145000000.0;bool noPtt=!m_transmitting&&!m_hamlibCat->pttActive();m_hamlibCat->disconnectRig();QEventLoop closing;QTimer::singleShot(700,&closing,&QEventLoop::quit);closing.exec();bool disconnected=!m_hamlibCat->connected()&&!m_catConnected;bool ok=connected&&frequency&&noPtt&&disconnected;return {{"wasShutdown",!ok},{"shutdown",ok},{"connected",connected},{"frequency",frequency},{"noPtt",noPtt},{"disconnected",disconnected},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''

probe='''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
reservation=socket.socket(socket.AF_INET,socket.SOCK_STREAM);reservation.bind(('127.0.0.1',0));rigPort=reservation.getsockname()[1];reservation.close();dummy=None
try:
 dummyPolicy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:*"))'
 dummy=subprocess.Popen(['sandbox-exec','-p',dummyPolicy,'/opt/homebrew/bin/rigctld','-m','1','-T','127.0.0.1','-t',str(rigPort)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 import time
 for _ in range(50):
  try:
   probeSocket=socket.create_connection(('127.0.0.1',rigPort),timeout=.2);probeSocket.close();break
  except OSError:time.sleep(.1)
 else:raise RuntimeError('isolated dummy rigctld failed to start')
 sstv.write_text(s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QBuffer>\n#include <QEventLoop>\n#include "DecodiumTransceiverManager.h"\n#include "DecodiumCat4OmManager.h"\n#include "DecodiumOmniRigManager.h"',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='    initialiseCatConnections();\n'; assert m.count(point)==1
   dependencies=n.split('void DecodiumBridge::initialiseCatConnections()',1)[0].replace('#define bridgeLog decodiumBridgeDecodeFlowLog\n','')
   main.write_text(m.replace('#include "DecodiumBridge.h"',dependencies,1).replace(point,fixture,1));mod.write_text(n)
  else:main.write_text(m);mod.write_text(n)
  build=subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
  if build.returncode:raise RuntimeError(build.stdout[-12000:])
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='CatRigctlDummy'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   env=os.environ.copy();env.update(LAB_DUMMY_RIGCTLD_PORT=str(rigPort),DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   sockets=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sockets.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:*"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[DISPLAY-FREQ-LAB] ',1)[1] for line in result.stdout.splitlines() if '[DISPLAY-FREQ-LAB] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-8000:])
   val=json.loads(rows[0]);
   if not val['shutdown']:print(result.stdout[-14000:],flush=True)
   assert not val['wasShutdown'] and val['shutdown'] and not val['cat'] and not val['tx'],val
   readProbe="\n Timer {interval:9000;running:true;repeat:false;onTriggered:{console.log('[SHUTDOWN-READ] '+String(bridge.getSetting('ShutdownSynthetic','missing')));Qt.quit();}}\n"
   qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+readProbe,1))
   reopened=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   persisted=[line.split('[SHUTDOWN-READ] ',1)[1] for line in reopened.stdout.splitlines() if '[SHUTDOWN-READ] ' in line]
   if reopened.returncode or persisted!=['persisted']:raise RuntimeError('persistence readback failed: '+reopened.stdout[-8000:])
   val['readback']=persisted[0];results.append(val);print(label,json.dumps(val,sort_keys=True),flush=True)
 assert results[0]==results[1]
finally:
 if dummy is not None:
  dummy.terminate();dummy.wait(timeout=5)
 for p,content in ((main,m),(mod,n),(sstv,s),(qml,q)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
