#!/usr/bin/env python3
"""Compare SSTV storage startup and run isolated application checks."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeSstvStorageStartup.cpp';qml=r/'build/qml/decodium/Main.qml';s,n,q=(p.read_text() for p in (sstv,module,qml));fixture=(r/'tests/bridge_refactor_fixtures/sstv_storage_startup_before.inc').read_text();signature='void DecodiumBridge::initialiseSstvStorage()';body=signature+n.split(signature,1)[1];assert body.replace('decodiumSstvQsoRequestFromMap(', 'sstvQsoRequestFromMap(')==fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(';assert s.count(anchor)==1
injection='''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values);if(m_sstvShareController)m_sstvShareController->incomingHandoffReady(QVariantMap{});return {{"emitted",m_sstvShareController!=nullptr}}; }
#endif
'''
probe='''
 property bool labHandoffRejected: false
 Connections {target:bridge; function onErrorMessage(message){if(String(message).indexOf("validated handoff has no transfer identifier")>=0)labHandoffRejected=true}}
 Timer {interval:12000;running:true;repeat:false;onTriggered:{var ready=bridge.sstvAvailable&&bridge.sstvStorageReady&&bridge.sstvGallery!==null&&bridge.sstvShare!==null;bridge.sstvRxAutoSaveEnabled=true;var enabled=bridge.sstvRxAutoSaveEnabled;bridge.sstvRxAutoSaveEnabled=false;labResult.ready=ready;labResult.enabled=enabled;labResult.disabled=!bridge.sstvRxAutoSaveEnabled;labResult.emitted=bridge.logSstvQso({}).emitted;labFinish.start();}}
 Timer {id:labFinish;property bool ready:false;property bool enabled:false;property bool disabled:false;property bool emitted:false;interval:1000;repeat:false;onTriggered:console.log('[SSTVSTARTUP-PROBE] '+JSON.stringify({ready:ready,enabled:enabled,disabled:disabled,emitted:emitted,rejected:labHandoffRejected,tx:bridge.transmitting,monitor:bridge.monitoring}))}
'''.replace('labResult.', 'labFinish.')

rigs=[];out=[]
try:
 for label in ('before','after'):
  if label=='before':source=s.replace(anchor,fixture+anchor,1);module.write_text('#include "DecodiumBridge.h"\n')
  else:source=s;module.write_text(n)
  target='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert source.count(target)==1;sstv.write_text(source.replace(target,target+injection,1))
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='SstvStartup'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-sstv-startup-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:*"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','17000'];proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[x.split('[SSTVSTARTUP-PROBE] ',1)[1] for x in proc.stdout.splitlines() if '[SSTVSTARTUP-PROBE] ' in x]
   if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(proc.stdout[-8000:])
   value=json.loads(rows[0]);expected={'ready':True,'enabled':True,'disabled':True,'emitted':True,'rejected':True,'tx':False,'monitor':False};assert value==expected,value;out.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert out[0]==out[1]
finally:
 sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   target=parent/('Decodium - '+rig+' - test')
   if target.is_dir():shutil.rmtree(target)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
