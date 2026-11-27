#!/usr/bin/env python3
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/generic_settings_before.inc').read_text()
s=(R/'src/bridge/DecodiumBridgeGenericSettings.cpp').read_text();sig='static QString canonicalProfileServiceSettingKey('
b=sig+s.split(sig,1)[1]
for new,old in [('decodiumBridgeAliasedSettingKey(', 'aliasedBridgeSettingKey('),('decodiumBridgeIsHamlibFamilyBackend(', 'isHamlibFamilyBackend('),('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')]:b=b.replace(new,old)
if a!=b:raise SystemExit('generic settings differs from pre-extraction code')
print('Generic settings: helper, getter and setter match pre-extraction source with existing wrappers')
