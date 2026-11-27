#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeAudioSink.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/audiosink_before.inc').read_text();mark='void DecodiumBridge::ensureAudioSink()';head=n.split(mark,1)[0];assert n==head+fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
    {Q_UNUSED(values);ensureAudioSink();int cw=0,rtty=0,jtty=0,ft2=0;int ft2samples=0;
    auto c1=connect(this,&DecodiumBridge::campioniRxCw,this,[&](const QVector<short>&v){cw+=v.size();});
    auto c2=connect(this,&DecodiumBridge::campioniRxRtty,this,[&](const QVector<short>&v){rtty+=v.size();});
    auto c3=connect(this,&DecodiumBridge::campioniRxJtty,this,[&](const QVector<short>&v){jtty+=v.size();});
    auto c4=connect(this,&DecodiumBridge::ft2LinkRxSamplesReady,this,[&](QVector<short>v,quint64){++ft2;ft2samples+=v.size();});
    QVector<short> samples(240,1234);m_monitoring=true;m_transmitting=false;m_tuning=false;m_rttyTxActive=false;m_cwInAscolto=true;m_rttyInAscolto=false;m_mode=QStringLiteral("FT8");
    emit m_audioSink->audioSamplesReady(samples);m_transmitting=true;emit m_audioSink->audioSamplesReady(samples);m_transmitting=false;m_cwInAscolto=false;
    m_rttyInAscolto=true;emit m_audioSink->audioSamplesReady(samples);m_tuning=true;emit m_audioSink->audioSamplesReady(samples);m_tuning=false;m_rttyInAscolto=false;
    m_mode=QStringLiteral("JTTY");emit m_audioSink->audioSamplesReady(samples);m_rttyTxActive=true;emit m_audioSink->audioSamplesReady(samples);m_rttyTxActive=false;
    m_mode=QStringLiteral("FT8");QCoreApplication::processEvents();m_mode=QStringLiteral("FT2-LINK");m_monitoring=true;emit m_audioSink->audioSamplesReady(samples);QEventLoop loop;QTimer monitorGuard;connect(&monitorGuard,&QTimer::timeout,&loop,[&](){m_monitoring=true;});monitorGuard.start(5);QString observedMode;bool observedMonitor=false,observedScheduled=false;int observedPending=-1;QTimer::singleShot(100,&loop,[&](){observedMode=m_mode;observedMonitor=m_monitoring;observedScheduled=m_ft2LinkModernRxDrainScheduled;observedPending=m_ft2LinkModernRxPending.size();});QTimer::singleShot(500,&loop,&QEventLoop::quit);loop.exec();
    m_mode=QStringLiteral("FT8");m_monitoring=false;QObject::disconnect(c1);QObject::disconnect(c2);QObject::disconnect(c3);QObject::disconnect(c4);
    return {{"modeAt100",observedMode},{"monitorAt100",observedMonitor},{"scheduledAt100",observedScheduled},{"pendingAt100",observedPending},{"cw",cw},{"rtty",rtty},{"jtty",jtty},{"ft2",ft2},{"ft2samples",ft2samples},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[AUDIO-SINK-TAPS] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text(s.replace('#include <QTimer>','#include <QTimer>\n#include <QEventLoop>',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_UTC_TIMER_INSERTION_POINT';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='AudioSinkTaps'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-audiosinktaps-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[AUDIO-SINK-TAPS] ',1)[1] for line in result.stdout.splitlines() if '[AUDIO-SINK-TAPS] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert all(value[k]==240 for k in ('cw','rtty','jtty')) and value['ft2']==1 and value['ft2samples']==240 and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert all(values[0][k]==values[1][k] for k in ("cw","rtty","jtty","ft2","ft2samples","cat","tx"))
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
