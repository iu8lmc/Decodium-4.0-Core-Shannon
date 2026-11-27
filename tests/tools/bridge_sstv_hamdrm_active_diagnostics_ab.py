#!/usr/bin/env python3
"""Compare core timer initialization in isolated apps."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeSstv.cpp'; mod=r/'src/bridge/DecodiumBridgeSstvRxDiagnostics.cpp'; sstv=main; qml=r/'build/qml/decodium/Main.qml'
m,n,q=(p.read_text() for p in (main,mod,qml));s=m;fixture=(r/'tests/bridge_refactor_fixtures/sstv_rx_diagnostics_before.inc').read_text();assert n.endswith(fixture);head=n[:-len(fixture)]
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);initialiseSstvRuntime();if(!m_sstvRxRuntime)return {{"error","runtime"}};bool digital=false;
#if DECODIUM_HAS_HAMDRM
 using namespace decodium::sstv::hamdrm;using namespace decodium::sstv::hamdrm::waveform;struct LabSink final:HamDrmWaveformRxSink{void hamDrmRxProgress(std::uint64_t,double)override{}void hamDrmRxMotGroup(std::uint64_t,QByteArray)override{}void hamDrmRxFinished(std::uint64_t,HamDrmStatus)override{}} sink;auto previous=m_hamDrmRxBackend;HamDrmNativeRxHooks hooks;hooks.activateSharedAudioTap=[](){return HamDrmStatus::success();};hooks.deactivateSharedAudioTap=[](){};m_hamDrmRxBackend=std::make_shared<HamDrmNativeRxBackend>(std::move(hooks));auto* profile=HamDrmProfileRegistry::findById("hamdrm-A-2500-normal-64-QAM-short");bool active=profile&&m_hamDrmRxBackend->start(*profile,123,sink).ok();std::vector<std::int16_t> samples(1000,0);bool acceptedDigital=active&&m_hamDrmRxBackend->submitPcm16(samples.data(),samples.size(),12000);m_hamDrmRxRequested=true;m_hamDrmRxSourceKind=3;m_hamDrmRxStreamId=456;auto now=sstvRxDiagnostics();digital=acceptedDigital&&now.value("hamdrmActive").toBool()&&now.value("hamdrmRequested").toBool()&&now.value("hamdrmStreamId").toUInt()==456&&now.value("hamdrmAcceptedChunks").toULongLong()==1&&now.value("hamdrmAcceptedSamples").toULongLong()==1000;m_hamDrmRxBackend->cancel(123);m_hamDrmRxBackend->shutdown();m_hamDrmRxBackend=previous;m_hamDrmRxRequested=false;m_hamDrmRxSourceKind=0;m_hamDrmRxStreamId=0;
#endif
 auto initial=sstvRxDiagnostics();bool idle=initial.value("available").toBool()&&!initial.value("requested").toBool();bool started=m_sstvRxRuntime->start(decodium::sstv::SstvAudioSourceKind::Replay,123);auto token=m_sstvRxRuntime->routeToken();QVector<short> audio(4000,1000);bool accepted=m_sstvRxRuntime->enqueuePcm16At(audio,12000,token,0);QElapsedTimer wait;wait.start();while(m_sstvRxRuntime->snapshot().chunksProcessed<1&&wait.elapsed()<5000){QCoreApplication::processEvents();QThread::msleep(2);}auto running=sstvRxDiagnostics();bool retained=running.value("replayRetainedSamples").toULongLong()>0;bool source=running.value("streamId").toUInt()==123;bool stop=m_sstvRxRuntime->stop();bool ok=digital&&idle&&started&&accepted&&retained&&source&&stop&&!m_catConnected&&!m_transmitting;return {{"wasShutdown",!ok},{"shutdown",ok},{"digital",digital},{"idle",idle},{"started",started},{"accepted",accepted},{"retained",retained},{"source",source},{"stopped",stop},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 

probe='''\n Timer {interval:14000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 for label in ('before','after'):
  instrumented=s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QFile>\n#include <QFileInfo>\n#include <QElapsedTimer>\n#include <QThread>',1).replace(anchor,anchor+inj,1)
  if label=='before':
   main.write_text(instrumented.replace('void DecodiumBridge::initialiseSstvRuntime()',fixture+'void DecodiumBridge::initialiseSstvRuntime()',1));mod.write_text(head)
  else:main.write_text(instrumented);mod.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='SstvHamdrmActiveDiagnostics'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   env=os.environ.copy();env.update(LAB_ADIF_PATH=tmp+'/synthetic.adi',DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
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
 for p,content in ((main,m),(mod,n),(qml,q)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
