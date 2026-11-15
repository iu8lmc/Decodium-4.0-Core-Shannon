#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeSpectrumTimer.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/spectrum_timer_before.inc').read_text();mark='void DecodiumBridge::onSpectrumTimer()';head=n.split(mark,1)[0];guard='            decodium::normalizeWaterfallRingPosition(m_wfRingPos, WF_RING_SIZE);\n';assert n.count(guard)==1 and n.replace(guard,'')==head+fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {if(values.value("action").toString()==QStringLiteral("start")){m_sstvWorkspaceActive=false;m_mode=QStringLiteral("FT8");if(m_legacyStateTimer)m_legacyStateTimer->stop();m_monitoring=true;m_transmitting=false;m_tuning=false;m_spectrumVisible=true;m_legacyPcmSpectrumFeed=true;m_forceGpuPanadapterFft.store(false);m_gpuPanadapterFftAvailable.store(false);m_panadapterItems.clear();if(m_spectrumTimer)m_spectrumTimer->stop();m_nfa=200;m_nfb=3000;m_wfRingPos=4096;for(int i=0;i<4096;i++)m_wfRing[i]=static_cast<short>(2000.0*std::sin(2.0*3.141592653589793*1000.0*i/12000.0));m_lastPanadapterData.clear();m_panadapterComputeBusy.store(false);m_lastPanadapterFrameMs=QDateTime::currentMSecsSinceEpoch()-5000;onSpectrumTimer();return {{"started",true},{"cat",m_catConnected},{"tx",m_transmitting}};} int pos=-1;float max=-1e9;for(int i=0;i<m_lastPanadapterData.size();++i)if(m_lastPanadapterData[i]>max){max=m_lastPanadapterData[i];pos=i;}double hz=pos>=0?m_lastPanFreqMin+(m_lastPanFreqMax-m_lastPanFreqMin)*pos/m_lastPanadapterData.size():-1;return {{"monitor",m_monitoring},{"busy",m_panadapterComputeBusy.load()},{"bins",m_lastPanadapterData.size()},{"peakHz",hz},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.logSstvQso({action:"start"});waitSpectrum.start();}}
 Timer {id:waitSpectrum;interval:200;repeat:true;property int ticks:0;onTriggered:{var v=bridge.logSstvQso({action:"read"});if(v.bins>0 || ++ticks>50){console.log("[MODE-CLEANUP] "+JSON.stringify(v));stop();Qt.quit();}}}
'''
rigs=[];values=[]
try:
 sstv.write_text(s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <cmath>\n#include <QTimer>').replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_UTC_TIMER_INSERTION_POINT';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='SpectrumTimer'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-modecleanup-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[MODE-CLEANUP] ',1)[1] for line in result.stdout.splitlines() if '[MODE-CLEANUP] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value['bins']>0 and 950<=value['peakHz']<=1050 and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
