#!/usr/bin/env python3
"""Compare real SSTV producer reconnection after WAV replay cancellation."""
import json, os, secrets, shutil, socket, subprocess, tempfile, wave
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeSstv.cpp'; mod=r/'src/bridge/DecodiumBridgeSstvWavReplay.cpp'; sstv=main; qml=r/'build/qml/decodium/Main.qml'
m,n,q=(p.read_text() for p in (main,mod,qml));s=m;fixture=(r/'tests/bridge_refactor_fixtures/sstv_wav_replay_before.inc').read_text();assert n.endswith(fixture);head=n[:-len(fixture)];assert "refreshSstvProducerTaps();" in fixture
labFixture=fixture;labModule=head+fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);ensureAudioSink();initialiseSstvRuntime();if(!m_sstvWavReplayController||!m_sstvRxRuntime)return {{"error","runtime"}};m_sstvRxRequested=true;auto kind=currentSstvAudioSourceKind();quint32 stream=currentSstvAudioStreamId(kind);bool live=m_sstvRxRuntime->start(kind,stream);bool finished=false,completed=false,cancelled=false;QEventLoop loop;QTimer::singleShot(10000,&loop,&QEventLoop::quit);auto c=connect(m_sstvWavReplayController,&decodium::sstv::SstvWavReplayController::replayFinished,&loop,[&](bool ok,bool cancel,quint64){finished=true;completed=ok;cancelled=cancel;loop.quit();});bool started=startSstvWavReplay(QUrl::fromLocalFile(qEnvironmentVariable("LAB_WAV_PATH")));if(started){cancelSstvWavReplay();loop.exec();}disconnect(c);QCoreApplication::processEvents();auto route=m_sstvRxRuntime->routeToken();bool restored=route.valid()&&route.source.kind==kind&&route.source.streamId==stream&&m_sstvRxRuntime->isRunning()&&(bool(m_sstvLocalAudioTap)||bool(m_sstvLegacyAudioTap)||bool(m_sstvTciAudioTap)||bool(m_sstvDecoPortAudioTap)||bool(m_sstvRtlPcmTap)||bool(m_sstvRtlAudioTap));bool tapBefore=bool(m_sstvLocalAudioTap);bool tapAfter=tapBefore;auto samplesBefore=m_sstvRxRuntime->snapshot().ingress.acceptedSamples;m_audioSink->injectExternalSamples(QVector<short>(1024,1234));QEventLoop sampleLoop;QTimer::singleShot(200,&sampleLoop,&QEventLoop::quit);sampleLoop.exec();auto samplesAfter=m_sstvRxRuntime->snapshot().ingress.acceptedSamples;bool pcmDelivered=samplesAfter>=samplesBefore+1024;bool clear=m_sstvWavReplaySession==0&&!m_sstvWavReplayRestoreLive;bool ok=live&&started&&finished&&cancelled&&!completed&&restored&&pcmDelivered&&clear&&!m_catConnected&&!m_transmitting;return {{"pcmDelivered",pcmDelivered},{"sampleDelta",static_cast<qulonglong>(samplesAfter-samplesBefore)},{"tapBefore",tapBefore},{"tapAfter",tapAfter},{"state",int(m_sstvRxRuntime->state())},{"active",sstvWavReplayActive()},{"requested",m_sstvRxRequested},{"kind",int(kind)},{"sink",bool(m_audioSink)},{"legacy",bool(m_legacyBackend)},{"localTap",bool(m_sstvLocalAudioTap)},{"legacyTap",bool(m_sstvLegacyAudioTap)},{"wasShutdown",!ok},{"shutdown",ok},{"live",live},{"started",started},{"finished",finished},{"cancelled",cancelled},{"restored",restored},{"clear",clear},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 

probe='''\n Timer {interval:14000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 for label in ('before','after'):
  instrumented=s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include "DecodiumAudioSink.h"\n#include <QFile>\n#include <QFileInfo>',1).replace(anchor,anchor+inj,1)
  if label=='before':
   main.write_text(instrumented+labFixture);mod.write_text(head)
  else:main.write_text(instrumented);mod.write_text(labModule)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='SstvReplayCancel'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   wavpath=tmp+'/synthetic.wav'
   with wave.open(wavpath,'wb') as wav: wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(12000);wav.writeframes(bytes(24000*30))
   env=os.environ.copy();env.update(LAB_WAV_PATH=wavpath,DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   sockets=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sockets.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','40000']
   result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=55)
   rows=[line.split('[DISPLAY-FREQ-LAB] ',1)[1] for line in result.stdout.splitlines() if '[DISPLAY-FREQ-LAB] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-8000:])
   val=json.loads(rows[0]);assert not val['cat'] and not val['tx'],val
   readProbe="\n Timer {interval:9000;running:true;repeat:false;onTriggered:{console.log('[SHUTDOWN-READ] '+String(bridge.getSetting('ShutdownSynthetic','missing')));Qt.quit();}}\n"
   qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+readProbe,1))
   reopened=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=55)
   persisted=[line.split('[SHUTDOWN-READ] ',1)[1] for line in reopened.stdout.splitlines() if '[SHUTDOWN-READ] ' in line]
   if reopened.returncode or persisted!=['persisted']:raise RuntimeError('persistence readback failed: '+reopened.stdout[-8000:])
   val['readback']=persisted[0];results.append(val);print(label,json.dumps(val,sort_keys=True),flush=True)
 assert results[0]==results[1], results
 assert all(item['shutdown'] for item in results), results
finally:
 for p,content in ((main,m),(mod,n),(qml,q)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
