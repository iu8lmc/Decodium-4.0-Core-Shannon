#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeLegacyPttTransition.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/legacyptttransition_before.inc').read_text();mark='void DecodiumBridge::setTxRequestedState(';head=n.split(mark,1)[0];tail='#undef catSignalMatchesBackend\n#undef activeCatCanPtt\n#undef activeCatSetPtt\n#undef txTimelineLog\n#undef bridgeLog\n';assert n==head+fixture+tail;head+=tail
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);int actions=0;auto wait=[&](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};beginLegacyPttTransition(QStringLiteral("synthetic-dispatch"),decodium::tx::PttConfirmationMode::CommandDispatch,[&](){++actions;return true;});bool pending=m_txRequested && m_pttPending;wait(30);bool confirmed=m_txRequested && m_pttConfirmed && !m_pttPending && actions==1;clearLegacyPttTransition(QStringLiteral("test"));bool cleared=!m_txRequested && !m_pttPending && !m_pttConfirmed;beginLegacyPttTransition(QStringLiteral("synthetic-feedback"),decodium::tx::PttConfirmationMode::RigFeedback,[&](){++actions;return true;});handleActivePttFeedback(true,QStringLiteral("invalid"));bool mismatch=m_pttPending;handleActivePttFeedback(true,m_catBackend);wait(30);bool feedback=m_pttConfirmed && actions==2;clearLegacyPttTransition(QStringLiteral("test"));beginLegacyPttTransition(QStringLiteral("synthetic-timeout"),decodium::tx::PttConfirmationMode::RigFeedback,[&](){++actions;return true;});wait(decodium::tx::pttFeedbackTimeoutMs()+200);bool timedOut=!m_txRequested && !m_pttPending && !m_pttConfirmed && actions==2;beginLegacyPttTransition(QStringLiteral("synthetic-cancel"),decodium::tx::PttConfirmationMode::CommandDispatch,[&](){++actions;return true;});clearLegacyPttTransition(QStringLiteral("cancel"));wait(30);bool cancelled=actions==2 && !m_txRequested && !m_pttPending && !m_pttConfirmed;beginLegacyPttTransition(QStringLiteral("synthetic-audio-failure"),decodium::tx::PttConfirmationMode::CommandDispatch,[&](){++actions;return false;});wait(30);bool audioFailure=actions==3 && !m_txRequested && !m_pttPending && !m_pttConfirmed;requestLegacyPttOffOnce(QStringLiteral("test"));return {{"cancelled",cancelled},{"audioFailure",audioFailure},{"pending",pending},{"confirmed",confirmed},{"cleared",cleared},{"mismatch",mismatch},{"feedback",feedback},{"timedOut",timedOut},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[LEGACY-PTT-TRANSITION] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text('#include <QEventLoop>\n#include "PttTransitionPolicy.h"\n'+s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m+'\n'+tail+n.split(mark,1)[0]+fixture+tail);module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='LegacyPttTransition'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-legacyptt-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[LEGACY-PTT-TRANSITION] ',1)[1] for line in result.stdout.splitlines() if '[LEGACY-PTT-TRANSITION] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value['cancelled'] and value['audioFailure'] and value['pending'] and value['confirmed'] and value['cleared'] and value['mismatch'] and value['feedback'] and value['timedOut'] and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
