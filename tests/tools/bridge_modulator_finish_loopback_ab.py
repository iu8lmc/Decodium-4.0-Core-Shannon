#!/usr/bin/env python3
"""Compare original and extracted manual TX using isolated virtual audio."""
from pathlib import Path
import json,re,shutil,subprocess,tempfile
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeModulatorFinish.cpp';m=main.read_text();n=module.read_text();original=n;f=(r/'tests/bridge_refactor_fixtures/modulator_idle_before.inc').read_text();mark='void DecodiumBridge::finishModulatorIdlePlayback(const QString& reason)';head=n.split(mark,1)[0]
f=f.replace(mark+'\n{',mark+'\n{\n qWarning("[LAB-MODULATOR-FINISH]");',1);n=n.replace(mark+'\n{',mark+'\n{\n qWarning("[LAB-MODULATOR-FINISH]");',1)
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
try:
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_MODULATOR_FINISH_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,'#undef bridgeLog\n'+head+f+'\n#undef bridgeLog\n#undef txTimelineLog\n#undef activeCatCanPtt\n#undef activeCatSetPtt\n'+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  proc=subprocess.run(['sandbox-exec','-p',policy,'python3','tests/tools/bridge_ft8_matched_ab_lab.py'],cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=150)
  print(label,proc.stdout,flush=True)
  if proc.returncode:raise RuntimeError('virtual FT8 loopback failed')
  report=json.loads(proc.stdout.strip().splitlines()[-1]);artifact=Path(report['artifacts'])
  if artifact.parent==Path(tempfile.gettempdir()) and artifact.name.startswith('decodium-bridge-ft8-'):
   evidence=r/'tests/bridge_refactor_evidence/modulator_finish';evidence.mkdir(parents=True,exist_ok=True);txlog=(artifact/'pair-immediate-tx.log').read_text(errors='replace');report['modulator_finish_seen']='[LAB-MODULATOR-FINISH]' in txlog;assert report['modulator_finish_seen'], 'modulator finish was not reached';report.pop('artifacts',None);(evidence/(label+'.json')).write_text(json.dumps(report,indent=2)+'\n')
   for log in artifact.glob('*.log'):
    match=re.search(r'Application identity: (Decodium - BridgePairimmediate(?:TX|RX)[0-9a-f]{8} - test)',log.read_text(errors='replace'))
    if match:
     for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
      target=parent/match.group(1)
      if target.is_dir():shutil.rmtree(target)
   shutil.rmtree(artifact)
finally:
 main.write_text(m);module.write_text(original)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
