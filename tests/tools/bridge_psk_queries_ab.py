#!/usr/bin/env python3
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/psk_queries_before.inc').read_text();sig='void DecodiumBridge::sendPskReporterNow()'
s=(R/'src/bridge/DecodiumBridgePskQueries.cpp').read_text();b=sig+s.split(sig,1)[1];b=b.replace('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')
if a!=b:raise SystemExit('PSK queries differ')
print('PSK query methods match pre-extraction snapshot modulo existing logging wrapper')
