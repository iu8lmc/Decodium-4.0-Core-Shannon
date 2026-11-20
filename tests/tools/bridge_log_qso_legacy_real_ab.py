#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeLogQso.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/log_qso_now_before.inc').read_text();mark='void DecodiumBridge::logQsoNow()';head=n.split(mark,1)[0];assert n==head+fixture
init=r/'src/bridge/DecodiumBridgeLegacyInit.cpp';initOriginal=init.read_text();macro='#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled';assert initOriginal.count(macro)==1
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
window=r/'widgets/mainwindow.cpp';winOrig=window.read_text();logdlg=r/'widgets/logqso.cpp';dlgOrig=logdlg.read_text();legacy=r/'src/bridge/DecodiumLegacyBackend.cpp';origLegacy=legacy.read_text();legacyAnchor='void DecodiumLegacyBackend::logQso()\n{';assert origLegacy.count(legacyAnchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);m_mode=QStringLiteral("FT8");m_useLegacyTxBackend=true;m_forceLegacyTxForSpecialOp=false;m_monitoring=false;m_transmitting=false;m_tuning=false;m_callsign=QStringLiteral("9H1SR");m_dxCall=QStringLiteral("K9ZZZ");m_qsoLogged=false;m_promptLogSnapshotValid=true;m_promptLogCall=QStringLiteral("K9ZZZ");m_promptLogGrid=QStringLiteral("FN42");m_promptLogMode=QStringLiteral("FT8");m_promptLogDialFreq=14074000;m_promptLogOn=QDateTime::currentDateTimeUtc();m_promptLogOff=m_promptLogOn;setSetting(QStringLiteral("UseLegacyBackendLogWriter"),true);m_legacyBackend->setDxCall(QStringLiteral("K9ZZZ"));m_legacyBackend->setDxGrid(QStringLiteral("FN42"));bool selected=usingLegacyBackendForTx();logQsoNow();return {{"rigEnabled",m_legacyBackend->rigControlEnabled()},{"legacy",selected},{"cat",m_catConnected},{"tx",m_transmitting},{"dir",QCoreApplication::instance()->property("decodiumEmbeddedLegacyDataDir").toString()}};}
#endif
'''
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{console.log("[DIALOG-START] "+JSON.stringify(bridge.logSstvQso({})));}}
 Timer {interval:16000;running:true;repeat:false;onTriggered:{Qt.quit();}}
'''

rigs=[];values=[]
try:
 init.write_text(initOriginal.replace(macro,'#define embeddedLegacyRigControlEnabledForBackend(...) false',1))
 sstv.write_text(s.replace(anchor,anchor+inj,1));None
 window.write_text(winOrig.replace('void MainWindow::legacyLogQso()\n{','void MainWindow::legacyLogQso()\n{\n qWarning("[LAB] legacyLogQso entered");',1).replace('  if (logHisCall.trimmed ().isEmpty ()) {','  qWarning() << "[LAB] logHisCall:" << logHisCall;\n  if (logHisCall.trimmed ().isEmpty ()) {',1));logdlg.write_text(dlgOrig.replace('void LogQSO::accept()\n{','void LogQSO::accept()\n{\n qWarning("[LAB] LogQSO accept entered");',1))
 for label in ('before','after'):
  if label=='before':
   main.write_text(m+'\n#undef bridgeLog\n'+head.split('QString bridgeAdifRecordText(',1)[0]+fixture);module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='LegacyWriterReal'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-modecleanup-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   starts=[line.split('[DIALOG-START] ',1)[1] for line in result.stdout.splitlines() if '[DIALOG-START] ' in line]
   if result.returncode or len(starts)!=1 :raise RuntimeError(result.stdout[-10000:])
   value=json.loads(starts[0]);assert not value['rigEnabled'] and value['legacy'] and not value['cat'] and not value['tx'] and '/.qttest/' in value['dir'],value
   print('diagnostics:', '\n'.join(x for x in result.stdout.splitlines() if '[LAB]' in x),flush=True);print('ADIF locations:',[str(p) for p in Path(value['dir']).parent.rglob('*.adi')],flush=True);path=Path(value['dir'])/'wsjtx_log.adi'; data=path.read_text(errors='replace') if path.exists() else '';count=data.upper().count('K9ZZZ');print(label,'legacy file entries',count,flush=True);assert count>=1,(value,str(path));values.append({'count':count})
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q);legacy.write_text(origLegacy);window.write_text(winOrig);logdlg.write_text(dlgOrig);init.write_text(initOriginal)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
