#!/usr/bin/env python3
"""Compare shutdown and persisted settings in isolated Decodium processes."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridge.cpp'; mod=r/'src/bridge/DecodiumBridgeTune.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,n,s,q=(p.read_text() for p in (main,mod,sstv,qml)); fixture=(r/'tests/bridge_refactor_fixtures/tune_before.inc').read_text(); helper=(r/'tests/bridge_refactor_fixtures/tune_wave_before.inc').read_text();head=n.split('static QVector<float> buildTuneWaveform(',1)[0]; assert n==head+helper+'\n'+fixture

init=r/'src/bridge/DecodiumBridgeLegacyInit.cpp';initOriginal=init.read_text();macro='#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled';assert initOriginal.count(macro)==1
sstvAnchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(sstvAnchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);QCoreApplication::instance()->setProperty("decodiumEmbeddedLegacyRigControlEnabled",false);bool ready=ensureLegacyBackendAvailable();m_useLegacyTxBackend=true;m_forceLegacyTxForSpecialOp=false;bool rig=m_legacyBackend&&m_legacyBackend->rigControlEnabled();return {{"ready",ready},{"legacy",usingLegacyBackendForTx()},{"rig",rig}};}
#endif
'''

rigs=[]; results=[]
try:
 init.write_text(initOriginal.replace(macro,'#define embeddedLegacyRigControlEnabledForBackend(...) false',1));sstv.write_text(s.replace(sstvAnchor,sstvAnchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   main.write_text(m+'\n#undef bridgeLog\n'+head+helper+'\n'+fixture.replace('void DecodiumBridge::startTune()\n{','void DecodiumBridge::startTune()\n{\n qWarning("[LAB-TUNE-START]");',1));mod.write_text(head)
  else:main.write_text(m);mod.write_text(n.replace('void DecodiumBridge::startTune()\n{','void DecodiumBridge::startTune()\n{\n qWarning("[LAB-TUNE-START]");',1))
  text=main.read_text() if label=='before' else mod.read_text();text=text.replace('void DecodiumBridge::stopTune()\n{','void DecodiumBridge::stopTune()\n{\n qWarning("[LAB-TUNE-STOP]");',1);(main if label=='before' else mod).write_text(text)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  result=subprocess.run(['python3','tests/tools/bridge_tune_legacy_audio_runner.py','--binary','build/decodium','--kind','cw'],cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=100)
  if result.returncode:raise RuntimeError(result.stdout[-8000:])
  val=json.loads(result.stdout.strip().splitlines()[-1]);assert val['seen'] and val['accepted'] and val['stopped'] and not val['cat'] and val['audioPeak']>0.003 and val['stopSeen'],val;results.append(val);print(label,json.dumps(val,sort_keys=True),flush=True)

 assert {k:v for k,v in results[0].items() if k not in ("audioPeak","tailPeak")}=={k:v for k,v in results[1].items() if k not in ("audioPeak","tailPeak")}
finally:
 for p,content in ((main,m),(mod,n),(sstv,s),(qml,q),(init,initOriginal)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
