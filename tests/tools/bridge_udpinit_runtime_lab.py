#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeUdpInit.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/udpinit_before.inc').read_text();mark='void DecodiumBridge::initUdpMessageClient(';head=n.split(mark,1)[0];tail='#undef decodeMirrorEntryKey\n#undef bridgeLog\n';assert n==head+fixture+tail;head+=tail
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);shutdownUdpMessageClient();setSetting(QStringLiteral("UDPListenPort"),22455);setSetting(QStringLiteral("UDPClientId"),QStringLiteral("Decodium"));setSetting(QStringLiteral("AcceptUDPRequests"),true);setSetting(QStringLiteral("UDPSecondaryEnabled"),true);setSetting(QStringLiteral("UDPTertiaryEnabled"),true);setSetting(QStringLiteral("UDPSecondarySendStatus"),true);setSetting(QStringLiteral("UDPTertiarySendStatus"),true);setSetting(QStringLiteral("UDPSecondaryServer"),QStringLiteral("127.0.0.1"));setSetting(QStringLiteral("UDPTertiaryServer"),QStringLiteral("127.0.0.1"));QEventLoop startup;QTimer::singleShot(400,&startup,&QEventLoop::quit);startup.exec();shutdownUdpMessageClient();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);initUdpMessageClient(true);bool primary=m_udpMessageClient!=nullptr;bool secondary=m_udpSecondaryMessageClient!=nullptr;bool tertiary=m_udpTertiaryMessageClient!=nullptr;auto* first=m_udpMessageClient;initUdpMessageClient(true);bool reused=first==m_udpMessageClient;m_decodeList={QVariantMap{{"message","test band"}}};m_rxDecodeList={QVariantMap{{"message","test rx"}}};m_udpMessageClient->clear_decodes(0);bool clearBand=m_decodeList.isEmpty() && !m_rxDecodeList.isEmpty();m_udpMessageClient->clear_decodes(1);bool clearRx=m_rxDecodeList.isEmpty();m_txEnabled=true;m_udpMessageClient->halt_tx(true);bool autoOnly=m_txEnabled;m_udpMessageClient->halt_tx(false);bool halted=!m_txEnabled;setSetting(QStringLiteral("UDPSecondaryEnabled"),false);setSetting(QStringLiteral("UDPTertiaryEnabled"),false);initUdpMessageClient(true);bool removed=!m_udpSecondaryMessageClient && !m_udpTertiaryMessageClient;shutdownUdpMessageClient();bool stopped=!m_udpMessageClient;return {{"primary",primary},{"secondary",secondary},{"tertiary",tertiary},{"reused",reused},{"clearBand",clearBand},{"clearRx",clearRx},{"autoOnly",autoOnly},{"halted",halted},{"removed",removed},{"stopped",stopped},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[UDP-INIT] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text('#include "Network/MessageClient.hpp"\n#include <QEventLoop>\n'+s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='UdpInit'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-udpinit-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))(allow network-outbound (remote ip "localhost:22455"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[UDP-INIT] ',1)[1] for line in result.stdout.splitlines() if '[UDP-INIT] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value['primary'] and value['secondary'] and value['tertiary'] and value['reused'] and value['clearBand'] and value['clearRx'] and value['autoOnly'] and value['halted'] and value['removed'] and value['stopped'] and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
