#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeAudioSink.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/audiosink_before.inc').read_text();mark='void DecodiumBridge::ensureAudioSink()';head=n.split(mark,1)[0];assert n==head+fixture
ssb=r/'src/bridge/SsbBridge.inc';ssbOriginal=ssb.read_text();ssbAnchor='void DecodiumBridge::ssbMonitorAudio(const QVector<short>& samples)\n{';assert ssbOriginal.count(ssbAnchor)==1
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
    {Q_UNUSED(values);ensureAudioSink();QVector<short> samples(240,1234);
    m_mode=QStringLiteral("SSB");m_decoPortUseRemote=false;emit m_audioSink->audioSamplesReady(samples);
    m_decoPortUseRemote=true;emit m_audioSink->audioSamplesReady(samples);
    m_decoPortUseRemote=false;m_mode=QStringLiteral("FT8");emit m_audioSink->audioSamplesReady(samples);
    return {{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[AUDIO-SINK-TAPS] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 ssb.write_text(ssbOriginal.replace(ssbAnchor,ssbAnchor+'\n    qWarning("[SSB-SINK-AB] invoked samples=%d", int(samples.size()));',1))
 sstv.write_text(s.replace('#include <QTimer>','#include <QTimer>\n#include <QEventLoop>',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_UTC_TIMER_INSERTION_POINT';assert m.count(point)==1;main.write_text(m.replace(point, "bool decodiumBridgeIsFt2LinkApplicationMode(QString);\n#define isFt2LinkApplicationMode decodiumBridgeIsFt2LinkApplicationMode\n"+fixture+"\n#undef isFt2LinkApplicationMode\n"+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='AudioSinkSsb'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-audiosinkssb-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[AUDIO-SINK-TAPS] ',1)[1] for line in result.stdout.splitlines() if '[AUDIO-SINK-TAPS] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert not value['cat'] and not value['tx'],value
   hits=[line for line in result.stdout.splitlines() if '[SSB-SINK-AB] invoked' in line]
   assert len(hits)==1 and 'samples=240' in hits[0],hits
   values.append(value);print(label,'SSB local 240 samples; remote and other mode gated',flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q);ssb.write_text(ssbOriginal)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
