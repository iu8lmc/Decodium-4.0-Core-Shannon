#!/usr/bin/env python3
"""Compare core timer initialization in isolated apps."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeSstv.cpp'; mod=r/'src/bridge/DecodiumBridgeSstvStudioArchive.cpp'; sstv=main; qml=r/'build/qml/decodium/Main.qml'
m,n,q=(p.read_text() for p in (main,mod,qml));s=m;fixture=(r/'tests/bridge_refactor_fixtures/hamdrm_gallery_archive_before.inc').read_text();assert n.endswith(fixture);head=n[:-len(fixture)]
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
 {Q_UNUSED(values);using namespace decodium::sstv;initialiseSstvRuntime();if(!m_sstvStorageWorker)return {{"error","worker"}};bool pending=!m_sstvStorageReady&&m_sstvStorageInitializationPending;int warning=0;auto cw=connect(this,&DecodiumBridge::statusMessage,this,[&](QString text){if(text.contains(QStringLiteral("queue is full")))++warning;});QImage image(32,24,QImage::Format_RGB32);image.fill(Qt::red);for(int i=0;i<9;++i)queueHamDrmTransmittedImage(image,QStringLiteral("hamdrm-test"),2500,true,true,{});bool bounded=m_pendingHamDrmGalleryImages.size()==8&&m_pendingHamDrmGalleryBytes==8*image.sizeInBytes()&&warning==1;disconnect(cw);int stored=0;bool valid=true;QEventLoop loop;QTimer::singleShot(25000,&loop,&QEventLoop::quit);auto cs=connect(m_sstvStorageWorker,&SstvStorageWorker::imageStoreFinished,&loop,[&](quint64,bool ok,SstvImageRecord rec,QString){valid &= ok&&rec.category==SstvImageCategory::Received&&rec.digital&&QFileInfo::exists(rec.imagePath)&&rec.imagePath.contains(QStringLiteral("/.qttest/"));if(ok)++stored;if(stored>=8)loop.quit();});if(stored<8)loop.exec();disconnect(cs);bool flushed=stored==8&&valid&&m_sstvStorageReady&&m_pendingHamDrmGalleryImages.empty()&&m_pendingHamDrmGalleryBytes==0;bool ok=pending&&bounded&&flushed&&!m_catConnected&&!m_transmitting;return {{"wasShutdown",!ok},{"shutdown",ok},{"pending",pending},{"bounded",bounded},{"flushed",flushed},{"stored",stored},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 

probe='''\n Timer {interval:14000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 for label in ('before','after'):
  instrumented=s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QFile>\n#include <QFileInfo>\n#include <QElapsedTimer>\n#include <QEventLoop>\n#include <QThread>\n#include <QTimer>',1).replace(anchor,anchor+inj,1)
  if label=='before':
   main.write_text(instrumented+'\n'+fixture);mod.write_text(head)
  else:main.write_text(instrumented);mod.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='HamdrmDeferred'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SSTV_STORAGE_DELAY_MS='25000',LAB_ADIF_PATH=tmp+'/synthetic.adi',DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   sockets=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sockets.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','65000']
   result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=85)
   rows=[line.split('[DISPLAY-FREQ-LAB] ',1)[1] for line in result.stdout.splitlines() if '[DISPLAY-FREQ-LAB] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-8000:])
   val=json.loads(rows[0]);assert not val['wasShutdown'] and val['shutdown'] and not val['cat'] and not val['tx'],val
   readProbe="\n Timer {interval:9000;running:true;repeat:false;onTriggered:{console.log('[SHUTDOWN-READ] '+String(bridge.getSetting('ShutdownSynthetic','missing')));Qt.quit();}}\n"
   qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+readProbe,1))
   reopened=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=85)
   persisted=[line.split('[SHUTDOWN-READ] ',1)[1] for line in reopened.stdout.splitlines() if '[SHUTDOWN-READ] ' in line]
   if reopened.returncode or persisted!=['persisted']:raise RuntimeError('persistence readback failed: '+reopened.stdout[-8000:])
   val['readback']=persisted[0];results.append(val);print(label,json.dumps(val,sort_keys=True),flush=True)
 assert results[0]==results[1]
finally:
 for p,content in ((main,m),(mod,n),(qml,q)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
