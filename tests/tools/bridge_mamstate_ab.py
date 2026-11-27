#!/usr/bin/env python3
"""Compare MAM activation and slot-message methods before and after extraction."""
from pathlib import Path
r=Path(__file__).resolve().parents[2]
old=(r/'tests/bridge_refactor_fixtures/mamstate_before.inc').read_text()
new='bool DecodiumBridge::multiStreamActive() const'+(r/'src/bridge/DecodiumBridgeMamState.cpp').read_text().split('bool DecodiumBridge::multiStreamActive() const',1)[1].split('\nvoid DecodiumBridge::setMultiAnswerMode(bool v)',1)[0]
new=new.replace('decodiumBridgeMamQueueLog(', 'bridgeLog(')
if old!=new:raise SystemExit('MAM state extraction differs')
print('MAM state source A/B identical; see runtime labs')
