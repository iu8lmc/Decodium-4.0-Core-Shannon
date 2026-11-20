#!/usr/bin/env python3
"""Compare TX completion with a real BlackHole audio sink in isolated apps."""
import argparse, json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--drained',action='store_true');args=p.parse_args()
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridge.cpp'; mod=r/'src/bridge/DecodiumBridgeCompleteTxPlayback.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,n,s,q=(p.read_text() for p in (main,mod,sstv,qml)); fixture=(r/'tests/bridge_refactor_fixtures/complete_tx_playback_before.inc').read_text(); head=n.split('void DecodiumBridge::completeTxPlayback(const QString& reason, bool error)',1)[0]; assert n==head+fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values); QAudioDevice device; for(const auto& d:QMediaDevices::audioOutputs()) if(d.description()==QStringLiteral("BlackHole 2ch")) device=d;
 if(device.isNull())return {{"error",QStringLiteral("missing-device")}};
 auto fmt=device.preferredFormat(); m_txAudioSink=new QAudioSink(device,fmt,this); m_txPcmBuffer=new QBuffer(this); m_txPcmBuffer->setData(QByteArray(fmt.bytesForDuration(3000000), '\x01')); m_txPcmBuffer->open(QIODevice::ReadOnly); m_txAudioSink->start(m_txPcmBuffer);
 QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec(); bool started=m_txAudioSink->state()!=QAudio::StoppedState;
 m_transmitting=true;m_tuning=false;m_bridgeAudioLegacyTxActive=true;m_ft2LinkTxActive=false;m_txPlaybackHoldUntilMs=0;m_txPlaybackHardDeadlineMs=0;m_txPlaybackReleasePending=false;
 __COMPLETE__ bool cleared=!m_transmitting&&m_txPcmBuffer==nullptr&&m_txAudioSink==nullptr;
 return {{"wasShutdown",!started},{"shutdown",cleared},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
if args.drained:
 inj=inj.replace('fmt.bytesForDuration(3000000)', 'fmt.bytesForDuration(300000)')
 inj=inj.replace('__COMPLETE__', '''QEventLoop drain;QTimer poll;poll.setInterval(20);QObject::connect(&poll,&QTimer::timeout,&drain,[&](){if(m_txPcmBuffer->atEnd())drain.quit();});poll.start();QTimer::singleShot(4000,&drain,&QEventLoop::quit);drain.exec();bool consumed=m_txPcmBuffer->atEnd();if(!consumed)return {{"error",QStringLiteral("not-consumed")}};completeTxPlayback(QStringLiteral("sink-drained"),false);''')
else:
 inj=inj.replace('__COMPLETE__', 'completeTxPlayback(QStringLiteral("sink-error"),true);')

probe='''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 sstv.write_text(s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QBuffer>\n#include <QAudioSink>\n#include <QMediaDevices>\n#include <QEventLoop>',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_COMPLETE_TX_INSERTION_POINT\n'; assert m.count(point)==1; main.write_text(m.replace(point,'#undef bridgeLog\n'+head+'\n'+fixture+'\n#undef bridgeLog\n#undef txTimelineLog\n#undef retireAudioSink\n#undef activeCatCanPtt\n#undef activeCatSetPtt\n'+point));mod.write_text(head)
  else:main.write_text(m);mod.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='CompletePcmProbe'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
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
 for p,content in ((main,m),(mod,n),(sstv,s),(qml,q)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
