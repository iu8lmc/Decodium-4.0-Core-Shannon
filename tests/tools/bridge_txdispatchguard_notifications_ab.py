#!/usr/bin/env python3
"""Exercise TX dispatch guards in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeTxDispatchGuard.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/txdispatchguard_before.inc').read_text();mark='void DecodiumBridge::armExplicitManualTxRequest(';head=n.split(mark,1)[0];tail='#undef bridgeLog\n';assert n==head+fixture+tail;head+=tail
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''\n#if DECODIUM_HAS_SSTV
    {Q_UNUSED(values);m_currentTx=6;m_txEnabled=true;m_autoCqRepeat=false;m_multiAnswerMode=false;m_manualTxHold=false;m_startupTxGuard=false;m_explicitManualTxRequest=0;int notifications=0;auto c=connect(this,&DecodiumBridge::txEnabledChanged,this,[&](){++notifications;});bool first=!txDispatchGuardAllows(QStringLiteral("lab"))&&!m_txEnabled&&notifications==1;bool second=!txDispatchGuardAllows(QStringLiteral("lab"))&&notifications==1;armExplicitManualTxRequest(6,QStringLiteral("lab"));m_manualTxHold=true;m_startupTxGuard=true;bool explicitAllowed=txDispatchGuardAllows(QStringLiteral("lab"))&&m_explicitManualTxRequest==6;m_currentTx=1;bool staleBlocked=!txDispatchGuardAllows(QStringLiteral("lab"))&&m_explicitManualTxRequest==0&&notifications==1;disconnect(c);return {{"first",first},{"second",second},{"explicitAllowed",explicitAllowed},{"staleBlocked",staleBlocked},{"notifications",notifications},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif\n'''

probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[TX-GUARD] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='TxGuardNotify'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-txguard-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[TX-GUARD] ',1)[1] for line in result.stdout.splitlines() if '[TX-GUARD] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert all(value[k] for k in ('first','second','explicitAllowed','staleBlocked')) and value['notifications']==1 and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
