#!/usr/bin/env python3
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/load_settings_before.inc').read_text();sig='void DecodiumBridge::loadSettings()'
s=(R/'src/bridge/DecodiumBridgeLoadSettings.cpp').read_text();b=sig+s.split(sig,1)[1]
for new,old in [('decodiumBridgeTruthyEnvironmentFlag(', 'truthyEnvironmentFlag('),('decodiumBridgeCanonicalApplicationDecodeMode(', 'canonicalApplicationDecodeMode('),('decodiumBridgeLegacyDepthBitsToModernDepth(', 'legacyDepthBitsToModernDepth('),('decodiumBridgeNormalizeWsprPowerDbm(', 'normalizeWsprPowerDbm('),('decodiumBridgeFtAutoUiReserveForCores(', 'ftAutoUiReserveForCores('),('decodiumBridgeAutoFtThreadCountForCores(', 'autoFtThreadCountForCores('),('decodiumBridgeAudioDeviceIdSettingForLog(', 'audioDeviceIdSettingForLog('),('decodiumBridgeMaxFtDecodeThreads()', 'kMaxFtDecodeThreads'),('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')]:b=b.replace(new,old)
if a!=b:raise SystemExit('load settings differs')
print('Load settings matches pre-extraction snapshot modulo forwarding wrappers')
constants=(R/'src/bridge/BridgeConfigConstants.h').read_text()
for definition in ('kDefaultTxWatchdogMinutes = 6;', 'kDefaultTxWatchdogCount = 3;', 'kSpecialOpFox = 6;', 'kSpecialOpHound = 7;'):
 if definition not in constants:raise SystemExit('shared setting default changed: '+definition)
print('Shared watchdog and special-operation defaults retain original values')
