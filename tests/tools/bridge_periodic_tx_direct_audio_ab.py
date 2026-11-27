#!/usr/bin/env python3
"""Compare original and extracted manual TX using isolated virtual audio."""
from pathlib import Path
import json,re,shutil,subprocess,tempfile
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgePeriodicTx.cpp';m=main.read_text();n=module.read_text();sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';originalSstv=sstv.read_text();f=(r/'tests/bridge_refactor_fixtures/periodic_tx_before.inc').read_text();mark='void DecodiumBridge::checkAndStartPeriodicTx()';head=n.split(mark,1)[0];assert n==head+f
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
try:
 anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert originalSstv.count(anchor)==1
 injection='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);m_mode=QStringLiteral("FT8");m_useLegacyTxBackend=false;m_forceLegacyTxForSpecialOp=false;m_txEnabled=true;m_manualTxHold=false;m_transmitting=false;m_tuning=false;m_autoCqRepeat=false;m_multiAnswerMode=false;m_autoSeq=false;m_currentTx=6;m_explicitManualTxRequest=6;m_txDisabledMask=0;int slot=int(correctedUtcMsecsSinceStartOfDay()/15000);m_txPeriod=(slot%2)==0?1:0;checkAndStartPeriodicTx();return {{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
 sstv.write_text(originalSstv.replace(anchor,anchor+injection,1))
 f=f.replace('        startTx();','        qWarning("[LAB-PERIODIC-DISPATCH]"); startTx();')
 n=head+f
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,f+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  proc=subprocess.run(['sandbox-exec','-p',policy,'python3','tests/tools/bridge_periodic_tx_audio_runner.py'],cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=150)
  print(label,proc.stdout,flush=True)
  if proc.returncode:raise RuntimeError('virtual FT8 loopback failed')
  report=json.loads(proc.stdout.strip().splitlines()[-1]);artifact=Path(report['artifacts'])
  txlog=(artifact/'pair-immediate-tx.log').read_text(errors='replace');assert '[LAB-PERIODIC-DISPATCH]' in txlog,'periodic scheduler did not dispatch'
  if artifact.parent==Path(tempfile.gettempdir()) and artifact.name.startswith('decodium-bridge-ft8-'):
   evidence=r/'tests/bridge_refactor_evidence/periodic_tx_direct';evidence.mkdir(parents=True,exist_ok=True);report.pop('artifacts',None);(evidence/(label+'.json')).write_text(json.dumps(report,indent=2)+'\n')
   for log in artifact.glob('*.log'):
    match=re.search(r'Application identity: (Decodium - BridgePairimmediate(?:TX|RX)[0-9a-f]{8} - test)',log.read_text(errors='replace'))
    if match:
     for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
      target=parent/match.group(1)
      if target.is_dir():shutil.rmtree(target)
   shutil.rmtree(artifact)
finally:
 main.write_text(m);module.write_text(head+(r/'tests/bridge_refactor_fixtures/periodic_tx_before.inc').read_text());sstv.write_text(originalSstv)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
