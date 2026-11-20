#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile,wave
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeFt8Early.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/ft8early_before.inc').read_text();mark='void DecodiumBridge::maybeDispatchFt8EarlyDecode(';head=n.split(mark,1)[0];assert n==head+fixture
# Force the native dispatch branch in this isolated test on a legacy RX build.
fixture_test=fixture.replace("|| usingLegacyBackendForTx()", "|| false", 1);assert fixture_test!=fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
    {QFile file(values.value("path").toString());if(!file.open(QIODevice::ReadOnly))return {{"error","open"}};QByteArray raw=file.readAll();if(raw.size()!=360000)return {{"error","size"}};
    QVector<short> pcm(raw.size()/2);memcpy(pcm.data(),raw.constData(),raw.size());
    m_sstvWorkspaceActive=false;m_mode=QStringLiteral("FT8");m_monitoring=true;m_transmitting=false;m_tuning=false;m_rxAudioSuspendedForTx=false;
    m_pendingTimeSyncDecodeActive=false;m_audioBuffer=pcm;const auto session=++m_periodTimerSessionId;
    Q_UNUSED(session);m_monitoringSince=QDateTime();m_ft8StartupEarlyDecodeGuardUntilMs=0;m_cpuPressureUntilMs=0;m_cpuPressureSevereUntilMs=0;m_cpuPressureEventCount=0;m_lowCpuModeEnabled=false;ensureDecodeWorkerForMode(m_mode);const qint64 slot=correctedUtcEpochMs()/15000;maybeDispatchFt8EarlyDecode(slot,11000,15000);
    return {{"submitted",true},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{bridge.filtersBypassed=true;var a=bridge.logSstvQso({path:__PCM__});console.log("[FT8-DISPATCH-START] "+JSON.stringify(a));waitDecode.start();}}
 Timer {id:waitDecode;interval:250;repeat:true;running:false;property int ticks:0;onTriggered:{var rows=bridge.decodeList;for(var i=0;i<rows.length;i++){if(String(rows[i].message).indexOf("CQ K1ABC FN42")>=0){console.log("[AUDIO-SINK-TAPS] "+JSON.stringify({decoded:true,tx:bridge.transmitting}));stop();Qt.quit();return;}}if(++ticks>=100){console.log("[AUDIO-SINK-TAPS] timeout");stop();Qt.quit()}}}
'''

rigs=[];values=[];pcm_paths=[]
try:
 sstv.write_text(s.replace('#include <QTimer>','#include <QFile>\n#include <cstring>\n#include <QTimer>',1).replace('#include <QTimer>','#include <QTimer>\n#include <QEventLoop>',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_UTC_TIMER_INSERTION_POINT';assert m.count(point)==1;main.write_text(m.replace(point,fixture_test+point));module.write_text(head)
  else:main.write_text(m);module.write_text(head+fixture_test)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);rawtmp=tempfile.NamedTemporaryFile(prefix='decodium-ft8-pcm-',delete=False);
  with wave.open(str(r/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'),'rb') as wav:rawtmp.write(wav.readframes(wav.getnframes()))
  rawtmp.close(); pcm_paths.append(rawtmp.name);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe.replace('__PCM__',json.dumps(rawtmp.name)),1));rig='Ft8EarlyDecode'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-ft8earlydecode-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','45000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=65)
   rows=[line.split('[AUDIO-SINK-TAPS] ',1)[1] for line in result.stdout.splitlines() if '[AUDIO-SINK-TAPS] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value=={'decoded':True,'tx':False},value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for path in pcm_paths:Path(path).unlink(missing_ok=True)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
