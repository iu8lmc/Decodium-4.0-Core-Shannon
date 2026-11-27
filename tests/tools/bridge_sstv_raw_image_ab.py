#!/usr/bin/env python3
"""Compare core timer initialization in isolated apps."""
import json, os, secrets, shutil, socket, subprocess, tempfile, wave
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeSstv.cpp'; mod=r/'src/bridge/DecodiumBridgeSstvRawAudio.cpp'; sstv=main; qml=r/'build/qml/decodium/Main.qml'
m,n,q=(p.read_text() for p in (main,mod,qml));s=m;fixture=(r/'tests/bridge_refactor_fixtures/sstv_raw_audio_before.inc').read_text();assert n.endswith(fixture);head=n[:-len(fixture)]
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);using namespace decodium::sstv;initialiseSstvRuntime();if(!m_sstvRxRuntime||!m_sstvRxAudioJobController)return {{"error","runtime"}};QEventLoop ready;QTimer::singleShot(5000,&ready,&QEventLoop::quit);auto cr=connect(this,&DecodiumBridge::sstvStorageStateChanged,&ready,[&](){if(m_sstvStorageReady)ready.quit();});if(!m_sstvStorageReady)ready.exec();disconnect(cr);if(!m_sstvStorageReady)return {{"error","storage"}};bool started=m_sstvRxRuntime->start(SstvAudioSourceKind::Replay,502);SstvMartinM1EncoderConfig cfg;cfg.mode=SstvMartinMode::M1;cfg.sampleRate=12000;std::vector<SstvRgbPixel> pixels(SstvMartinM1Encoder::pixelCount(cfg.mode),SstvRgbPixel{210U,70U,145U});SstvMartinM1Encoder encoder(pixels,cfg);std::vector<std::int16_t> payload(36000);bool encoded=encoder.pullPcm16(payload.data(),payload.size())==payload.size();auto token=m_sstvRxRuntime->routeToken();bool fed=true;quint64 chunks=0;for(int offset=0;offset<36000;offset+=4000){QVector<short> part;part.reserve(4000);for(int i=0;i<4000;++i)part.append(payload[offset+i]);fed &= m_sstvRxRuntime->enqueuePcm16At(part,12000,token,qint64(offset)*1000000000LL/12000);++chunks;QElapsedTimer wait;wait.start();while(m_sstvRxRuntime->snapshot().chunksProcessed<chunks&&wait.elapsed()<4000){QCoreApplication::processEvents();QThread::msleep(2);}}auto snap=m_sstvRxRuntime->snapshot();quint64 image=snap.image.acquisitionId;quint64 retained=snap.replay.mostRecentAcquisitionId;bool matched=image!=0&&(retained&0xffffffffULL)==image&&(retained>>32U)==(snap.image.generation&0xffffffffULL);bool saved=false;quint64 exported=0;QString out;QEventLoop job;QTimer::singleShot(10000,&job,&QEventLoop::quit);auto ce=connect(m_sstvRxAudioJobController,&SstvRxAudioJobController::rawAudioExportFinished,&job,[&](bool ok,QString path,quint64 acquisition,QString){saved=ok;out=path;exported=acquisition;job.quit();});bool requested=saveSstvRxRawAudio();if(requested)job.exec();disconnect(ce);QCoreApplication::processEvents();QString app=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);bool safe=app.contains(QStringLiteral("/.qttest/"))&&out.startsWith(app+QLatin1Char('/'));QFile result(out);bool wav=safe&&result.open(QIODevice::ReadOnly)&&result.size()>44&&result.read(4)==QByteArray("RIFF");if(result.isOpen())result.close();bool removed=safe&&result.remove();bool imageLinked=exported==retained&&m_sstvRxRawAudioAcquisitionId==image;m_sstvRxRuntime->stop();bool ok=started&&encoded&&fed&&matched&&requested&&saved&&wav&&removed&&imageLinked&&!m_catConnected&&!m_transmitting;return {{"wasShutdown",!ok},{"shutdown",ok},{"started",started},{"encoded",encoded},{"fed",fed},{"matched",matched},{"requested",requested},{"saved",saved},{"wav",wav},{"removed",removed},{"imageLinked",imageLinked},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 

probe='''\n Timer {interval:14000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 for label in ('before','after'):
  instrumented=s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QFile>\n#include <QFileInfo>\n#include <QStandardPaths>\n#include <QEventLoop>\n#include <QTimer>\n#include <QElapsedTimer>\n#include <QThread>\n#include "src/sstv/analog/SstvMartinM1.h"',1).replace(anchor,anchor+inj,1)
  if label=='before':
   main.write_text(instrumented+fixture);mod.write_text(head)
  else:main.write_text(instrumented);mod.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='SstvRawImage'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   wavpath=tmp+'/synthetic.wav'
   with wave.open(wavpath,'wb') as wav: wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(12000);wav.writeframes(bytes(24000))
   env=os.environ.copy();env.update(LAB_WAV_PATH=wavpath,DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   sockets=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sockets.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','40000']
   result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=55)
   rows=[line.split('[DISPLAY-FREQ-LAB] ',1)[1] for line in result.stdout.splitlines() if '[DISPLAY-FREQ-LAB] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-8000:])
   val=json.loads(rows[0]);assert not val['wasShutdown'] and val['shutdown'] and not val['cat'] and not val['tx'],val
   readProbe="\n Timer {interval:9000;running:true;repeat:false;onTriggered:{console.log('[SHUTDOWN-READ] '+String(bridge.getSetting('ShutdownSynthetic','missing')));Qt.quit();}}\n"
   qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+readProbe,1))
   reopened=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=55)
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
