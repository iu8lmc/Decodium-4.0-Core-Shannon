#!/usr/bin/env python3
"""Compare legacy filter signal wiring in isolated real applications."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridgeConstructor.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeLegacyFilterConnections.cpp';qml=r/'build/qml/decodium/Main.qml';m,s,n,q=(p.read_text() for p in (main,sstv,module,qml));first=(r/'tests/bridge_refactor_fixtures/legacy_cq_filter_connections_before.inc').read_text();second=(r/'tests/bridge_refactor_fixtures/legacy_mycall_filter_connection_before.inc').read_text();anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
legacy_init=r/'src/bridge/DecodiumBridgeLegacyInit.cpp';legacy_original=legacy_init.read_text();rig_macro='#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled';assert legacy_original.count(rig_macro)==1
injection='''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values);bool legacy=usingLegacyBackendForTx()&&m_legacyBackend&&m_legacyBackend->available();bool previous=m_syncingLegacyBackendDecodeList;m_syncingLegacyBackendDecodeList=true;
 auto reset=[this](){m_legacyBandActivityRevision=42;m_legacyRxFrequencyRevision=43;m_legacyAllTxtRevisionKey=QStringLiteral("sentinel");};
 auto cleared=[this](){return m_legacyBandActivityRevision==-1&&m_legacyRxFrequencyRevision==-1&&m_legacyAllTxtRevisionKey.isEmpty();};
 reset();emit filterCqOnlyChanged();bool cq=cleared();reset();emit cqFilterLevelChanged();bool level=cleared();reset();emit filterMyCallOnlyChanged();bool mycall=cleared();m_syncingLegacyBackendDecodeList=previous;return {{"legacy",legacy},{"cq",cq},{"level",level},{"mycall",mycall},{"cat",m_catConnected},{"tx",m_transmitting}}; }
#endif
'''
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:Qt.quit()}
 Timer {interval:10000;running:true;repeat:false;onTriggered:console.log('[LEGACYFILTER-PROBE] '+JSON.stringify(bridge.logSstvQso({}))) }
'''
rigs=[];out=[]
try:
 legacy_init.write_text(legacy_original.replace(rig_macro,'#define embeddedLegacyRigControlEnabledForBackend(...) false',1))
 sstv.write_text(s.replace(anchor,anchor+injection,1))
 for label in ('before','after'):
  if label=='before':
   assert m.count('    initialiseLegacyCqFilterConnections();\n')==1 and m.count('    initialiseLegacyMyCallFilterConnection();\n')==1
   main.write_text(m.replace('    initialiseLegacyCqFilterConnections();\n',first,1).replace('    initialiseLegacyMyCallFilterConnection();\n',second,1));module.write_text('#include "DecodiumBridge.h"\n')
  else:main.write_text(m);module.write_text(n)
  build=subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
  if build.returncode:raise RuntimeError(build.stdout[-12000:])
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='LegacyFilter'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-legacyfilter-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45);rows=[x.split('[LEGACYFILTER-PROBE] ',1)[1] for x in proc.stdout.splitlines() if '[LEGACYFILTER-PROBE] ' in x]
   if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(proc.stdout[-6000:])
   value=json.loads(rows[0]);expected={'legacy':True,'cq':True,'level':True,'mycall':True,'cat':False,'tx':False};assert value==expected,value;out.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert out[0]==out[1]
finally:
 legacy_init.write_text(legacy_original)
 for p,text in ((main,m),(sstv,s),(module,n),(qml,q)):p.write_text(text)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   target=parent/('Decodium - '+rig+' - test')
   if target.is_dir():shutil.rmtree(target)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
