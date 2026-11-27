#!/usr/bin/env python3
"""Compare WAV recording initialization in isolated apps."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeConstructor.cpp'; mod=r/'src/bridge/DecodiumBridgeWavInit.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,n,s,q=(p.read_text() for p in (main,mod,sstv,qml)); fixture=(r/'tests/bridge_refactor_fixtures/wav_init_before.inc').read_text();assert n.split('void DecodiumBridge::initialiseWavManager()\n{\n',1)[1]==fixture+'}\n'
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);QString dir=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+QStringLiteral("/Decodium/recordings/");QString now=QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));QString candidate=dir+QStringLiteral("decodium_")+now+QStringLiteral(".wav");bool safe=dir.startsWith(QDir::homePath()+QStringLiteral("/.qttest/"))&&!QFile::exists(candidate)&&!QFile::exists(dir+QStringLiteral("decodium_")+QDateTime::currentDateTimeUtc().addSecs(1).toString(QStringLiteral("yyyyMMdd_HHmmss"))+QStringLiteral(".wav"));int notices=0;auto c=connect(this,&DecodiumBridge::recordRxEnabledChanged,this,[&](){++notices;});bool started=false,stopped=false,wav=false,removed=false;if(safe){m_wavManager->startRecording();QString path=m_wavManager->lastFilePath();bool unique=!QFile::exists(path)&&path.startsWith(dir);started=unique&&m_wavManager->recording()&&m_recordRxEnabled;m_wavManager->feedSamples(QVector<short>{100,-200,300});m_wavManager->stopRecording();stopped=!m_wavManager->recording()&&!m_recordRxEnabled;QFile f(path);if(unique&&f.open(QIODevice::ReadOnly)){QByteArray bytes=f.readAll();wav=bytes.size()==50&&bytes.startsWith("RIFF")&&bytes.mid(8,4)=="WAVE";f.close();removed=f.remove();}}disconnect(c);bool ok=safe&&started&&stopped&&wav&&removed&&notices==2;return {{"wasShutdown",!ok},{"shutdown",ok},{"safe",safe},{"started",started},{"stopped",stopped},{"wav",wav},{"removed",removed},{"notices",notices},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''

probe='''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 sstv.write_text(s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QBuffer>\n#include <QStandardPaths>\n#include <QFile>\n#include <QDir>',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='    initialiseWavManager();\n'; assert m.count(point)==1; main.write_text(m.replace(point,fixture,1));mod.write_text(n)
  else:main.write_text(m);mod.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='WavInit'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   sockets=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sockets.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[DISPLAY-FREQ-LAB] ',1)[1] for line in result.stdout.splitlines() if '[DISPLAY-FREQ-LAB] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-8000:])
   val=json.loads(rows[0]);assert not val['wasShutdown'] and val['shutdown'] and not val['cat'] and not val['tx'],val
   readProbe="\n Timer {interval:9000;running:true;repeat:false;onTriggered:{console.log('[SHUTDOWN-READ] '+String(bridge.getSetting('ShutdownSynthetic','missing')));Qt.quit();}}\n"
   qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+readProbe,1))
   reopened=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   persisted=[line.split('[SHUTDOWN-READ] ',1)[1] for line in reopened.stdout.splitlines() if '[SHUTDOWN-READ] ' in line]
   if reopened.returncode or persisted!=['persisted']:raise RuntimeError('persistence readback failed: '+reopened.stdout[-8000:])
   val['readback']=persisted[0];results.append(val);print(label,json.dumps(val,sort_keys=True),flush=True)
 assert results[0]==results[1]
finally:
 for p,content in ((main,m),(mod,n),(sstv,s),(qml,q)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
