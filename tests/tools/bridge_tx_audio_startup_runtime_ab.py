#!/usr/bin/env python3
"""Compare TX audio startup and real error signal wiring with CAT disabled."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridgeConstructor.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeTxAudioStartup.cpp';qml=r/'build/qml/decodium/Main.qml';m,s,n,q=(p.read_text() for p in (main,sstv,module,qml));fixture=(r/'tests/bridge_refactor_fixtures/tx_audio_startup_before.inc').read_text();anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
prototypes='#include "Audio/soundout.h"\n#include "Modulator/Modulator.hpp"\n#include <QBuffer>\n'
injection='''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values);QString error;auto conn=connect(this,&DecodiumBridge::errorMessage,this,[&](const QString& s){error=s;});bool objects=m_modulator&&m_soundOutput&&!m_modulator->isActive();if(m_soundOutput)m_soundOutput->error(QStringLiteral("synthetic fault"));disconnect(conn);m_transmitting=true;m_tuning=false;m_txPlaybackHoldUntilMs=QDateTime::currentMSecsSinceEpoch()+60000;m_txPlaybackReleasePending=false;if(m_modulator)m_modulator->stateChanged(Modulator::Idle);bool held=m_txPlaybackReleasePending;m_transmitting=false;m_txPlaybackHoldUntilMs=0;m_txPlaybackReleasePending=false;return {{"objects",objects},{"error",error==QStringLiteral("Audio TX: synthetic fault")},{"held",held},{"cat",m_catConnected},{"tx",m_transmitting}}; }
#endif
'''
probe='''\n Timer {interval:12000;running:true;repeat:false;onTriggered:Qt.quit()}\n Timer {interval:10000;running:true;repeat:false;onTriggered:console.log('[TXSTARTUP-PROBE] '+JSON.stringify(bridge.logSstvQso({}))) }\n'''
rigs=[];out=[]
try:
 sstv.write_text(s.replace(anchor,prototypes+anchor+injection,1))
 for label in ('before','after'):
  if label=='before':
   assert m.count('    initialiseTxAudioOutput();\n')==1;main.write_text(m.replace('#include "DecodiumBridge.h"', '#include "DecodiumBridge.h"\n'+prototypes,1).replace('    initialiseTxAudioOutput();\n',fixture,1));module.write_text('#include "DecodiumBridge.h"\n')
  else:main.write_text(m);module.write_text(n)
  build=subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
  if build.returncode:raise RuntimeError(build.stdout[-12000:])
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='TxAudioStart'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-txaudiostart-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[x.split('[TXSTARTUP-PROBE] ',1)[1] for x in proc.stdout.splitlines() if '[TXSTARTUP-PROBE] ' in x]
   if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(proc.stdout[-6000:])
   value=json.loads(rows[0]);expected={'objects':True,'error':True,'held':True,'cat':False,'tx':False};assert value==expected,value;out.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert out[0]==out[1]
finally:
 for p,text in ((main,m),(sstv,s),(module,n),(qml,q)):p.write_text(text)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   target=parent/('Decodium - '+rig+' - test')
   if target.is_dir():shutil.rmtree(target)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
