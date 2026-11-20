#!/usr/bin/env python3
"""Compare real idle audio buffer release in isolated apps."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeSstv.cpp'; mod=r/'src/bridge/DecodiumBridgeAudioBuffers.cpp'; sstv=main; qml=r/'build/qml/decodium/Main.qml'
m,n,q=(p.read_text() for p in (main,mod,qml));s=m
signature='void DecodiumBridge::invalidateTxAudioCache()';head,tail=n.split(signature,1);fixture=signature+tail
baseline=subprocess.check_output(['git','show','v1.0.737:src/bridge/DecodiumBridge.cpp'],cwd=r,text=True);start=baseline.index(signature);end=baseline.index('QAudioDevice DecodiumBridge::resolveTxOutputDevice(',start);old=baseline[start:end].replace('bridgeLog(','decodiumBridgeAudioLevelLog(');assert old.strip()==fixture.strip()

anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);bool safe=!m_catConnected&&!m_transmitting&&!m_monitoring&&!m_txPcmBuffer; if(!safe)return {{"wasShutdown",true},{"shutdown",false},{"safe",false},{"cat",m_catConnected},{"tx",m_transmitting}};
 m_txAudioCache.wave={0.1f,0.2f};m_txPcmData=QByteArray("pcm");m_audioBuffer={1,2};m_transmitting=true;releaseIdleAudioBuffers();bool held=!m_txAudioCache.wave.isEmpty()&&!m_txPcmData.isEmpty()&&!m_audioBuffer.isEmpty()&&m_audioBufferReleaseTimer&&m_audioBufferReleaseTimer->isActive();
 m_transmitting=false;m_monitoring=false;m_monitorRequested=false;releaseIdleAudioBuffers();bool cleared=m_txAudioCache.wave.isEmpty()&&m_txPcmData.isEmpty()&&m_audioBuffer.isEmpty();if(m_audioBufferReleaseTimer)m_audioBufferReleaseTimer->stop();bool ok=safe&&held&&cleared&&!m_catConnected&&!m_transmitting;
 return {{"wasShutdown",!ok},{"shutdown",ok},{"safe",safe},{"held",held},{"cleared",cleared},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''


probe='''\n Timer {interval:14000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 for label in ('before','after'):
  instrumented=s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QFile>\n#include <QFileInfo>\n#include <QElapsedTimer>\n#include <QThread>',1).replace(anchor,anchor+inj,1)
  if label=='before':
   main.write_text(instrumented);mod.write_text(head+old)
  else:main.write_text(instrumented);mod.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='SstvStatus'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   env=os.environ.copy();env.update(LAB_ADIF_PATH=tmp+'/synthetic.adi',DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   sockets=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sockets.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[DISPLAY-FREQ-LAB] ',1)[1] for line in result.stdout.splitlines() if '[DISPLAY-FREQ-LAB] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-8000:])
   val=json.loads(rows[0]);assert not val['wasShutdown'] and val['shutdown'] and not val['cat'] and not val['tx'],val
   results.append(val);print(label,json.dumps(val,sort_keys=True),flush=True)
 assert results[0]==results[1]
finally:
 for p,content in ((main,m),(mod,n),(qml,q)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
