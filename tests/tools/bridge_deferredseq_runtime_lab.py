#!/usr/bin/env python3
"""Exercise FT2 deferred reply in isolated before/after app builds."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeAutoSeqAfterTx.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/deferredseq_before.inc').read_text();mark='void DecodiumBridge::armFt2AutoCqOneShotAfterCompletedTx(';head=n.split(mark,1)[0];assert (mark+n.split(mark,1)[1]).replace('decodiumBridgeDeferredSeqLog(', 'bridgeLog(')==fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''\n#if DECODIUM_HAS_SSTV\n    { Q_UNUSED(values); m_dxCall=QStringLiteral("K1ABC"); m_pendingAutoSeqTxAfterActiveTx=2; m_pendingAutoSeqPartnerBase=QStringLiteral("K1ABC"); m_pendingAutoSeqMessage=QStringLiteral("9H1SR K1ABC -10"); m_pendingAutoSeqMode=QStringLiteral("FT2");m_currentTx=6;m_multiAnswerMode=false;m_ft2AutoCqAwaitingPartnerTx=0;m_asyncTxEnabled=true;m_autoCqRepeat=true;m_txEnabled=true; const bool applied=applyPendingAutoSeqTxAfterCompletedTx(6);const int current=m_currentTx,awaiting=m_ft2AutoCqAwaitingPartnerTx;const bool gate=ft2AutoCqAwaitingPartnerDecode(),legacyEnabled=m_legacyBackend && m_legacyBackend->txEnabled(),enabled=m_txEnabled,autoState=m_autoCqRepeat,asyncState=m_asyncTxEnabled,multiState=m_multiAnswerMode;const QString modeState=m_mode,dxState=m_dxCall;const bool oldMonitor=m_monitoring;m_monitoring=true;checkAndStartPeriodicTx();const bool txAfterCheck=m_transmitting;m_monitoring=oldMonitor;m_txEnabled=false;m_autoCqRepeat=false;++m_txRequestGeneration;return {{QStringLiteral("applied"),applied},{QStringLiteral("current"),current},{QStringLiteral("awaiting"),awaiting},{QStringLiteral("txAfterCheck"),txAfterCheck},{QStringLiteral("gate"),gate},{QStringLiteral("legacyEnabled"),legacyEnabled},{QStringLiteral("enabled"),enabled},{QStringLiteral("auto"),autoState},{QStringLiteral("async"),asyncState},{QStringLiteral("multi"),multiState},{QStringLiteral("mode"),modeState},{QStringLiteral("dx"),dxState},{QStringLiteral("pending"),m_pendingAutoSeqTxAfterActiveTx},{QStringLiteral("cat"),m_catConnected},{QStringLiteral("tx"),m_transmitting}}; }\n#endif\n'''
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";bridge.grid="JM75";console.log('[DEFERRED-SEQ] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='DeferredSeq'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-deferredseq-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-audio-input','BlackHole 16ch','--lab-audio-output','BlackHole 2ch','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[DEFERRED-SEQ] ',1)[1] for line in result.stdout.splitlines() if '[DEFERRED-SEQ] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value['gate'] and value['applied'] and value['current']==2 and value['awaiting']==2 and value['pending']==0 and not value['cat'] and not value['tx'] and not value['txAfterCheck'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
