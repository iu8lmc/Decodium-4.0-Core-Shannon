#!/usr/bin/env python3
"""Compare FT scheduler inside isolated real application processes."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; module=r/'src/bridge/DecodiumBridgeLogRouting.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,s,n,q=(p.read_text() for p in (main,sstv,module,qml))
fixture=(r/'tests/bridge_refactor_fixtures/bridge_logging_before.inc').read_text()
assert fixture in n
head=n[:-len(fixture)]
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert s.count(anchor)==1
prototypes=""
injection='''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values);bool managers=m_mediaDevices&&m_themeManager&&m_propagationManager&&m_diagnostics&&m_macroManager&&m_bandManager&&m_cat4OmCat&&m_omniRigCat&&m_hamlibCat&&m_activeStations&&m_alertManager;bool timers=m_weatherRefreshTimer&&m_legacyStateTimer;bool sstv=m_sstvRxRuntime&&m_sstvAudioRelay;return {{"managers",managers},{"timers",timers},{"sstv",sstv},{"mode",m_mode},{"cat",m_catConnected},{"tx",m_transmitting}}; }
#endif
'''
probe='''
 Timer { interval:12000; running:true; repeat:false; onTriggered:Qt.quit() }
 Timer { interval:10000; running:true; repeat:false; onTriggered:{bridge.qmlDebugLog('SYNTHETIC-LOG-ROUTING-AB');console.log('[LOGPATH] '+bridge.diagnosticLogPath());console.log('[FTSCHED-PROBE] '+JSON.stringify(bridge.logSstvQso({})))} }
'''
rigs=[]; results=[]
try:
 sstv.write_text(s.replace(anchor,prototypes+anchor+injection,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_CONSTRUCTOR_INSERTION_POINT'; assert m.count(point)==1
   main.write_text(m.replace(point,'#undef bridgeLog\n'+fixture+'\nvoid decodiumBridgeTxPrepareTimelineLog(const QString& message) { txTimelineLog(message); }\n#define bridgeLog decodiumBridgeUiActionLog\n'+point)); module.write_text('#include <QString>\n')
  else: main.write_text(m); module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  rig='LogRouteLab'+secrets.token_hex(5); rigs.append(rig); marker='SYNTHETIC-LOG-ROUTING-AB-'+rig; qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe.replace('SYNTHETIC-LOG-ROUTING-AB',marker),1))
  with tempfile.TemporaryDirectory(prefix='decodium-ftsched-runtime-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[FTSCHED-PROBE] ',1)[1] for line in proc.stdout.splitlines() if '[FTSCHED-PROBE] ' in line]
   if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(proc.stdout[-6000:])
   paths=[line.split('[LOGPATH] ',1)[1] for line in proc.stdout.splitlines() if '[LOGPATH] ' in line]; assert len(paths)==1 and marker in Path(paths[0]).read_text(errors='replace')
   value=json.loads(rows[0]); expected={'managers':True,'timers':True,'sstv':True,'mode':'FT8','cat':False,'tx':False}
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
