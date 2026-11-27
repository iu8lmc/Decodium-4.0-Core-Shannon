#!/usr/bin/env python3
"""Compare legacy rig policy inside isolated real application processes."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; module=r/'src/bridge/DecodiumBridgeLegacyRigPolicy.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,s,n,q=(p.read_text() for p in (main,sstv,module,qml))
fixture=(r/'tests/bridge_refactor_fixtures/legacy_rig_policy_before.inc').read_text()
assert n.endswith(fixture)
head=n[:-len(fixture)]
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert s.count(anchor)==1
prototypes='''bool decodiumBridgeLegacyRigControlFallback(DecodiumLegacyBackend*,const QString&);
bool decodiumBridgeEmbeddedLegacyRigEnabled(const QString&,bool);
bool decodiumBridgeLegacyOwnsRigControl(DecodiumLegacyBackend*);
'''
injection='''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values);bool legacy=usingLegacyBackendForTx()&&m_legacyBackend&&m_legacyBackend->available();return {{"legacy",legacy},{"fallback",!decodiumBridgeLegacyRigControlFallback(m_legacyBackend,"hamlib")},{"policy",decodiumBridgeEmbeddedLegacyRigEnabled("hamlib",false)&&!decodiumBridgeEmbeddedLegacyRigEnabled("omnirig",false)&&!decodiumBridgeEmbeddedLegacyRigEnabled("hamlib",true)},{"owner",!decodiumBridgeLegacyOwnsRigControl(nullptr)&&decodiumBridgeLegacyOwnsRigControl(m_legacyBackend)==(legacy&&m_legacyBackend->rigControlEnabled())},{"cat",m_catConnected},{"tx",m_transmitting}}; }
#endif
'''
probe='''
 Timer { interval:12000; running:true; repeat:false; onTriggered:Qt.quit() }
 Timer { interval:10000; running:true; repeat:false; onTriggered:console.log('[LEGACYRIG-PROBE] '+JSON.stringify(bridge.logSstvQso({}))) }
'''
rigs=[]; results=[]
try:
 sstv.write_text(s.replace(anchor,prototypes+anchor+injection,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n'; assert m.count(point)==1
   main.write_text(m.replace(point,fixture+point)); module.write_text(head)
  else: main.write_text(m); module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1)); rig='LegacyRigPolicyLab'+secrets.token_hex(5); rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-legacyrig-runtime-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[LEGACYRIG-PROBE] ',1)[1] for line in proc.stdout.splitlines() if '[LEGACYRIG-PROBE] ' in line]
   if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(proc.stdout[-6000:])
   value=json.loads(rows[0]); expected={'legacy':True,'fallback':True,'policy':True,'owner':True,'cat':False,'tx':False}
   assert value==expected,value
   results.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert results[0]==results[1]
finally:
 for p,text in ((main,m),(sstv,s),(module,n),(qml,q)):p.write_text(text)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
