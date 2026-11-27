#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeTxMessages.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/cleartxmessages_before.inc').read_text();mark='void DecodiumBridge::clearTxMessages()';head=n.split(mark,1)[0];tail='#undef bridgeLog\n';assert n==head+fixture+tail;head+=tail
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);m_dxCall=QStringLiteral("K1ABC");m_dxGrid=QStringLiteral("FN42");m_qsoProgress=4;m_reportSent=QStringLiteral("-03");m_reportReceived=QStringLiteral("-07");m_tx1=QStringLiteral("OLD1");m_tx2=QStringLiteral("OLD2");m_tx3=QStringLiteral("OLD3");m_tx4=QStringLiteral("OLD4");m_tx5=QStringLiteral("OLD5");m_tx6=QStringLiteral("CQ 9H1SR JM75");m_currentTx=4;m_txRetryCount=3;m_nTx73=2;m_activeTxMessage=QStringLiteral("OLD");m_pendingAutoSeqTxAfterActiveTx=2;clearTxMessages();bool reset=m_dxCall.isEmpty() && m_dxGrid.isEmpty() && m_qsoProgress==0 && m_reportSent==QStringLiteral("-10") && m_reportReceived==QStringLiteral("-10") && m_tx1.isEmpty() && m_tx2.isEmpty() && m_tx3.isEmpty() && m_tx4.isEmpty() && m_tx5.isEmpty() && m_txRetryCount==0 && m_nTx73==0 && m_activeTxMessage.isEmpty() && m_pendingAutoSeqTxAfterActiveTx==0 && !m_txEnabled;bool cq=m_currentTx==6;m_tx6.clear();clearTxMessages();bool emptyCq=m_currentTx==1;return {{"reset",reset},{"cq",cq},{"emptyCq",emptyCq},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[CLEAR-TX-MESSAGES] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='ClearTxMessages'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-cleartx-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[CLEAR-TX-MESSAGES] ',1)[1] for line in result.stdout.splitlines() if '[CLEAR-TX-MESSAGES] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value['reset'] and value['cq'] and value['emptyCq'] and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
