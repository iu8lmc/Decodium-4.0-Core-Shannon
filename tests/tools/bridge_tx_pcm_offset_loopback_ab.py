#!/usr/bin/env python3
"""Compare timed PCM offset using two isolated FT8 apps and real slot alignment."""
from pathlib import Path
import json,os,re,shutil,subprocess,sys,tempfile
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';mod=r/'src/bridge/DecodiumBridgeTxTiming.cpp';m=main.read_text();n=mod.read_text();sig='qint64 DecodiumBridge::syncTxPcmStartOffsetBytes(';nextsig='bool DecodiumBridge::shouldAlignTxAudioToCurrentSyncSlot() const';a=n.index(sig);b=n.index(nextsig,a);current=n[a:b]
base=subprocess.check_output(['git','show','v1.0.737:src/bridge/DecodiumBridge.cpp'],cwd=r,text=True);x=base.index(sig);y=base.index('void DecodiumBridge::scheduleSyncTxBoundaryStop(',x);old=base[x:y].replace('txSyncLeadInMsForMode(', 'decodiumBridgeTxSyncLeadInMsForMode(');assert old==current
line='    int const elapsedMs = static_cast<int>(nowMs % static_cast<qint64>(periodMs));\n';assert current.count(line)==1
instrumented=current.replace(line,line+'    qWarning("[LAB-PCM-OFFSET] elapsed=%d size=%lld", elapsedMs, static_cast<long long>(pcmSizeBytes));\n',1)
point='// BRIDGE_LAB_INSERTION_POINT';assert m.count(point)==1
sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; original_sstv=sstv.read_text(); qml=r/'build/qml/decodium/Main.qml'; original_qml=qml.read_text()
starttx=r/'src/bridge/DecodiumBridgeStartTx.cpp';original_starttx=starttx.read_text();mac_branch='#if defined(Q_OS_MAC)\n    {\n        bool requestedDeviceFound = false;';assert original_starttx.count(mac_branch)==1
mac_pcm='#if defined(Q_OS_MAC)\n    needPcm = false;\n#endif';assert original_starttx.count(mac_pcm)==1
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert original_sstv.count(anchor)==1
inject='''
#if DECODIUM_HAS_SSTV
 if(values.value("_labPcmNative").toBool()){m_useLegacyTxBackend=false;m_forceLegacyTxForSpecialOp=false;m_currentTx=6;m_explicitManualTxRequest=6;bool native=!usingLegacyBackendForTx();startTx();return {{"native",native},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''\n Timer {interval:7000;running:true;repeat:false;onTriggered:{if(bridge.callsign==="9H1SR"){nativeTx.interval=15000-(Date.now()%15000)+200;nativeTx.start();}}}\n Timer {id:nativeTx;repeat:false;onTriggered:{console.log("[LAB-NATIVE-PCM] "+JSON.stringify(bridge.logSstvQso({_labPcmNative:true})));}}\n'''

policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';env=os.environ.copy();env.update(DECODIUM_LAB_QPA_PLATFORM='cocoa', DECODIUM_LAB_SENDTX_MS='300000')
try:
 starttx.write_text(original_starttx.replace(mac_branch,mac_branch.replace('#if defined(Q_OS_MAC)','#if 0'),1).replace(mac_pcm,mac_pcm.replace('#if defined(Q_OS_MAC)','#if 0'),1))
 sstv.write_text(original_sstv.replace(anchor,anchor+inject,1))
 for label in ('before','after'):
  if label=='before':main.write_text(m.replace(point,'int decodiumBridgeTxSyncLeadInMsForMode(const QString&);\n'+instrumented+point,1));mod.write_text(n[:a]+n[b:])
  else:main.write_text(m);mod.write_text('#include <QDebug>\n'+n[:a]+instrumented+n[b:])
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(original_qml.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1))
  proc=subprocess.run(['sandbox-exec','-p',policy,sys.executable,'tests/tools/bridge_ft8_two_app_lab.py','aligned'],cwd=r,env=env,capture_output=True,text=True,timeout=135)
  print(label,proc.stdout.strip(),proc.stderr.strip(),flush=True)
  if proc.returncode:raise RuntimeError('aligned FT8 loopback failed')
  report=json.loads(proc.stdout.strip().splitlines()[-1]);artifact=Path(report['artifacts']);txlog=(artifact/'pair-aligned-tx.log').read_text(errors='replace');assert '[LAB-NATIVE-PCM] ' in txlog and '\"native\":true' in txlog, 'native TX selection was not confirmed';hits=re.findall(r'\[LAB-PCM-OFFSET\] elapsed=(\d+) size=(\d+)',txlog);assert hits, 'timed PCM offset path was not reached';report['offset_calls']=[{'elapsed_ms':int(e),'size':int(z)} for e,z in hits];report.pop('artifacts',None)
  evidence=r/'tests/bridge_refactor_evidence/tx_pcm_offset';evidence.mkdir(parents=True,exist_ok=True);(evidence/(label+'.json')).write_text(json.dumps(report,indent=2)+'\n')
  if artifact.parent==Path(tempfile.gettempdir()) and artifact.name.startswith('decodium-bridge-ft8-aligned-'):
   for log in artifact.glob('*.log'):
    match=re.search(r'Application identity: (Decodium - BridgePairaligned(?:TX|RX)[0-9a-f]{8} - test)',log.read_text(errors='replace'))
    if match:
     for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
      target=parent/match.group(1)
      if target.is_dir():shutil.rmtree(target)
   shutil.rmtree(artifact)
finally:
 main.write_text(m);mod.write_text(n);sstv.write_text(original_sstv);qml.write_text(original_qml);starttx.write_text(original_starttx)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
