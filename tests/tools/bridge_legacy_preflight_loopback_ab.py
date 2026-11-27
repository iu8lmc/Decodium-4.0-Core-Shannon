#!/usr/bin/env python3
"""Compare original and extracted manual TX using isolated virtual audio."""
from pathlib import Path
import json,re,shutil,subprocess,tempfile
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeLegacyPreflight.cpp';m=main.read_text();n=module.read_text();f=(r/'tests/bridge_refactor_fixtures/legacy_bridge_preflight_before.inc').read_text();mark='bool DecodiumBridge::preflightLegacyBridgeTxBeforePtt(const QString& reason)';head=n.split(mark,1)[0];assert n==head+f
f=f.replace(mark+'\n{',mark+'\n{\n qWarning("[LAB-LEGACY-PREFLIGHT]");',1);n=head+f
init=r/'src/bridge/DecodiumBridgeLegacyInit.cpp';origInit=init.read_text();macro='#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled';assert origInit.count(macro)==1
sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';origSstv=sstv.read_text();runner=r/'tests/tools/bridge_ft8_matched_ab_lab.py';origRunner=runner.read_text();anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert origSstv.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);QCoreApplication::instance()->setProperty("decodiumEmbeddedLegacyRigControlEnabled",false);QCoreApplication::instance()->setProperty("decodiumEmbeddedLegacyRigControlEnabled",false);bool backendReady=ensureLegacyBackendAvailable();if(!m_legacyBackend || m_legacyBackend->rigControlEnabled()){qWarning("[LAB-LEGACY-RIG-UNSAFE]");return {{"started",false}};}if(m_legacyBackend)m_legacyBackend->setRigControlEnabled(false);m_catConnected=false;m_mode=QStringLiteral("FT8");m_useLegacyTxBackend=true;m_forceLegacyTxForSpecialOp=false;m_pttConfirmed=true;m_pttOnDispatched=false;m_currentTx=6;m_tuning=false;qWarning()<<"[LAB-LEGACY-POLICY] ready"<<backendReady<<"legacy"<<usingLegacyBackendForTx()<<"policy"<<shouldUseBridgeAudioForLegacyDigitalTx()<<"message"<<currentBridgeTxRepresentativeMessage();bool preflight=preflightLegacyBridgeTxBeforePtt(QStringLiteral("isolated-test"));qWarning()<<"[LAB-PREFLIGHT-RESULT]"<<preflight;bool ok=preflight&&startBridgeAudioForLegacyDigitalTx(QStringLiteral("isolated-virtual-audio"));qWarning()<<"[LAB-LEGACY-START-RESULT]"<<ok<<"CAT"<<m_catConnected;return {{"started",ok}};}
#endif
'''
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
try:
 init.write_text(origInit.replace(macro,'#define embeddedLegacyRigControlEnabledForBackend(...) false',1));sstv.write_text(origSstv.replace(anchor,anchor+inj,1));assert 'bridge.sendTx(6)' in origRunner;runner.write_text(origRunner.replace('bridge.sendTx(6)','bridge.logSstvQso({})',1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LEGACY_PREFLIGHT_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,f+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  proc=subprocess.run(['sandbox-exec','-p',policy,'python3','tests/tools/bridge_ft8_matched_ab_lab.py'],cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=150)
  print(label,proc.stdout,flush=True)
  if proc.returncode:raise RuntimeError('virtual FT8 loopback failed')
  report=json.loads(proc.stdout.strip().splitlines()[-1]);artifact=Path(report['artifacts'])
  if artifact.parent==Path(tempfile.gettempdir()) and artifact.name.startswith('decodium-bridge-ft8-'):
   evidence=r/'tests/bridge_refactor_evidence/legacy_preflight';evidence.mkdir(parents=True,exist_ok=True);txlog=(artifact/'pair-immediate-tx.log').read_text(errors='replace');assert '[LAB-LEGACY-RIG-UNSAFE]' not in txlog, 'legacy rig control enabled';assert '[LAB-PREFLIGHT-RESULT] true' in txlog and '[LAB-LEGACY-START-RESULT] true CAT false' in txlog, 'legacy start did not succeed without CAT';report['legacy_preflight_seen']='[LAB-LEGACY-PREFLIGHT]' in txlog;assert report['legacy_preflight_seen'], 'legacy audio start was not reached';report.pop('artifacts',None);(evidence/(label+'.json')).write_text(json.dumps(report,indent=2)+'\n')
   for log in artifact.glob('*.log'):
    match=re.search(r'Application identity: (Decodium - BridgePairimmediate(?:TX|RX)[0-9a-f]{8} - test)',log.read_text(errors='replace'))
    if match:
     for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
      target=parent/match.group(1)
      if target.is_dir():shutil.rmtree(target)
   shutil.rmtree(artifact)
finally:
 init.write_text(origInit);sstv.write_text(origSstv);runner.write_text(origRunner);main.write_text(m);module.write_text(head+(r/'tests/bridge_refactor_fixtures/legacy_bridge_preflight_before.inc').read_text())
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
