#!/usr/bin/env python3
"""Check MAM decode dispatch survived extraction unchanged."""
from pathlib import Path
r=Path(__file__).resolve().parents[2]
old=(r/'tests/bridge_refactor_fixtures/mamingest_before.inc').read_text()
new='void DecodiumBridge::mamIngestDecode('+(r/'src/bridge/DecodiumBridgeMamIngest.cpp').read_text().split('void DecodiumBridge::mamIngestDecode(',1)[1]
new=new.replace('decodiumBridgeMamQueueLog(', 'bridgeLog(').replace('decodiumBridgeInferPartnerForAutolog(', 'inferPartnerFromDirectedMessage(')
if old!=new:raise SystemExit('MAM decode ingestion differs')
print('MAM decode ingestion source A/B identical; see runtime A/B lab')
