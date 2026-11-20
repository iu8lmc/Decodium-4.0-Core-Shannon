#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile,wave
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeFt2AsyncTimer.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/ft2_async_timer_before.inc').read_text();mark='void DecodiumBridge::onAsyncDecodeTimer()';head=n.split(mark,1)[0];assert n==head+fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {if(values.value("action").toString()==QStringLiteral("read"))return {{"mode",m_mode},{"monitor",m_monitoring},{"pending",m_asyncDecodePending},{"pos",static_cast<qulonglong>(m_asyncAudioPos.load())},{"counter",m_ft2NarrowPassCounter},{"done",property("labDone").toInt()},{"decoded",property("labDecoded").toInt()},{"cat",m_catConnected},{"tx",m_transmitting}};QFile f(values.value("path").toString());if(!f.open(QIODevice::ReadOnly))return {{"error","open"}};QByteArray raw=f.readAll();if(raw.size()!=90000)return {{"error","size"}};if(m_legacyStateTimer)m_legacyStateTimer->stop();if(m_periodTimer)m_periodTimer->stop();if(m_asyncDecodeTimer)m_asyncDecodeTimer->stop();m_txEnabled=false;m_transmitting=false;m_tuning=false;m_autoCqRepeat=false;m_mode=QStringLiteral("FT2");m_monitoring=true;m_lowCpuModeEnabled=false;m_cpuPressureUntilMs=0;m_cpuPressureSevereUntilMs=0;m_ft2AdaptiveDecode=false;m_ft2NarrowAsyncDecode=true;m_ft2NarrowPassCounter=0;m_dxCall=QStringLiteral("W1XYZ");m_nfa=200;m_nfb=4000;m_rxFrequency=1000;m_qsoPartnerFreqHz=1000;m_qsoPartnerFreqMs=QDateTime::currentMSecsSinceEpoch();m_ft2AsyncDeadlineMs=0;memcpy(m_asyncAudio,raw.constData(),raw.size());memcpy(m_asyncAudio+45000,raw.constData(),raw.size());ensureDecodeWorkerForMode(m_mode);if(!m_ft2Worker)return {{"error","worker"}};setProperty("labDone",0);setProperty("labDecoded",0);connect(m_ft2Worker,&decodium::ft2::FT2DecodeWorker::asyncDecodeReady,this,[this](QStringList rows){int done=property("labDone").toInt()+1;setProperty("labDone",done);for(const auto &row:rows)if(row.contains(QStringLiteral("CQ K1ABC FN42")))setProperty("labDecoded",property("labDecoded").toInt()+1);if(done<4)QTimer::singleShot(0,this,[this,done](){m_cpuPressureUntilMs=0;m_cpuPressureSevereUntilMs=0;m_asyncAudioPos.store(45000+done);m_lastFt2AsyncDecodeDispatchMs=0;onAsyncDecodeTimer();});});m_asyncDecodePending=false;m_ft2AsyncLastDispatchAudioPos=0;m_lastFt2AsyncDecodeDispatchMs=0;m_asyncAudioPos.store(45000);onAsyncDecodeTimer();return {{"started",m_asyncDecodePending}};}
#endif
'''
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";bridge.filtersBypassed=true;bridge.logSstvQso({action:"start",path:__PCM__});wait.start();}}
 Timer {id:wait;interval:250;repeat:true;property int ticks:0;onTriggered:{var v=bridge.logSstvQso({action:"read"});if(v.done>=4 || ++ticks>=100){console.log("[MODE-CLEANUP] "+JSON.stringify(v));stop();Qt.quit();}}}
'''
tag='    req.nfb   = reqNfb;'
assert fixture.count(tag)==1
instrumented=fixture.replace(tag,tag+'\n    qWarning().noquote() << "[FT2-RANGE-AB]" << req.nfa << req.nfb << req.nfqso;',1)
ft2tmp=tempfile.NamedTemporaryFile(prefix='decodium-ft2-seq-',suffix='.wav',delete=False);ft2tmp.close();ft2wav=Path(ft2tmp.name)
subprocess.run([str(r/'build/tests/ft2_make_test_wav'),'--message','CQ K1ABC FN42',str(ft2wav)],check=True,stdout=subprocess.DEVNULL)
rawtmp=tempfile.NamedTemporaryFile(prefix='decodium-ft2-seq-',delete=False)
with wave.open(str(ft2wav),'rb') as wav:rawtmp.write(wav.readframes(wav.getnframes()))
rawtmp.close()
rigs=[];values=[]
try:
 sstv.write_text(s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include "Detector/FT2DecodeWorker.hpp"\n#include <QFile>\n#include <QTimer>\n#include <cstring>').replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_UTC_TIMER_INSERTION_POINT';assert m.count(point)==1;main.write_text(m.replace(point,instrumented+point));module.write_text(head)
  else:main.write_text(m);module.write_text(head+instrumented)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe.replace('__PCM__',json.dumps(rawtmp.name)),1));rig='Ft2Sequential'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-modecleanup-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','50000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)
   rows=[line.split('[MODE-CLEANUP] ',1)[1] for line in result.stdout.splitlines() if '[MODE-CLEANUP] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value['done']==4 and value['decoded']>=1 and not value['cat'] and not value['tx'],value;ranges=[line.split('[FT2-RANGE-AB] ',1)[1].strip() for line in result.stdout.splitlines() if '[FT2-RANGE-AB] ' in line];assert ranges==['850 1150 1000']*3+['200 4000 1000'],ranges;values.append(value);print(label,json.dumps({'result':value,'ranges':ranges},sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q);ft2wav.unlink(missing_ok=True);Path(rawtmp.name).unlink(missing_ok=True)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
