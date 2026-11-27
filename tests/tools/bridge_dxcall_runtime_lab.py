#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeDxCall.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/dxcall_before.inc').read_text();mark='QString DecodiumBridge::dxCall() const';head=n.split(mark,1)[0];tail='#undef bridgeDiagnosticOneLine\n';assert n==head+fixture+tail;head+=tail
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);m_dxCall=QStringLiteral("K1ABC");m_qsoPartnerFreqHz=1300;m_currentPartnerSnrDb=-10;m_pendingAutoSeqTxAfterActiveTx=1;m_pendingAutoSeqPartnerBase=QStringLiteral("K1ABC");m_ft2AutoCqAwaitingPartnerTx=1;m_nextLogName=QStringLiteral("old name");m_nextLogQth=QStringLiteral("old qth");m_lastTransmittedMessage=QStringLiteral("K1ABC 73");m_decodeList={QVariantMap{{"fromCall","W1AW"},{"matchesDxCall",false}},QVariantMap{{"fromCall","K1ABC"},{"matchesDxCall",true}}};int notificationCount=0;auto conn=QObject::connect(this,&DecodiumBridge::dxCallChanged,this,[&](){++notificationCount;});setDxCall(QStringLiteral(" W1AW "));bool changed=m_dxCall==QStringLiteral("W1AW") && m_qsoPartnerFreqHz==0 && m_currentPartnerSnrDb==127 && m_pendingAutoSeqTxAfterActiveTx==0 && m_ft2AutoCqAwaitingPartnerTx==0 && m_nextLogName.isEmpty() && m_nextLogQth.isEmpty() && m_lastTransmittedMessage.isEmpty();bool matches=m_decodeList.at(0).toMap().value("matchesDxCall").toBool() && !m_decodeList.at(1).toMap().value("matchesDxCall").toBool();m_qsoPartnerFreqHz=777;setDxCall(QStringLiteral("W1AW"));bool unchanged=m_qsoPartnerFreqHz==777 && notificationCount==1;setDxCall(QStringLiteral("<...>"));bool placeholder=m_dxCall.isEmpty() && notificationCount==2;QObject::disconnect(conn);return {{"changed",changed},{"matches",matches},{"unchanged",unchanged},{"placeholder",placeholder},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[DXCALL] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='DxCall'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-dxcall-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[DXCALL] ',1)[1] for line in result.stdout.splitlines() if '[DXCALL] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value['changed'] and value['matches'] and value['unchanged'] and value['placeholder'] and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
