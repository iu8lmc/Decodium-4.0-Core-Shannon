#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgePeriodicTx.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/periodic_tx_before.inc').read_text();mark='void DecodiumBridge::checkAndStartPeriodicTx()';head=n.split(mark,1)[0];assert n==head+fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);m_mode=QStringLiteral("FT8");m_useLegacyTxBackend=false;m_forceLegacyTxForSpecialOp=false;m_monitoring=true;m_transmitting=false;m_tuning=false;m_txEnabled=true;m_manualTxHold=false;m_asyncTxEnabled=false;m_autoCqRepeat=false;m_multiAnswerMode=false;m_autoSeq=false;m_periodicTxInFlight=false;m_currentTx=2;m_qsoProgress=3;m_dxCall=QStringLiteral("K1ABC");m_callsign=QStringLiteral("9H1SR");m_txDisabledMask=0;m_lastNtx=-1;m_stationTelemetryPending=false;setTxMessage(2,QStringLiteral("K1ABC 9H1SR -10"));int slot=int(correctedUtcMsecsSinceStartOfDay()/15000);m_txPeriod=(slot%2)==0?1:0;checkAndStartPeriodicTx();bool scheduled=m_syncTxRetryScheduled;bool canceled=false;return {{"scheduled",scheduled},{"canceled",canceled},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''\n Timer {interval:50;running:true;repeat:true;property int ticks:0;onTriggered:{if(++ticks<200 || Date.now()%15000<12000)return;stop();bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[MODE-CLEANUP] '+JSON.stringify(bridge.logSstvQso({})));finish.start();}}\n Timer {id:finish;interval:22000;repeat:false;onTriggered:{console.log('[LAB-WAIT-FINISHED]');Qt.quit();}}\n'''
originalMain=m;originalModule=n
startTxFile=r/'src/bridge/DecodiumBridgeStartTx.cpp';startTxOriginal=startTxFile.read_text()
sync=r/'src/bridge/DecodiumBridgeSyncSchedule.cpp';syncOriginal=sync.read_text()
rigs=[];values=[]
try:
 point='QTimer::singleShot(delayMs, Qt::PreciseTimer, this, [this, reason, modeSnapshot, retrySerial, requestGeneration]() {';assert syncOriginal.count(point)==1
 sync.write_text(syncOriginal.replace(point,point+'\n        qWarning() << "[LAB-RETRY-CALLBACK]" << retrySerial << m_syncTxRetrySerial << requestGeneration << m_txRequestGeneration << m_monitoring << m_manualTxHold << m_transmitting << m_tuning << m_txEnabled;',1))
 fixture=fixture.replace('        if (latestStartMs > 0 && elapsedMs >= latestStartMs) {', '        if (latestStartMs > 0 && elapsedMs >= latestStartMs) { qWarning("[LAB-PERIODIC-LATE]");',1);n=head+fixture
 signature='void DecodiumBridge::startTx()\n{';assert startTxOriginal.count(signature)==1;startTxFile.write_text(startTxOriginal.replace(signature,signature+'\n    qWarning("[LAB-PERIODIC-START]"); return;',1))
 sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='PeriodicTxRetry'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-modecleanup-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','50000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=65)
   rows=[line.split('[MODE-CLEANUP] ',1)[1] for line in result.stdout.splitlines() if '[MODE-CLEANUP] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);
   if '[LAB-PERIODIC-START]' not in result.stdout: print(result.stdout[-10000:],flush=True)
   assert '[LAB-PERIODIC-LATE]' in result.stdout and '[LAB-PERIODIC-START]' in result.stdout and value['scheduled'] and not value['canceled'] and '[LAB-WAIT-FINISHED]' in result.stdout and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 startTxFile.write_text(startTxOriginal);main.write_text(originalMain);sstv.write_text(s);module.write_text(originalModule);qml.write_text(q);sync.write_text(syncOriginal)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
