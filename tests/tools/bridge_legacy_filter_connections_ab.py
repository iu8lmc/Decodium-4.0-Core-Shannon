#!/usr/bin/env python3
"""Check exact retained bodies and initialisation order of legacy filter connections."""
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=(r/'src/bridge/DecodiumBridgeConstructor.cpp').read_text();module=(r/'src/bridge/DecodiumBridgeLegacyFilterConnections.cpp').read_text()
first=(r/'tests/bridge_refactor_fixtures/legacy_cq_filter_connections_before.inc').read_text();second=(r/'tests/bridge_refactor_fixtures/legacy_mycall_filter_connection_before.inc').read_text()
expected='#include "DecodiumBridge.h"\n#include "DecodiumLegacyBackend.h"\n\nvoid DecodiumBridge::initialiseLegacyCqFilterConnections()\n{\n'+first+'}\n\nvoid DecodiumBridge::initialiseLegacyMyCallFilterConnection()\n{\n'+second+'}\n'
assert module==expected
assert main.index('    initialiseLegacyCqFilterConnections();')<main.index('    initialiseReadyProfileConnections();')<main.index('    initialiseLegacyMyCallFilterConnection();')<main.index('    initialiseDecodeClusterConnections();')
print('Legacy CQ and MyCall filter connections: identical bodies and original constructor ordering')
