#!/usr/bin/env python3
"""Compare FT scheduler inside isolated real application processes."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; module=r/'src/bridge/DecodiumBridgeLogPreview.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,s,n,q=(p.read_text() for p in (main,sstv,module,qml))
signature='QVariantMap DecodiumBridge::pendingLogQsoPreview() const';head,tail=n.split(signature,1);fixture=signature+tail
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert s.count(anchor)==1
prototypes="""#include <QDateTime>
"""
injection='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);auto t=QDateTime::fromString(QStringLiteral("2026-01-02T03:04:05Z"),Qt::ISODate);m_promptLogSnapshotValid=true;m_promptLogCall=QStringLiteral("K1ABC");m_promptLogGrid=QStringLiteral("FN42");m_promptLogRptSent=QStringLiteral("-10");m_promptLogRptRcvd=QStringLiteral("-08");m_promptLogDialFreq=14074000;m_promptLogMode=QStringLiteral("FT8");m_logModeOverride=QStringLiteral("FT4");m_promptLogOn=t;m_promptLogOff=QDateTime{};m_promptLogCommentOverrideValid=true;m_promptLogComment=QStringLiteral("snapshot");auto snapshot=pendingLogQsoPreview();bool snap=snapshot.value("call")==QStringLiteral("K1ABC")&&snapshot.value("grid")==QStringLiteral("FN42")&&snapshot.value("mode")==QStringLiteral("FT4")&&snapshot.value("timeOn")==snapshot.value("timeOff")&&snapshot.value("comment")==QStringLiteral("snapshot");m_promptLogSnapshotValid=false;m_logModeOverride.clear();m_pendingAutoLogValid=true;m_pendingAutoLogCall=QStringLiteral("K2ABC");m_pendingAutoLogGrid=QStringLiteral("FN31");m_pendingAutoLogOn=t;m_pendingAutoLogOff=t.addSecs(-60);m_pendingAutoLogDialFreq=7074000;m_mode=QStringLiteral("FT8");m_promptLogComment=QStringLiteral("pending");auto pending=pendingLogQsoPreview();bool pend=pending.value("call")==QStringLiteral("K2ABC")&&pending.value("grid")==QStringLiteral("FN31")&&pending.value("timeOn")==pending.value("timeOff")&&pending.value("comment")==QStringLiteral("pending");m_pendingAutoLogValid=false;m_dxCall=QStringLiteral("K3ABC");m_dxGrid=QStringLiteral("FM19");auto live=pendingLogQsoPreview();bool curr=live.value("call")==QStringLiteral("K3ABC")&&live.value("grid")==QStringLiteral("FM19");return {{"snapshot",snap},{"pending",pend},{"live",curr},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''
 Timer { interval:12000; running:true; repeat:false; onTriggered:Qt.quit() }
 Timer { interval:10000; running:true; repeat:false; onTriggered:console.log('[FTSCHED-PROBE] '+JSON.stringify(bridge.logSstvQso({}))) }
'''
rigs=[]; results=[]
try:
 sstv.write_text(s.replace(anchor,prototypes+anchor+injection,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_CONSTRUCTOR_INSERTION_POINT'; assert m.count(point)==1
   main.write_text(m.replace(point,head+fixture+point)); module.write_text(head)
  else: main.write_text(m); module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1)); rig='LogSnapshotLab'+secrets.token_hex(5); rigs.append(rig)
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
   value=json.loads(rows[0]); expected={'snapshot':True,'pending':True,'live':True,'cat':False,'tx':False}
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
