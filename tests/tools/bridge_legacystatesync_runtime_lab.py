#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeLegacyState.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/legacystatesync_before.inc').read_text();mark='void DecodiumBridge::syncLegacyBackendState()';head=n.split(mark,1)[0];tail=n[len(head+fixture):];assert n==head+fixture+tail and tail.startswith('#undef useLegacyRigControlFallback');head+=tail
init=r/'src/bridge/DecodiumBridgeLegacyInit.cpp';initOriginal=init.read_text();macro='#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled';assert initOriginal.count(macro)==1
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);bool available=ensureLegacyBackendAvailable();bool usesLegacy=usingLegacyBackendForRx();m_localRxFrequencyGuardUntilMs=0;m_localTxFrequencyGuardUntilMs=0;m_legacyBackend->setRxFrequency(1600);m_legacyBackend->setTxFrequency(1700);m_legacyBackend->setTxFirst(true);m_legacyBackend->setAlt12Enabled(true);m_legacyBackend->setRxInputLevel(37);m_legacyBackend->setTxOutputAttenuation(12);m_legacyBackend->setTxMessage(1,QStringLiteral("LEGACY SYNTHETIC"));m_tx1=QStringLiteral("BRIDGE SYNTHETIC");int backendRx=m_legacyBackend->rxFrequency();int backendTx=m_legacyBackend->txFrequency();m_rxFrequency=400;m_txFrequency=500;m_syncingLegacyBackendState=true;syncLegacyBackendState();bool reentry=m_rxFrequency==400 && m_txFrequency==500;m_syncingLegacyBackendState=false;syncLegacyBackendState();bool rxSynced=m_rxFrequency==backendRx;bool txSynced=m_txFrequency==backendTx;bool guardReset=!m_syncingLegacyBackendState;bool period=m_txPeriod==(m_legacyBackend->txFirst()?1:0);bool alt=m_alt12Enabled==m_legacyBackend->alt12Enabled();bool inputLevel=qFuzzyCompare(m_rxInputLevel+1.0,qBound(0.0,static_cast<double>(m_legacyBackend->rxInputLevel()),100.0)+1.0);bool outputLevel=m_txOutputLevel>=0.0 && m_txOutputLevel<=100.0;bool bridgeMessage=m_tx1==QStringLiteral("BRIDGE SYNTHETIC");return {{"rigEnabled",m_legacyBackend->rigControlEnabled()},{"available",available},{"usesLegacy",usesLegacy},{"rxSynced",rxSynced},{"txSynced",txSynced},{"reentry",reentry},{"guardReset",guardReset},{"period",period},{"alt",alt},{"inputLevel",inputLevel},{"outputLevel",outputLevel},{"bridgeMessage",bridgeMessage},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[LEGACY-STATE] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 init.write_text(initOriginal.replace(macro,'#define embeddedLegacyRigControlEnabledForBackend(...) false',1))
 sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   main.write_text(m+'\n#undef bridgeLog\n'+head.split(tail,1)[0]+fixture+tail);module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='LegacyStateSync'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-legacystate-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[LEGACY-STATE] ',1)[1] for line in result.stdout.splitlines() if '[LEGACY-STATE] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert not value['rigEnabled'] and value['available'] and value['usesLegacy'] and value['rxSynced'] and value['txSynced'] and value['reentry'] and value['guardReset'] and value['period'] and value['alt'] and value['inputLevel'] and value['outputLevel'] and value['bridgeMessage'] and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q);init.write_text(initOriginal)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
