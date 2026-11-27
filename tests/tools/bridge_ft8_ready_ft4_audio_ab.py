#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile,wave
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeFt8Ready.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/ft8_ready_before.inc').read_text();mark='void DecodiumBridge::onFt8DecodeReady(';head=n.split(mark,1)[0];assert n==head+fixture
callback=r/'src/bridge/DecodiumBridgeLegacyJtCallbacks.cpp';callbackOriginal=callback.read_text();callbackAnchor='void DecodiumBridge::onFt4DecodeReady(quint64 serial, QStringList rows)\n{';assert callbackOriginal.count(callbackAnchor)==1
mirror='bool const legacyUiMirrorActive =\n        usingLegacyBackendForTx() && decodeMode != QStringLiteral("FT8");';assert fixture.count(mirror)==1
m_test=m
timesync=r/'src/bridge/DecodiumBridgeTimeSyncDispatch.cpp';timesyncOriginal=timesync.read_text();timesyncTest=timesyncOriginal.replace("if (usingLegacyBackendForRx()) {", "if (false) {",1);assert timesyncTest!=timesyncOriginal
fixture_test=fixture.replace(mirror,'bool const legacyUiMirrorActive = qEnvironmentVariableIntValue("DECODIUM_LAB_USE_LEGACY_MIRROR") != 0;',1)

anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
    {QFile file(values.value("path").toString());if(!file.open(QIODevice::ReadOnly))return {{"error","open"}};QByteArray raw=file.readAll();if(raw.size()!=145152)return {{"error","size"}};
    QVector<short> pcm(raw.size()/2);memcpy(pcm.data(),raw.constData(),raw.size());
    m_sstvWorkspaceActive=false;m_mode=QStringLiteral("FT4");m_monitoring=true;m_transmitting=false;m_tuning=false;m_rxAudioSuspendedForTx=false;
    ensureDecodeWorkerForMode(m_mode);if(m_ft4Worker)connect(m_ft4Worker,&decodium::ft4::FT4DecodeWorker::decodeReady,this,[](quint64 serial,QStringList rows){qWarning().noquote()<<"[FT4-RAW-AB]"<<serial<<rows.join("|");});m_decodeSerial=1000;m_pendingTimeSyncDecodeActive=false;m_audioBuffer=pcm;const auto session=++m_periodTimerSessionId;
    const qint64 slot=correctedUtcEpochMs()/7500-1;dispatchTimeSyncDecodeWhenReady(slot,m_mode,session,0);
    return {{"submitted",true},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{bridge.mode="FT4";bridge.filtersBypassed=true;var a=bridge.logSstvQso({path:__PCM__});console.log("[FT8-DISPATCH-START] "+JSON.stringify(a));waitDecode.start();}}
 Timer {id:waitDecode;interval:250;repeat:true;running:false;property int ticks:0;onTriggered:{var rows=bridge.decodeList;for(var i=0;i<rows.length;i++){if(String(rows[i].message).indexOf("CQ K1ABC FN42")>=0){console.log("[AUDIO-SINK-TAPS] "+JSON.stringify({decoded:true,tx:bridge.transmitting}));stop();Qt.quit();return;}}if(++ticks>=24){console.log("[AUDIO-SINK-TAPS] timeout");stop();Qt.quit()}}}
'''

ft4tmp=tempfile.NamedTemporaryFile(prefix='decodium-ft4-fixture-',suffix='.wav',delete=False);ft4tmp.close();ft4wav=Path(ft4tmp.name)
subprocess.run([str(r/'build/tests/ft4_make_test_wav'),'--message','CQ K1ABC FN42',str(ft4wav)],check=True,stdout=subprocess.DEVNULL)
rigs=[];values=[];pcm_paths=[]
try:
 callback.write_text(callbackOriginal.replace(callbackAnchor,callbackAnchor+'\n    qWarning().noquote()<<"[FT4-CALLBACK-AB]"<<serial<<rows.size()<<m_mode<<m_decodeModeBySerial.value(serial)<<m_decodeSessionBySerial.value(serial,0)<<m_decodeSessionId<<m_sstvWorkspaceActive;',1))
 timesync.write_text(timesyncTest)
 sstv.write_text(s.replace('#include <QTimer>','#include "Detector/FT4DecodeWorker.hpp"\n#include <QFile>\n#include <cstring>\n#include <QTimer>',1).replace('#include <QTimer>','#include <QTimer>\n#include <QEventLoop>',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_UTC_TIMER_INSERTION_POINT';assert m.count(point)==1;main.write_text(m_test.replace(point,'void decodiumBridgeAppendDecodeTextBatchesAsync(QString,QString,QString,QString);\nQString decodiumBridgeDedupKey(const QString&,const QString&,const QString&);\nQString decodiumBridgeMirrorSemanticKey(const QVariantMap&);\nint decodiumBridgeAdaptiveInteractiveFtThreadCount(int);\n#define appendDecodeTextBatchesAsync decodiumBridgeAppendDecodeTextBatchesAsync\n#define decodeDedupKey decodiumBridgeDedupKey\n#define decodeMirrorSemanticKey decodiumBridgeMirrorSemanticKey\n#define adaptiveInteractiveFtThreadCount decodiumBridgeAdaptiveInteractiveFtThreadCount\nstatic constexpr int kNativeDecodeDedupFreqToleranceHz = 5;\n'+fixture_test+'\n#undef appendDecodeTextBatchesAsync\n#undef decodeDedupKey\n#undef decodeMirrorSemanticKey\n#undef adaptiveInteractiveFtThreadCount\n'+point));module.write_text(head.replace('static constexpr int kNativeDecodeDedupFreqToleranceHz','[[maybe_unused]] static constexpr int kNativeDecodeDedupFreqToleranceHz'))
  else:main.write_text(m_test);module.write_text(head+fixture_test)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);rawtmp=tempfile.NamedTemporaryFile(prefix='decodium-ft8-pcm-',delete=False);
  with wave.open(str(ft4wav),'rb') as wav:rawtmp.write(wav.readframes(wav.getnframes()))
  rawtmp.close(); pcm_paths.append(rawtmp.name);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe.replace('__PCM__',json.dumps(rawtmp.name)),1));rig='Ft8ReadyFt4'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-feeddecoder-ft4-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','45000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=65)
   rows=[line.split('[AUDIO-SINK-TAPS] ',1)[1] for line in result.stdout.splitlines() if '[AUDIO-SINK-TAPS] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-12000:])
   raw=[line for line in result.stdout.splitlines() if "[FT4-RAW-AB]" in line];callbacks=[line for line in result.stdout.splitlines() if "[FT4-CALLBACK-AB]" in line];print(label,"raw",raw,"callbacks",callbacks,flush=True)
   assert any("CQ K1ABC FN42" in line for line in raw),result.stdout[-12000:]
   value={'workerDecoded':True,'displayed':rows[0]!='timeout'};assert value['displayed'],result.stdout[-12000:]
   if value['displayed']:assert json.loads(rows[0])=={'decoded':True,'tx':False}
   values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q);timesync.write_text(timesyncOriginal);callback.write_text(callbackOriginal)
 ft4wav.unlink(missing_ok=True)
 for path in pcm_paths:Path(path).unlink(missing_ok=True)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
