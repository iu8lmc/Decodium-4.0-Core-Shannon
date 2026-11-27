#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile,wave
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeFt8Ready.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/ft8_ready_before.inc').read_text();mark='void DecodiumBridge::onFt8DecodeReady(';head=n.split(mark,1)[0];assert n==head+fixture
mirror='bool const legacyUiMirrorActive =\n        usingLegacyBackendForTx() && decodeMode != QStringLiteral("FT8");';assert fixture.count(mirror)==1
m_test=m
timesync=r/'src/bridge/DecodiumBridgeTimeSyncDispatch.cpp';timesyncOriginal=timesync.read_text();timesyncTest=timesyncOriginal.replace("if (usingLegacyBackendForRx()) {", "if (false) {",1);assert timesyncTest!=timesyncOriginal
fixture_test=fixture.replace(mirror,'bool const legacyUiMirrorActive = qEnvironmentVariableIntValue("DECODIUM_LAB_USE_LEGACY_MIRROR") != 0;',1)

anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
    {Q_UNUSED(values);QString err;auto wave=buildTxWaveformForMessage(QStringLiteral("JT4"),QStringLiteral("CQ K1ABC FN42"),1500,60.0,&err);if(wave.isEmpty())return {{"error",err}};
    QVector<short> pcm(720000,0);for(int i=0;i<wave.size()/4 && i+12000<pcm.size();++i)pcm[i+12000]=qBound(-32767,qRound(wave[i*4]*24000.0f),32767);
    m_sstvWorkspaceActive=false;m_mode=QStringLiteral("JT4");m_monitoring=true;m_transmitting=false;m_tuning=false;m_rxAudioSuspendedForTx=false;
    ensureDecodeWorkerForMode(m_mode);if(m_legacyJtWorker)connect(m_legacyJtWorker,&decodium::legacyjt::LegacyJtDecodeWorker::decodeReady,this,[](quint64 serial,QStringList rows){qWarning().noquote()<<"[JT4-RAW-AB]"<<serial<<rows.join("|");});
    m_decodeSerial=1000;m_forcedDecodeAudioSnapshot=pcm;m_forcedDecodeAudioSnapshotActive=true;feedAudioToDecoder(correctedUtcEpochMs()/60000);return {{"submitted",true},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{bridge.mode="JT4";bridge.filtersBypassed=true;var a=bridge.logSstvQso({path:__PCM__});console.log("[FT8-DISPATCH-START] "+JSON.stringify(a));waitDecode.start();}}
 Timer {id:waitDecode;interval:250;repeat:true;running:false;property int ticks:0;onTriggered:{var rows=bridge.decodeList;for(var i=0;i<rows.length;i++){if(String(rows[i].message).indexOf("CQ K1ABC FN42")>=0){console.log("[AUDIO-SINK-TAPS] "+JSON.stringify({decoded:true,tx:bridge.transmitting}));stop();Qt.quit();return;}}if(++ticks>=40){console.log("[AUDIO-SINK-TAPS] timeout");stop();Qt.quit()}}}
'''


rigs=[];values=[];pcm_paths=[]
try:
 timesync.write_text(timesyncTest)
 sstv.write_text(s.replace('#include <QTimer>','#include "Detector/LegacyJtDecodeWorker.hpp"\n#include "DecodiumBridgeTxWave.h"\n#include <QFile>\n#include <cstring>\n#include <QTimer>',1).replace('#include <QTimer>','#include <QTimer>\n#include <QEventLoop>',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_UTC_TIMER_INSERTION_POINT';assert m.count(point)==1;main.write_text(m_test.replace(point,'void decodiumBridgeAppendDecodeTextBatchesAsync(QString,QString,QString,QString);\nQString decodiumBridgeDedupKey(const QString&,const QString&,const QString&);\nQString decodiumBridgeMirrorSemanticKey(const QVariantMap&);\nint decodiumBridgeAdaptiveInteractiveFtThreadCount(int);\n#define appendDecodeTextBatchesAsync decodiumBridgeAppendDecodeTextBatchesAsync\n#define decodeDedupKey decodiumBridgeDedupKey\n#define decodeMirrorSemanticKey decodiumBridgeMirrorSemanticKey\n#define adaptiveInteractiveFtThreadCount decodiumBridgeAdaptiveInteractiveFtThreadCount\nstatic constexpr int kNativeDecodeDedupFreqToleranceHz = 5;\n'+fixture_test+'\n#undef appendDecodeTextBatchesAsync\n#undef decodeDedupKey\n#undef decodeMirrorSemanticKey\n#undef adaptiveInteractiveFtThreadCount\n'+point));module.write_text(head.replace('static constexpr int kNativeDecodeDedupFreqToleranceHz','[[maybe_unused]] static constexpr int kNativeDecodeDedupFreqToleranceHz'))
  else:main.write_text(m_test);module.write_text(head+fixture_test)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe.replace('__PCM__',json.dumps('')),1));rig='Ft8ReadyJt4'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-feeddecoder-jt4-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','70000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
   rows=[line.split('[AUDIO-SINK-TAPS] ',1)[1] for line in result.stdout.splitlines() if '[AUDIO-SINK-TAPS] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-12000:])
   raw=[line for line in result.stdout.splitlines() if "[JT4-RAW-AB]" in line];callbacks=[line for line in result.stdout.splitlines() if "[JT4-CALLBACK-AB]" in line];print(label,"raw",raw,"callbacks",callbacks,flush=True)
   if not raw:raise RuntimeError('JT4 worker did not return: '+result.stdout[-4000:])
   expected=any("CQ K1ABC FN42" in line for line in raw)
   value={'expectedMessage':expected,'displayedExpected':rows[0]!='timeout'};values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
 if not all(value['expectedMessage'] and value['displayedExpected'] for value in values):raise RuntimeError('JT4 expected message was not decoded and displayed')
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q);timesync.write_text(timesyncOriginal)
 for path in pcm_paths:Path(path).unlink(missing_ok=True)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
