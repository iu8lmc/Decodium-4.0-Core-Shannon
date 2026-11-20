#!/usr/bin/env python3
"""Compare PSK Reporter and Cloudlog initialization in isolated apps."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeConstructor.cpp'; mod=r/'src/bridge/DecodiumBridgeOnlineLogbook.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,n,s,q=(p.read_text() for p in (main,mod,sstv,qml)); fixture=(r/'tests/bridge_refactor_fixtures/psk_cloudlog_init_before.inc').read_text();assert n.split('void DecodiumBridge::initialisePskAndCloudlog()\n{\n',1)[1].startswith(fixture+'}\n#undef bridgeLog\n')
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj=r'''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);QTcpServer server;bool listening=server.listen(QHostAddress::LocalHost,0);QByteArray requestBytes;QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);connect(&server,&QTcpServer::newConnection,&loop,[&](){QTcpSocket* peer=server.nextPendingConnection();connect(peer,&QTcpSocket::readyRead,&loop,[&,peer](){requestBytes+=peer->readAll();if(requestBytes.contains("TESTCALL")){peer->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");peer->flush();}});});QString result;auto c=connect(this,&DecodiumBridge::externalAdifUploadStatus,&loop,[&](quint32 id,const QString& state,const QString&){if(id==201){result=state;loop.quit();}});ExternalAdifUploadPending pending;pending.dxCall=QStringLiteral("TESTCALL");pending.target=QStringLiteral("Cloudlog");pending.pendingBackends<<QStringLiteral("Cloudlog");m_externalAdifUploads.insert(201,pending);m_cloudlog->setApiUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));m_cloudlog->setApiKey(QStringLiteral("synthetic"));m_cloudlog->setEnabled(true);m_cloudlog->uploadAdif(QStringLiteral("TESTCALL"),QByteArray("<CALL:8>TESTCALL <EOR>"),201);timeout.start(5000);loop.exec();bool request=requestBytes.contains("POST /index.php/api/qso")&&requestBytes.contains("TESTCALL")&&requestBytes.contains("synthetic");bool confirmed=result==QStringLiteral("Sent")&&!m_externalAdifUploads.contains(201);m_cloudlog->setEnabled(false);disconnect(c);bool ok=listening&&request&&confirmed;return {{"wasShutdown",!ok},{"shutdown",ok},{"listening",listening},{"request",request},{"confirmed",confirmed},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''

probe='''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 sstv.write_text(s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QBuffer>\n#include <QTcpServer>\n#include <QTcpSocket>\n#include <QHostAddress>\n#include <QEventLoop>\n#include "Network/DecodiumCloudlogLite.h"\n#include "Network/DecodiumPskReporterLite.h"',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='    initialisePskAndCloudlog();\n'; assert m.count(point)==1; prefix=n.split('\nvoid DecodiumBridge::',1)[0].split('\nnamespace {',1)[0]; prefix='\n'.join(line for line in prefix.splitlines() if not line.startswith('#define bridgeLog '))+'\n'; undef=''.join('\n#undef '+line.split()[1]+'\n' for line in prefix.splitlines() if line.startswith('#define ')); main.write_text(m.replace('#include "DecodiumBridge.h"',prefix,1).replace(point,fixture+undef,1));mod.write_text(n)
  else:main.write_text(m);mod.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='PskCloudLocal'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   sockets=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sockets.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:*"))(allow network-outbound (remote ip "localhost:2239"))'
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
