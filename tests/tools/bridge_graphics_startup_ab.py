#!/usr/bin/env python3
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/graphics_startup_before.inc').read_text()
s=(R/'src/bridge/DecodiumBridgeGraphicsStartup.cpp').read_text();marker='#ifdef Q_OS_WIN\nstatic QString decodiumSlowQmlStartupFlagPath()';b=marker+s.split(marker,1)[1];b=b.replace('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')
if a!=b:raise SystemExit('startup graphics differs from pre-extraction source')
print('Startup graphics methods and Windows helpers match pre-extraction source with logging wrapper')
