#!/usr/bin/env python3
"""Compare SSB CAT permission in isolated sessions with no remote or physical rig."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeSsb.cpp';qml=r/'build/qml/decodium/Main.qml';m,s,n,q=(p.read_text() for p in (main,sstv,module,qml));anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
injection='''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values);m_mode=QStringLiteral("SSB");m_catConnected=true;m_catBackend=QStringLiteral("cat4om");m_catMode=QStringLiteral("USB");m_ssbRadioMode=QStringLiteral("USB");m_decoPortUseRemote=false;m_ssbControls.insert(QStringLiteral("busy"),false);m_audioOutputDevice=QStringLiteral("BlackHole 2ch");m_audioOutputDeviceId.clear();bool found=false;QAudioDevice device=resolveTxOutputDevice(&found);bool pre=ssbValid()&&found&&!device.isNull()&&m_cat4OmCat&&!m_cat4OmCat->canPtt()&&!m_transmitting&&!m_tuning;bool blocked=pre&&!ssbBegin()&&!m_ssbTxActive;m_catConnected=false;ssbSend(QVector<short>{1,2});ssbStop();ssbSetRadioMode(QStringLiteral("invalid"));bool invalidMode=m_ssbRadioMode==QStringLiteral("USB");setSsbRxAgcMode(99);setSsbRxAgcThreshold(-999);bool agc=ssbRxAgcMode()==5&&ssbRxAgcThreshold()==-120;ssbSetControl(QStringLiteral("synthetic"),0.5);ssbRefreshControls();ssbMonitorAudio(QVector<short>{1,2});bool idle=!ssbValid()&&!m_ssbTxActive&&!m_transmitting;return {{"preconditions",pre},{"blocked",blocked},{"invalidMode",invalidMode},{"agc",agc},{"idle",idle},{"cat",m_catConnected},{"tx",m_transmitting}}; }
#endif
'''
probe='''\n Timer {interval:12000;running:true;repeat:false;onTriggered:Qt.quit()}\n Timer {interval:10000;running:true;repeat:false;onTriggered:console.log('[SSBPERMIT-PROBE] '+JSON.stringify(bridge.logSstvQso({}))) }\n'''
rigs=[];out=[]
try:
 sstv.write_text(s.replace(anchor,anchor+injection,1))
 for label in ('before','after'):
  if label=='before':
   marker='// SSB voice workspace.'; assert m.count(marker)==1
   include='bool decodiumBridgeSharedCatCanPttExplicit(QObject const*, DecodiumTransceiverManager*, const QString&, DecodiumOmniRigManager*, DecodiumLegacyBackend*, DecodiumCat4OmManager*);\n#include "SsbBridge.inc"\n'
   main.write_text(m.replace(marker,marker+'\n'+include,1));module.write_text('#include "DecodiumBridge.h"\n')
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='SsbPermit'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-ssbpermit-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[x.split('[SSBPERMIT-PROBE] ',1)[1] for x in proc.stdout.splitlines() if '[SSBPERMIT-PROBE] ' in x]
   if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(proc.stdout[-6000:])
   value=json.loads(rows[0]);expected={'preconditions':True,'blocked':True,'invalidMode':True,'agc':True,'idle':True,'cat':False,'tx':False};assert value==expected,value;out.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert out[0]==out[1]
finally:
 for p,text in ((main,m),(sstv,s),(module,n),(qml,q)):p.write_text(text)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   target=parent/('Decodium - '+rig+' - test')
   if target.is_dir():shutil.rmtree(target)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
