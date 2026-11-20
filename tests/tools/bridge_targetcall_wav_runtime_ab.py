#!/usr/bin/env python3
"""Compare Target Call WAV-decode-triggered transition in isolated real application processes."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; module=r/'src/bridge/DecodiumBridgeTargetCallSettings.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,s,n,q=(p.read_text() for p in (main,sstv,module,qml))
legacy=r/'src/bridge/DecodiumBridgeLegacyInit.cpp'; legacy_original=legacy.read_text(); legacy_macro='#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled'; assert legacy_original.count(legacy_macro)==1
signature='void DecodiumBridge::setTargetCallSign(';head,tail=n.split(signature,1);fixture=signature+tail
baseline=subprocess.check_output(['git','show','v1.0.737:src/bridge/DecodiumBridge.cpp'],cwd=r,text=True)
old=baseline[baseline.index(signature):baseline.index('// === CAT ===',baseline.index(signature))]
normalized=fixture.replace('decodiumBridgeLogbookLog(', 'bridgeLog(').replace('\n\n\nvoid DecodiumBridge::startTargetCall()', '\n\nvoid DecodiumBridge::startTargetCall()')
assert normalized==old, 'Target Call body differs from the release baseline'
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert s.count(anchor)==1
prototypes="""#include <QDateTime>
"""
injection='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);m_suppressImmediateTxEnableDispatch=true;m_startupTxGuard=false;m_monitoring=false;m_transmitting=false;m_mode=QStringLiteral("FT8");m_callsign=QStringLiteral("9H1SR");m_grid=QStringLiteral("JM75");setTargetCallSign(QStringLiteral("K1ABC"));setArmedWatchEnabled(true);startTargetCall();return {{"armed",m_targetCallActive&&m_targetCallArmedWaiting},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{var s=bridge.logSstvQso({});console.log("[TARGET-START] "+JSON.stringify(s));if(s.armed&&!s.cat&&!s.tx){bridge.filtersBypassed=true;bridge.openWavForDecode(__WAV__);watch.start();}}}
 Timer {id:watch;interval:250;repeat:true;property int ticks:0;onTriggered:{if(bridge.targetCallActive&&!bridge.targetCallArmedWaiting&&bridge.dxCall==="K1ABC"){bridge.halt();console.log("[FTSCHED-PROBE] "+JSON.stringify({armed:true,calling:true,cat:bridge.catConnected,tx:bridge.transmitting}));stop();Qt.quit();}else if(++ticks>=64){console.log("[FTSCHED-PROBE] timeout");stop();Qt.quit();}}}
'''.replace('__WAV__',json.dumps(str(r/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav')))

rigs=[]; results=[]
try:
 legacy.write_text(legacy_original.replace(legacy_macro,'#define embeddedLegacyRigControlEnabledForBackend(...) false',1))
 sstv.write_text(s.replace(anchor,prototypes+anchor+injection,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_CONSTRUCTOR_INSERTION_POINT'; assert m.count(point)==1
   main.write_text(m.replace(point,head+fixture+point)); module.write_text(head)
  else: main.write_text(m); module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1)); rig='TargetWavLab'+secrets.token_hex(5); rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-ftsched-runtime-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-audio-output','BlackHole 2ch','--lab-quit-ms','35000']
   proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=50)
   rows=[line.split('[FTSCHED-PROBE] ',1)[1] for line in proc.stdout.splitlines() if '[FTSCHED-PROBE] ' in line]
   if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(proc.stdout[-6000:])
   if rows[0]=='timeout': raise RuntimeError('Target did not enter calling after WAV decode: '+proc.stdout[-9000:])
   value=json.loads(rows[0]); expected={'armed':True,'calling':True,'cat':False,'tx':False}
   assert value==expected,value
   results.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert results[0]==results[1]
finally:
 legacy.write_text(legacy_original)
 for p,text in ((main,m),(sstv,s),(module,n),(qml,q)):p.write_text(text)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
