#!/usr/bin/env python3
"""Compare autolog helpers inside isolated real application processes."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; module=r/'src/bridge/DecodiumBridgeAutologHelpers.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,s,n,q=(p.read_text() for p in (main,sstv,module,qml))
fixture=(r/'tests/bridge_refactor_fixtures/autolog_helpers_before.inc').read_text()
assert n.endswith(fixture)
head=n[:-len(fixture)]
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert s.count(anchor)==1
prototypes='''void decodiumBridgePruneDuplicateCache(QHash<QString,QDateTime>&,const QDateTime&);
QString decodiumBridgeInferPartnerForAutolog(const QString&,const QString&,const QString&);
bool decodiumBridge_isWithinRecentDuplicateWindow(const QDateTime&,const QDateTime&);
bool decodiumBridge_isWithinRecentAutoCqPartnerWindow(const QDateTime&,const QDateTime&);
'''
injection='''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values);const auto now=QDateTime::currentDateTimeUtc();QHash<QString,QDateTime> cache;for(int i=0;i<513;++i)cache.insert(QString::number(i),now.addSecs(-1801));cache.insert("keep",now);decodiumBridgePruneDuplicateCache(cache,now);return {{"cache",cache.size()==1&&cache.contains("keep")}, {"call",decodiumBridgeInferPartnerForAutolog("IU8LMC K1ABC -10","IU8LMC","IU8LMC")==QStringLiteral("K1ABC")},{"dup",decodiumBridge_isWithinRecentDuplicateWindow(now.addSecs(-90),now)&&!decodiumBridge_isWithinRecentDuplicateWindow(now.addSecs(-91),now)}, {"autocq",decodiumBridge_isWithinRecentAutoCqPartnerWindow(now.addSecs(-600),now)&&!decodiumBridge_isWithinRecentAutoCqPartnerWindow(now.addSecs(-601),now)}, {"cat",m_catConnected},{"tx",m_transmitting}}; }
#endif
'''
probe='''
 Timer { interval:12000; running:true; repeat:false; onTriggered:Qt.quit() }
 Timer { interval:10000; running:true; repeat:false; onTriggered:console.log('[AUTOLOG-PROBE] '+JSON.stringify(bridge.logSstvQso({}))) }
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
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1)); rig='AutoLogHelperLab'+secrets.token_hex(5); rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-autolog-runtime-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[AUTOLOG-PROBE] ',1)[1] for line in proc.stdout.splitlines() if '[AUTOLOG-PROBE] ' in line]
   if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(proc.stdout[-6000:])
   value=json.loads(rows[0]); expected={'cache':True,'call':True,'dup':True,'autocq':True,'cat':False,'tx':False}
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
