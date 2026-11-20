#!/usr/bin/env python3
"""Compare CAT connection signal routing in isolated apps."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeConstructor.cpp'; mod=r/'src/bridge/DecodiumBridgeCatTelemetry.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; qml=r/'build/qml/decodium/Main.qml'
complete=r/'src/bridge/DecodiumBridgeCompleteTxPlayback.cpp'; originalComplete=complete.read_text(); tci=r/'src/bridge/DecodiumBridgeTciAudio.cpp'; originalTci=tci.read_text(); m,n,s,q=(p.read_text() for p in (main,mod,sstv,qml)); fixture=(r/'tests/bridge_refactor_fixtures/cat_telemetry_init_before.inc').read_text();assert n.split('void DecodiumBridge::initialiseCatTelemetry()\n{\n',1)[1]==fixture+'}\n'
labFixture=fixture.replace('usingTciAudioInput()', 'property("labTci").toBool()')
assert labFixture != fixture and originalComplete.count('activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)') == 1
assert originalTci.count('|| !usingTciAudioInput()\n        || m_transmitting') == 1
labModule=n.replace(fixture,labFixture,1)
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);m_txEnabled=false;m_transmitting=false;m_tuning=false;m_sstvRxRequested=false;m_monitoring=true;m_tciAudioCaptureActive=true;setProperty("labTci",true);auto before=m_asyncAudioPos.load();QVector<short> samples{1000,-1000,2000,-2000};m_hamlibCat->tciPcmSamplesReady(samples);QCoreApplication::processEvents();bool pcm=m_asyncAudioPos.load()==before+4;setProperty("labTci",false);m_hamlibCat->tciPcmSamplesReady(samples);QCoreApplication::processEvents();bool pcmGuard=m_asyncAudioPos.load()==before+4;setProperty("labTci",true);m_monitoring=false;m_transmitting=true;m_cwTxActive=true;m_pendingCwText=QStringLiteral("test");m_autoCqRepeat=false;m_txPlaybackHoldUntilMs=123;m_txPlaybackHardDeadlineMs=456;m_txPlaybackReleasePending=true;m_txAudioRestartPending=true;m_hamlibCat->tciModActiveChanged(true);QCoreApplication::processEvents();bool activeGuard=m_txPlaybackHoldUntilMs==123&&m_transmitting;m_hamlibCat->tciModActiveChanged(false);QCoreApplication::processEvents();QCoreApplication::processEvents();bool completed=!m_transmitting&&!m_cwTxActive&&m_pendingCwText.isEmpty()&&m_txPlaybackHoldUntilMs==0&&m_txPlaybackHardDeadlineMs==0&&!m_txPlaybackReleasePending&&!m_txAudioRestartPending;m_transmitting=false;m_tciAudioCaptureActive=false;bool ok=pcm&&pcmGuard&&activeGuard&&completed&&!m_catConnected;return {{"wasShutdown",!ok},{"shutdown",ok},{"pcm",pcm},{"pcmGuard",pcmGuard},{"activeGuard",activeGuard},{"completed",completed},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 

probe='''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 complete.write_text(originalComplete.replace('activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)', 'false /* isolated simulated rig: no physical PTT */'))
 tci.write_text(originalTci.replace('|| !usingTciAudioInput()\n        || m_transmitting', '|| !property(\"labTci\").toBool()\n        || m_transmitting', 1))
 sstv.write_text(s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QBuffer>\n#include "DecodiumTransceiverManager.h"\n#include "DecodiumCat4OmManager.h"\n#include "DecodiumOmniRigManager.h"',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='    initialiseCatTelemetry();\n'; assert m.count(point)==1; main.write_text(m.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\nbool decodiumBridgePttSignalMatches(const QString&, const QString&);',1).replace(point,labFixture.replace("catSignalMatchesBackend", "decodiumBridgePttSignalMatches"),1));mod.write_text(labModule)
  else:main.write_text(m);mod.write_text(labModule)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='TciComplete'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
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
 for p,content in ((main,m),(mod,n),(sstv,s),(qml,q),(tci,originalTci),(complete,originalComplete)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
