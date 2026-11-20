#!/usr/bin/env python3
"""Compare capture configuration and retune with a simulated RTL input."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeRtlSdrCapture.cpp';rtl=r/'src/rtl/RtlSdrInput.cpp';qml=r/'build/qml/decodium/Main.qml'
m=main.read_text();s=sstv.read_text();n=module.read_text();origRtl=rtl.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/rtlsdrcapture_before.inc').read_text();mark='namespace {\nRtlSdrDsp::Demodulator rtlSdrDemodulatorFromSetting';head=n.split(mark,1)[0];assert mark+n.split(mark,1)[1]==fixture+'#undef bridgeLog\n'
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values);
 setSetting(QStringLiteral("RtlSdrFollowDial"), false);
 setSetting(QStringLiteral("RtlSdrFrequencyHz"), 14074000);
 setSetting(QStringLiteral("RtlSdrDemodulator"), QStringLiteral("usb"));
 setSetting(QStringLiteral("RtlSdrSampleRate"), 240000);
 startRtlSdrCapture(); auto a=m_rtlSdrInput->activeConfig();
 startRtlSdrCapture(); auto b=m_rtlSdrInput->activeConfig();
 setSetting(QStringLiteral("RtlSdrFrequencyHz"), 14075000);
 startRtlSdrCapture(); auto c=m_rtlSdrInput->activeConfig();
 setSetting(QStringLiteral("RtlSdrSampleRate"), 480000);
 startRtlSdrCapture(); auto d=m_rtlSdrInput->activeConfig();
 scheduleRtlSdrRetune(); int timer=m_rtlSdrRetuneTimer->interval(); stopRtlSdrCapture();
 QString status; auto connection=QObject::connect(this,&DecodiumBridge::statusMessage,this,[&](const QString& message){status=message;});
 setSetting(QStringLiteral("RtlSdrEnabled"), true); m_monitoring=true; m_transmitting=true;
 applyRtlSdrSettings(QStringLiteral("synthetic")); bool deferred=status.contains(QStringLiteral("after TX/Tune"));
 m_transmitting=false; m_monitoring=false; QObject::disconnect(connection); setSetting(QStringLiteral("RtlSdrEnabled"), false);
 return {{"aRate",a.sampleRate},{"aCenter",a.centerFrequencyHz},{"aOffset",a.channelOffsetHz},
 {"demod",static_cast<int>(a.demodulator)},{"unchanged",a.centerFrequencyHz==b.centerFrequencyHz},
 {"retuned",c.centerFrequencyHz!=a.centerFrequencyHz || c.channelOffsetHz!=a.channelOffsetHz},
 {"newRate",d.sampleRate},{"deferred",deferred},{"timer",timer},{"running",m_rtlSdrInput->isRunning()},
 {"cat",m_catConnected},{"tx",m_transmitting}}; }
#endif
'''
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{console.log('[RTL-CAPTURE] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 modified=origRtl
 for anchor2,insert in [ ('bool RtlSdrInput::retune(quint32 centerFrequencyHz, int channelOffsetHz)\n{','\n    m_activeConfig.centerFrequencyHz=centerFrequencyHz; m_activeConfig.channelOffsetHz=channelOffsetHz; qInfo("[RTL-SIM] retune"); return true;'),('void RtlSdrInput::start(const Config& config)\n{','\n    m_activeConfig=config; m_running=true; qInfo("[RTL-SIM] start"); return;'),('void RtlSdrInput::stop()\n{','\n    m_running=false; qInfo("[RTL-SIM] stop"); return;')]:
  assert modified.count(anchor2)==1;modified=modified.replace(anchor2,anchor2+insert,1)
 rtl.write_text(modified);sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_PALETTE_INSERTION_POINT';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='RtlCapture'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-rtlcapture-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[x.split('[RTL-CAPTURE] ',1)[1] for x in result.stdout.splitlines() if '[RTL-CAPTURE] ' in x]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);counts={kind:result.stdout.count('[RTL-SIM] '+kind) for kind in ('start','retune')};assert value['aRate']==240000 and value['newRate']==480000 and value['unchanged'] and value['retuned'] and value['deferred'] and value['timer']==250 and not value['running'] and not value['cat'] and not value['tx'] and counts=={'start':2,'retune':1},(value,counts);values.append((value,counts));print(label,json.dumps(value,sort_keys=True),counts,flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);rtl.write_text(origRtl);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
