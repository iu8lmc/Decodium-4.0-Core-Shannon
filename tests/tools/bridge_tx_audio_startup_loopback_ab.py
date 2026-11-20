#!/usr/bin/env python3
"""Compare TX audio startup using two Decodium sessions over BlackHole."""
from pathlib import Path
import json,re,shutil,subprocess,tempfile
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridgeConstructor.cpp';module=r/'src/bridge/DecodiumBridgeTxAudioStartup.cpp';m=main.read_text();original=module.read_text();fixture=(r/'tests/bridge_refactor_fixtures/tx_audio_startup_before.inc').read_text();signature='void DecodiumBridge::initialiseTxAudioOutput()\n{\n';assert original.split(signature,1)[1]==fixture+'}\n';head=original.split(signature,1)[0];instrumented=fixture.replace('        if (st == Modulator::Idle) {','        if (st == Modulator::Idle) {\n            qWarning("[LAB-TXSTARTUP-IDLE]");',1);assert instrumented!=fixture
timing_results=[]
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
try:
 for label in ('before','after'):
  if label=='before':
   assert m.count('    initialiseTxAudioOutput();\n')==1;includes='#include "Audio/soundout.h"\n#include "Modulator/Modulator.hpp"\n#include <QBuffer>\n';main.write_text(m.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n'+includes,1).replace('    initialiseTxAudioOutput();\n',instrumented,1));module.write_text('#include "DecodiumBridge.h"\n')
  else:main.write_text(m);module.write_text(head+signature+instrumented+'}\n')
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  proc=subprocess.run(['sandbox-exec','-p',policy,'python3','tests/tools/bridge_ft8_matched_ab_lab.py'],cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=150);print(label,proc.stdout,flush=True)
  timing_ok=proc.returncode==0;timing_results.append(timing_ok)
  if proc.returncode and 'FT8 audio was not aligned to the slot' not in proc.stdout:raise RuntimeError('virtual FT8 loopback failed')
  rows=[line for line in proc.stdout.splitlines() if line.startswith('{"role"')];assert len(rows)==1,proc.stdout
  report=json.loads(rows[0]);artifact=Path(report['artifacts']);txlog=(artifact/'pair-immediate-tx.log').read_text(errors='replace');report['modulator_idle_callback_seen']='[LAB-TXSTARTUP-IDLE]' in txlog;report['timing_check_passed']=timing_ok
  if artifact.parent==Path(tempfile.gettempdir()) and artifact.name.startswith('decodium-bridge-ft8-'):
   evidence=r/'tests/bridge_refactor_evidence/tx_audio_startup_constructor';evidence.mkdir(parents=True,exist_ok=True);report.pop('artifacts',None);(evidence/(label+'.json')).write_text(json.dumps(report,indent=2)+'\n')
   for log in artifact.glob('*.log'):
    match=re.search(r'Application identity: (Decodium - BridgePairimmediate(?:TX|RX)[0-9a-f]{8} - test)',log.read_text(errors='replace'))
    if match:
     for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
      target=parent/match.group(1)
      if target.is_dir():shutil.rmtree(target)
   shutil.rmtree(artifact)
 if not all(timing_results):raise RuntimeError("FT8 timing check failed in one or more variants; see saved evidence")
finally:
 main.write_text(m);module.write_text(original)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
