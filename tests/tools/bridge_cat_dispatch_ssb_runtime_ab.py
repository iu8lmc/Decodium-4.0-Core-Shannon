#!/usr/bin/env python3
"""Compare SSB CAT permission in isolated sessions with no remote or physical rig."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeCatDispatch.cpp';qml=r/'build/qml/decodium/Main.qml';m,s,n,q=(p.read_text() for p in (main,sstv,module,qml));ssb=r/'src/bridge/SsbBridge.inc';ssb_original=ssb.read_text();oldcall='activeCatCanPtt(this,m_hamlibCat,m_catBackend,m_omniRigCat,m_legacyBackend,m_cat4OmCat)';newcall='decodiumBridgeSharedCatCanPttExplicit(this,m_hamlibCat,m_catBackend,m_omniRigCat,m_legacyBackend,m_cat4OmCat)';assert ssb_original.count(newcall)==1;fixture=(r/'tests/bridge_refactor_fixtures/cat_dispatch_before.inc').read_text();methods=(r/'tests/bridge_refactor_fixtures/cat_dispatch_sstv_before.inc').read_text();assert fixture in n and methods in n;anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
prototypes='''RemoteRadioLink* decodiumBridgeDecoPortCatLink();
bool decodiumBridgeSharedCatCanPtt(QObject const*,DecodiumTransceiverManager*,const QString&,DecodiumOmniRigManager*,DecodiumLegacyBackend*);
bool decodiumBridgeSharedCatUsesVox(QObject const*,DecodiumTransceiverManager*,const QString&,DecodiumOmniRigManager*);
void decodiumBridgeSharedActiveCatSetPtt(QObject const*,DecodiumTransceiverManager*,const QString&,bool,DecodiumOmniRigManager*,DecodiumLegacyBackend*);
void decodiumBridgeCwSetCatFrequency(QObject const*,DecodiumTransceiverManager*,const QString&,double,DecodiumOmniRigManager*,DecodiumLegacyBackend*);
'''
injection='''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values);m_mode=QStringLiteral("SSB");m_catConnected=true;m_catBackend=QStringLiteral("cat4om");m_catMode=QStringLiteral("USB");m_ssbRadioMode=QStringLiteral("USB");m_decoPortUseRemote=false;m_ssbControls.insert(QStringLiteral("busy"),false);m_audioOutputDevice=QStringLiteral("BlackHole 2ch");m_audioOutputDeviceId.clear();bool found=false;QAudioDevice device=resolveTxOutputDevice(&found);bool pre=ssbValid()&&found&&!device.isNull()&&m_cat4OmCat&&!m_cat4OmCat->canPtt()&&!m_transmitting&&!m_tuning;bool blocked=pre&&!ssbBegin()&&!m_ssbTxActive;m_catConnected=false;return {{"preconditions",pre},{"blocked",blocked},{"cat",m_catConnected},{"tx",m_transmitting}}; }
#endif
'''
probe='''\n Timer {interval:12000;running:true;repeat:false;onTriggered:Qt.quit()}\n Timer {interval:10000;running:true;repeat:false;onTriggered:console.log('[SSBPERMIT-PROBE] '+JSON.stringify(bridge.logSstvQso({}))) }\n'''
rigs=[];out=[]
try:
 sstv.write_text(s.replace(anchor,prototypes+anchor+injection,1))
 for label in ('before','after'):
  if label=='before':
   ssb.write_text(ssb_original);point='// BRIDGE_CONSTRUCTOR_INSERTION_POINT';assert m.count(point)==1;assert m.count('// BRIDGE_CAT_DISPATCH_SSTV_INSERTION_POINT')==1;extra='bool decodiumBridgeSharedCatCanPttExplicit(QObject const* owner, DecodiumTransceiverManager* h, const QString& b, DecodiumOmniRigManager* o, DecodiumLegacyBackend* l, DecodiumCat4OmManager* c) { return activeCatCanPtt(owner,h,b,o,l,c); }\n';main.write_text(m.replace(point,fixture+'\n'+extra+point,1).replace('// BRIDGE_CAT_DISPATCH_SSTV_INSERTION_POINT',methods+'\n// BRIDGE_CAT_DISPATCH_SSTV_INSERTION_POINT',1));module.write_text('#include "DecodiumBridge.h"\n'+(r/'tests/bridge_refactor_fixtures/cat_ptt_state_before.inc').read_text())
  else:main.write_text(m);module.write_text(n);ssb.write_text(ssb_original)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='SsbPermit'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-ssbpermit-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[x.split('[SSBPERMIT-PROBE] ',1)[1] for x in proc.stdout.splitlines() if '[SSBPERMIT-PROBE] ' in x]
   if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(proc.stdout[-6000:])
   value=json.loads(rows[0]);expected={'preconditions':True,'blocked':True,'cat':False,'tx':False};assert value==expected,value;out.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert out[0]==out[1]
finally:
 for p,text in ((main,m),(sstv,s),(module,n),(qml,q),(ssb,ssb_original)):p.write_text(text)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   target=parent/('Decodium - '+rig+' - test')
   if target.is_dir():shutil.rmtree(target)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
