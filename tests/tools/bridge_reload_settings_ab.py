#!/usr/bin/env python3
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/reload_settings_before.inc').read_text();sig='void DecodiumBridge::reloadBridgeSettingsFromPersistentStore()'
s=(R/'src/bridge/DecodiumBridgeLoadSettings.cpp').read_text();b=sig+s.split(sig,1)[1]
for new,old in [('decodiumBridgeLegacyRigControlFallback(', 'useLegacyRigControlFallback('),('decodiumBridgeIsHamlibFamilyBackend(', 'isHamlibFamilyBackend('),('decodiumBridgeRxInputGainFromLevel(', 'rxInputGainFromLevel('),('decodiumBridgeTxAttenuationFromSlider(', 'txAttenuationFromSlider('),('decodiumBridgeTxGainFromSlider(', 'txGainFromSlider('),('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')]:b=b.replace(new,old)
if a!=b:raise SystemExit('reload settings differs')
print('Reload settings matches pre-extraction snapshot modulo existing forwarding wrappers')
