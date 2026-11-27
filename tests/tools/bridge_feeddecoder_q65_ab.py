#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile,wave
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeFeedDecoder.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/feeddecoder_before.inc').read_text();mark='void DecodiumBridge::feedAudioToDecoder(';head=n.split(mark,1)[0];assert n==head+fixture
mirror='bool const legacyUiMirrorActive =\n        usingLegacyBackendForTx() && decodeMode != QStringLiteral("FT8");';ready=r/'src/bridge/DecodiumBridgeFt8Ready.cpp';readyOriginal=ready.read_text();assert readyOriginal.count(mirror)==1
readyTest=readyOriginal.replace(mirror,'bool const legacyUiMirrorActive = qEnvironmentVariableIntValue("DECODIUM_LAB_USE_LEGACY_MIRROR") != 0;',1);m_test=m
timesync=r/'src/bridge/DecodiumBridgeTimeSyncDispatch.cpp';timesyncOriginal=timesync.read_text();timesyncTest=timesyncOriginal.replace("if (usingLegacyBackendForRx()) {", "if (false) {",1);assert timesyncTest!=timesyncOriginal
fixture_test=fixture

anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
    {QFile file(values.value("path").toString());if(!file.open(QIODevice::ReadOnly))return {{"error","open"}};QByteArray raw=file.readAll();if(raw.size()!=360000)return {{"error","size"}};
    QVector<short> pcm(raw.size()/2);memcpy(pcm.data(),raw.constData(),raw.size());
    m_sstvWorkspaceActive=false;m_mode=QStringLiteral("Q65-15A");m_monitoring=true;m_transmitting=false;m_tuning=false;m_rxAudioSuspendedForTx=false;
    ensureDecodeWorkerForMode(m_mode);if(m_q65Worker)connect(m_q65Worker,&decodium::q65::Q65DecodeWorker::decodeReady,this,[](quint64 serial,QStringList rows){qWarning().noquote()<<"[Q65-RAW-AB]"<<serial<<rows.join("|");});m_decodeSerial=1000;m_pendingTimeSyncDecodeActive=false;m_audioBuffer=pcm;const auto session=++m_periodTimerSessionId;
    Q_UNUSED(session);m_forcedDecodeAudioSnapshot=pcm;m_forcedDecodeAudioSnapshotActive=true;const qint64 slot=correctedUtcEpochMs()/15000;feedAudioToDecoder(slot);
    return {{"submitted",true},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{bridge.mode="Q65-15A";bridge.filtersBypassed=true;var a=bridge.logSstvQso({path:__PCM__});console.log("[FT8-DISPATCH-START] "+JSON.stringify(a));waitDecode.start();}}
 Timer {id:waitDecode;interval:250;repeat:true;running:false;property int ticks:0;onTriggered:{var rows=bridge.decodeList;for(var i=0;i<rows.length;i++){if(String(rows[i].message).indexOf("CQ K1ABC FN42")>=0){console.log("[AUDIO-SINK-TAPS] "+JSON.stringify({decoded:true,tx:bridge.transmitting}));stop();Qt.quit();return;}}if(++ticks>=100){console.log("[AUDIO-SINK-TAPS] timeout");stop();Qt.quit()}}}
'''

generator=r/'build/tests/decodium_lab_q65sim'
if not generator.is_file():
 cmake=r/'tests/CMakeLists.txt';original_cmake=cmake.read_text()
 try:
  cmake.write_text(original_cmake+'\nadd_executable(decodium_lab_q65sim ../utils/q65sim.cpp)\ntarget_include_directories(decodium_lab_q65sim PRIVATE ${PROJECT_BINARY_DIR})\ntarget_link_libraries(decodium_lab_q65sim wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)\n')
  subprocess.run(['cmake','--build','build','--target','decodium_lab_q65sim','-j','2'],cwd=r,check=True)
 finally:cmake.write_text(original_cmake)
fixtureDir=tempfile.TemporaryDirectory(prefix='decodium-q65-fixture-');q65wav=Path(fixtureDir.name)/'000000_000000.wav'
subprocess.run([str(r/'build/tests/decodium_lab_q65sim'),'CQ K1ABC FN42','A','1500','0','0','0','1','15','1','1','-10'],cwd=fixtureDir.name,check=True,stdout=subprocess.DEVNULL)

rigs=[];values=[];pcm_paths=[]
try:
 ready.write_text(readyTest)
 timesync.write_text(timesyncTest)
 sstv.write_text(s.replace('#include <QTimer>','#include "Detector/Q65DecodeWorker.hpp"\n#include <QFile>\n#include <cstring>\n#include <QTimer>',1).replace('#include <QTimer>','#include <QTimer>\n#include <QEventLoop>',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   main.write_text(m_test+'\n#undef bridgeLog\n'+head+fixture_test);module.write_text(head)
  else:main.write_text(m_test);module.write_text(head+fixture_test)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True);rawtmp=tempfile.NamedTemporaryFile(prefix='decodium-ft8-pcm-',delete=False);
  with wave.open(str(q65wav),'rb') as wav:rawtmp.write(wav.readframes(wav.getnframes()))
  rawtmp.close(); pcm_paths.append(rawtmp.name);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe.replace('__PCM__',json.dumps(rawtmp.name)),1));rig='FeedDecoderFt4'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-feeddecoder-q65-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','65000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
   rows=[line.split('[AUDIO-SINK-TAPS] ',1)[1] for line in result.stdout.splitlines() if '[AUDIO-SINK-TAPS] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-12000:])
   raw=[line for line in result.stdout.splitlines() if "[Q65-RAW-AB]" in line];callbacks=[line for line in result.stdout.splitlines() if "[Q65-CALLBACK-AB]" in line];print(label,"raw",raw,"callbacks",callbacks,flush=True)
   assert any("CQ K1ABC FN42" in line for line in raw),result.stdout[-12000:]
   value={'workerDecoded':True,'displayed':rows[0]!='timeout'};assert value['displayed'],result.stdout[-12000:]
   if value['displayed']:assert json.loads(rows[0])=={'decoded':True,'tx':False}
   values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 ready.write_text(readyOriginal)
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q);timesync.write_text(timesyncOriginal)
 fixtureDir.cleanup()
 for path in pcm_paths:Path(path).unlink(missing_ok=True)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
