#!/usr/bin/env python3
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/wspr_callback_before.inc').read_text();sig='void DecodiumBridge::onWsprDecodeReady('
s=(R/'src/bridge/DecodiumBridgeWsprCallback.cpp').read_text();b=sig+s.split(sig,1)[1]
for new,old in [('decodiumBridgeDecodeFlowLog(', 'bridgeLog('),('decodiumBridgeDedupKey(', 'decodeDedupKey(')]:b=b.replace(new,old)
if a!=b:raise SystemExit('WSPR callback differs from pre-extraction snapshot')
print('WSPR callback matches pre-extraction source modulo existing logging and dedup wrappers')
