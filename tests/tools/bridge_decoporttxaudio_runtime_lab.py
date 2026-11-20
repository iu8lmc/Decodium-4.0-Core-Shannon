#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];outFile=r/'src/rtl/RtlSdrAudioOutput.cpp';outOrig=outFile.read_text();outNeedle='void RtlSdrAudioOutput::enqueueSamples(const QVector<short>& samples, int sampleRate)\n{';assert outOrig.count(outNeedle)==1;main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeDecoPortTxAudio.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/decoporttxaudio_before.inc').read_text();mark='void DecodiumBridge::decoPortPlayTxAudio(';head=n.split(mark,1)[0];tail='#undef bridgeLog\n';assert n==head+fixture+tail;head+=tail
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);QVector<short> samples{0,4000,-4000};decoPortPlayTxAudio(samples,true);bool sourceBlocked=!m_decoPortTxOutThread;m_transmitting=true;decoPortPlayTxAudio(samples,false);bool txBlocked=!m_decoPortTxOutThread;m_transmitting=false;m_tuning=true;decoPortPlayTxAudio(samples,false);bool tuneBlocked=!m_decoPortTxOutThread;m_tuning=false;decoPortPlayTxAudio({},false);bool emptyBlocked=!m_decoPortTxOutThread;if(!m_decoPortTxGuard){m_decoPortTxGuard=new QTimer(this);m_decoPortTxGuard->setSingleShot(true);m_decoPortTxGuard->setInterval(3000);}m_decoPortRemoteKeyed=true;m_decoPortTxOutRate=48000;decoPortPlayTxAudio(samples,false);bool guard=m_decoPortTxGuard->isActive();m_decoPortTxOutRate=12000;decoPortPlayTxAudio(QVector<short>{100,-100},false);QEventLoop loop;QTimer::singleShot(300,&loop,&QEventLoop::quit);loop.exec();m_decoPortTxGuard->stop();m_decoPortRemoteKeyed=false;return {{"sourceBlocked",sourceBlocked},{"txBlocked",txBlocked},{"tuneBlocked",tuneBlocked},{"emptyBlocked",emptyBlocked},{"guard",guard},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[DECOPORT-TXAUDIO] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 outFile.write_text(outOrig.replace(outNeedle,outNeedle+'\n    QStringList parts; for(short value : samples) parts << QString::number(value); qInfo().noquote() << "[SIM-PCM]" << sampleRate << parts.join(","); return;',1));sstv.write_text('#include <QEventLoop>\n'+s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='DecoPortTxAudio'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-decoporttxaudio-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[DECOPORT-TXAUDIO] ',1)[1] for line in result.stdout.splitlines() if '[DECOPORT-TXAUDIO] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value['sourceBlocked'] and value['txBlocked'] and value['tuneBlocked'] and value['emptyBlocked'] and value['guard'] and not value['cat'] and not value['tx'],value;pcm=[line.split('[SIM-PCM] ',1)[1] for line in result.stdout.splitlines() if '[SIM-PCM] ' in line];assert pcm==['48000 0,1000,2000,3000,4000,2000,0,-2000,-4000,-4000,-4000,-4000','12000 100,-100'],pcm;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 outFile.write_text(outOrig);main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
