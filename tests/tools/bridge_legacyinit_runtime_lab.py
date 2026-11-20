#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeLegacyInit.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/legacyinit_before.inc').read_text();mark='bool DecodiumBridge::ensureLegacyBackendAvailable()';head=n.split(mark,1)[0];tail='#undef embeddedLegacyRigControlEnabledForBackend\n#undef activeCatCanPtt\n#undef activeCatUsesVoxPtt\n#undef activeCatSetTxPtt\n#undef activeCatSetTxPttAsync\n#undef catSuppressedByEnvironment\n#undef txTimelineLog\n#undef bridgeLog\n';assert n.startswith(head+fixture+tail);head+=n[len(head+fixture):];safeHead=head.replace("#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled", "#define embeddedLegacyRigControlEnabledForBackend(...) false");safeModule=n.replace("#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled", "#define embeddedLegacyRigControlEnabledForBackend(...) false")
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);bool ok=ensureLegacyBackendAvailable();auto* first=m_legacyBackend;bool okAgain=ensureLegacyBackendAvailable();bool reused=first==m_legacyBackend;int warnings=0;int setups=0;auto wc=QObject::connect(this,&DecodiumBridge::warningRaised,this,[&](const QString&t,const QString&s,const QString&d){if(t=="synthetic title" && s=="synthetic summary" && d=="synthetic details")++warnings;});auto sc=QObject::connect(this,&DecodiumBridge::setupSettingsRequested,this,[&](int tab){if(tab==0)++setups;});m_legacyBackend->warningRaised(QStringLiteral("synthetic title"),QStringLiteral("synthetic summary"),QStringLiteral("synthetic details"));m_legacyBackend->preferencesRequested();m_txRequested=true;m_pttPending=true;m_pttConfirmed=true;m_pttOnDispatched=false;m_legacyBackend->rigPttRequested(false);bool pttCleared=!m_txRequested && !m_pttPending && !m_pttConfirmed;QObject::disconnect(wc);QObject::disconnect(sc);return {{"rigEnabled",m_legacyBackend->rigControlEnabled()},{"ok",ok},{"okAgain",okAgain},{"reused",reused},{"pttCleared",pttCleared},{"warnings",warnings},{"setups",setups},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[LEGACY-INIT] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   main.write_text(m+'\n#undef bridgeLog\n'+safeHead.split(tail,1)[0]+fixture+tail);module.write_text(safeHead)
  else:main.write_text(m);module.write_text(safeModule)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='LegacyInit'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-legacyinit-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[LEGACY-INIT] ',1)[1] for line in result.stdout.splitlines() if '[LEGACY-INIT] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert not value['rigEnabled'] and value['ok'] and value['okAgain'] and value['reused'] and value['pttCleared'] and value['warnings']==1 and value['setups']==1 and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
