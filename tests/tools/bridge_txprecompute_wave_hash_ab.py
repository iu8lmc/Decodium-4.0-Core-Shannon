#!/usr/bin/env python3
"""Compare complete precomputed TX waveform bytes between isolated app builds."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];source=r/'src/bridge/DecodiumBridgeTxPrecompute.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';qml=r/'build/qml/decodium/Main.qml';orig=source.read_text();orig_sstv=sstv.read_text();orig_qml=qml.read_text();marker='QAudioDevice DecodiumBridge::resolveTxOutputDevice(';head=orig.split(marker,1)[0];new=marker+orig.split(marker,1)[1];old=(r/'tests/bridge_refactor_fixtures/txprecompute_before.inc').read_text()
for a,b in [('findOutputDevice(', 'decodiumBridgeFindTxOutputDevice('),('audioDeviceIdSettingForLog(', 'decodiumBridgeAudioDeviceIdSettingForLog('),('audioDeviceIdForSettings(', 'decodiumBridgeAudioDeviceIdForSettings('),('bridgeLog(', 'decodiumBridgeTxPrecomputeLog(')]:old=old.replace(a,b)
assert old==new
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert orig_sstv.count(anchor)==1
inj='''\n#if DECODIUM_HAS_SSTV\n    { Q_UNUSED(values); const bool previous=m_txEnabled; m_txEnabled=true; precomputeTxAudioForCurrentMessage(QStringLiteral("explicit-lab")); m_txEnabled=previous; return {{QStringLiteral("hash"),QString::fromLatin1(QCryptographicHash::hash(QByteArray(reinterpret_cast<const char*>(m_txAudioCache.wave.constData()),m_txAudioCache.wave.size()*sizeof(float)),QCryptographicHash::Sha256).toHex())},{QStringLiteral("samples"),m_txAudioCache.wave.size()},{QStringLiteral("message"),m_txAudioCache.message},{QStringLiteral("transmitting"),m_transmitting},{QStringLiteral("cat"),m_catConnected}}; }\n#endif\n'''
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";bridge.grid="JM75";bridge.currentTx=6;var a=bridge.logSstvQso({});var b=bridge.logSstvQso({});console.log('[PRECOMPUTE] '+JSON.stringify({first:a,second:b}));Qt.quit();}}\n'''
rigs=[];obs=[]
try:
 sstv.write_text(orig_sstv.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QCryptographicHash>',1).replace(anchor,anchor+inj,1))
 for label,body in [('before',old),('after',new)]:
  source.write_text(head+body);subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(orig_qml.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='PrecomputeExplicit'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-precompute-explicit-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',port));socks.append(s)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-audio-input','BlackHole 16ch','--lab-audio-output','BlackHole 2ch','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[x.split('[PRECOMPUTE] ',1)[1] for x in result.stdout.splitlines() if '[PRECOMPUTE] ' in x]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   x=json.loads(rows[0]);assert x['first']['samples']>0 and len(x['first']['hash'])==64 and x['first']==x['second'] and not x['first']['cat'] and not x['first']['transmitting'],x
   logs=Path.home()/'Library/Application Support/IU8LMC/Decodium';diagnostic=''.join(p.read_text() for p in (logs/'decodium_diagnostic.1.log',logs/'decodium_diagnostic.log') if p.exists()).rsplit(rig,1)[-1];assert 'TX audio precomputed (explicit-lab)' in diagnostic and 'cache=1' in diagnostic
   obs.append(x);print(label,json.dumps(x,sort_keys=True),flush=True)
 assert obs[0]==obs[1],obs
finally:
 source.write_text(orig);sstv.write_text(orig_sstv);qml.write_text(orig_qml)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
