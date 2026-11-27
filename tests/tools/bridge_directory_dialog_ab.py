#!/usr/bin/env python3
"""Compare directory selection and cancellation in isolated real application processes."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; module=r/'src/bridge/DecodiumBridgeReceivedFiles.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,s,n,q=(p.read_text() for p in (main,sstv,module,qml))
signature='QString DecodiumBridge::openDirectoryDialog(';start=n.index(signature);end=n.index('QString DecodiumBridge::defaultFt2LinkReceivedFilesDirectory()',start);fixture=n[start:end];head=n[:start]+n[end:]; helper=n[n.index('static QString localDialogPath('):n.index('static QString safeFt2LinkReceivedDirectoryName(')]
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert s.count(anchor)==1
prototypes="""#include <QDateTime>\n#include <QApplication>\n#include <QFileDialog>\n#include <QFileInfo>\n#include <QDir>
"""
injection='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QString dir=qEnvironmentVariable("LAB_DIRECTORY");auto closeDialog=[&](bool accept){QTimer::singleShot(300,qApp,[=](){for(QWidget* w:QApplication::topLevelWidgets()){auto* dialog=qobject_cast<QFileDialog*>(w);if(dialog&&dialog->isVisible()){if(accept){dialog->setDirectory(dir);dialog->selectFile(QStringLiteral("."));QMetaObject::invokeMethod(dialog,"accept");}else dialog->reject();}}});};closeDialog(false);QString cancelled=openDirectoryDialog(QStringLiteral("cancel"),dir);closeDialog(true);QString chosen=openDirectoryDialog(QStringLiteral("select"),QUrl::fromLocalFile(dir).toString());bool selected=QDir(chosen).canonicalPath()==QDir(dir).canonicalPath();return {{"cancelled",cancelled.isEmpty()},{"selected",selected},{"cat",m_catConnected},{"tx",m_transmitting}};}
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
   main.write_text(m.replace(point,"#include <QFileDialog>\n#include <QApplication>\n"+helper+fixture+point)); module.write_text(head)
  else: main.write_text(m); module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1)); rig='LogSnapshotLab'+secrets.token_hex(5); rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-ftsched-runtime-') as tmp:
   selected_dir=Path(tmp)/'Spazio prova è';selected_dir.mkdir()
   env=os.environ.copy();env.update(LAB_DIRECTORY=str(selected_dir),DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[FTSCHED-PROBE] ',1)[1] for line in proc.stdout.splitlines() if '[FTSCHED-PROBE] ' in line]
   if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(proc.stdout[-6000:])
   value=json.loads(rows[0]); expected={'cancelled':True,'selected':True,'cat':False,'tx':False}
   assert all(value.get(k)==v for k,v in expected.items()),value
   results.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert results[0]==results[1]
finally:
 for p,text in ((main,m),(sstv,s),(module,n),(qml,q)):p.write_text(text)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
