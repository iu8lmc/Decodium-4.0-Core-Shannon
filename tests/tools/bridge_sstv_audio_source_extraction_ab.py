#!/usr/bin/env python3
"""Compare SSTV QSO association before and after extraction in isolated apps."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeSstv.cpp'; mod=r/'src/bridge/DecodiumBridgeSstvAudioSource.cpp'; sstv=main; qml=r/'build/qml/decodium/Main.qml'
m,n,q=(p.read_text() for p in (main,mod,qml));s=m;fixture=(r/'tests/bridge_refactor_fixtures/sstv_audio_source_before.inc').read_text();assert n.endswith(fixture);head=n[:-len(fixture)]; helper=(r/'tests/bridge_refactor_fixtures/sstv_stream_id_before.inc').read_text(); assert helper in head
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 if(values.value("_lab").toBool()) {using K=decodium::sstv::SstvAudioSourceKind;bool oldRemote=m_decoPortUseRemote,oldRx=m_sstvRxRequested;bool rtl=rtlSdrEnabled(),tci=usingTciAudioInput();m_decoPortUseRemote=true;m_sstvRxRequested=false;bool remote=currentSstvAudioSourceKind()==K::DecoPort&&!nativeSstvRxForcesDedicatedAudioCapture();m_decoPortUseRemote=false;m_sstvRxRequested=true;bool forced=nativeSstvRxForcesDedicatedAudioCapture()==(!rtl&&!tci);bool local=rtl||tci||currentSstvAudioSourceKind()==K::LocalSoundCard;QVariantList ids;for(K kind:{K::LocalSoundCard,K::WebSdr,K::DecoPort,K::Tci,K::RtlSdr,K::LegacyBackend,K::Replay,K::Unknown})ids.append(QString::number(currentSstvAudioStreamId(kind)));bool stable=true;int i=0;for(K kind:{K::LocalSoundCard,K::WebSdr,K::DecoPort,K::Tci,K::RtlSdr,K::LegacyBackend,K::Replay,K::Unknown})stable=stable&&(ids.at(i++).toString()==QString::number(currentSstvAudioStreamId(kind)));bool nonzero=ids.at(0).toString()!=QStringLiteral("0")&&ids.at(2).toString()!=QStringLiteral("0")&&ids.at(7).toString()==QStringLiteral("0");m_rtlSdrEnabledCache.invalidate();m_rtlSdrEnabledCache.value(decodium::activeSettingsProfileName(),[](){return true;});bool oldPending=m_rtlSdrDiscoveryPending;m_rtlSdrDiscoveryPending=true;m_decoPortUseRemote=false;bool rtlSelect=currentSstvAudioSourceKind()==K::RtlSdr&&!nativeSstvRxForcesDedicatedAudioCapture();m_decoPortUseRemote=true;bool remoteOverRtl=currentSstvAudioSourceKind()==K::DecoPort;m_rtlSdrDiscoveryPending=oldPending;m_rtlSdrEnabledCache.invalidate();m_decoPortUseRemote=oldRemote;m_sstvRxRequested=oldRx;bool ok=remote&&forced&&local&&stable&&nonzero&&rtlSelect&&remoteOverRtl&&!m_catConnected&&!m_transmitting;return {{"shutdown",ok},{"wasShutdown",!ok},{"remote",remote},{"forced",forced},{"local",local},{"stable",stable},{"nonzero",nonzero},{"rtlSelect",rtlSelect},{"remoteOverRtl",remoteOverRtl},{"ids",ids},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''

probe='''\n Timer {interval:14000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({_lab:true})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 for label in ('before','after'):
  instrumented=s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QFile>\n#include <QFileInfo>\n#include <QEventLoop>\n#include <QTimer>',1).replace(anchor,anchor+inj,1)
  if label=='before':
   main.write_text(instrumented.replace('qulonglong boundedUnsigned(', helper+'qulonglong boundedUnsigned(',1).replace(anchor,fixture+anchor,1));mod.write_text('#include "DecodiumBridge.h"\n')
  else:main.write_text(instrumented);mod.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='SstvAssociate'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   env=os.environ.copy();env.update(LAB_ADIF_PATH=tmp+'/synthetic.adi',DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   sockets=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sockets.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','45000']
   result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=65)
   rows=[line.split('[DISPLAY-FREQ-LAB] ',1)[1] for line in result.stdout.splitlines() if '[DISPLAY-FREQ-LAB] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-8000:])
   val=json.loads(rows[0]);print(label,json.dumps(val,sort_keys=True),flush=True);assert not val['wasShutdown'] and val['shutdown'] and not val['cat'] and not val['tx'],val
   readProbe="\n Timer {interval:9000;running:true;repeat:false;onTriggered:{console.log('[SHUTDOWN-READ] '+String(bridge.getSetting('ShutdownSynthetic','missing')));Qt.quit();}}\n"
   qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+readProbe,1))
   reopened=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=65)
   persisted=[line.split('[SHUTDOWN-READ] ',1)[1] for line in reopened.stdout.splitlines() if '[SHUTDOWN-READ] ' in line]
   if reopened.returncode or persisted!=['persisted']:raise RuntimeError('persistence readback failed: '+reopened.stdout[-8000:])
   val['readback']=persisted[0];results.append(val);print(label,json.dumps(val,sort_keys=True),flush=True)
 assert results[0]==results[1]
finally:
 for p,content in ((main,m),(mod,n),(qml,q)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
