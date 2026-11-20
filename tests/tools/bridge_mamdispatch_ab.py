#!/usr/bin/env python3
"""Verify that MAM dispatch and elastic sequencing are unchanged."""
from pathlib import Path
r=Path(__file__).resolve().parents[2]
old=(r/'tests/bridge_refactor_fixtures/mamdispatch_before.inc').read_text()
new='bool DecodiumBridge::foxElasticActive() const'+(r/'src/bridge/DecodiumBridgeMamDispatch.cpp').read_text().split('bool DecodiumBridge::foxElasticActive() const',1)[1]
for a,b in (('decodiumBridgeMamQueueLog(', 'bridgeLog('),('decodiumBridgeTxPeriodIsEven(', 'bridgeTxPeriodIsEven('),('decodiumBridgeLatestD3SyncStartMs(', 'latestD3CompatibleSyncTxStartMs(')):new=new.replace(a,b)
if old!=new:raise SystemExit('MAM dispatch extraction differs')
print('MAM dispatch source A/B identical; see runtime A/B lab')
