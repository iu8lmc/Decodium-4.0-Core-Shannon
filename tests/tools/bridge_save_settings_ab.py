#!/usr/bin/env python3
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/save_settings_before.inc').read_text();sig='void DecodiumBridge::saveSettings()'
s=(R/'src/bridge/DecodiumBridgeSaveSettings.cpp').read_text();b=sig+s.split(sig,1)[1]
for new,old in [('decodiumBridgeSettingsWriteThreadPool(', 'settingsWriteThreadPool('),('decodiumBridgeCanonicalApplicationDecodeMode(', 'canonicalApplicationDecodeMode('),('decodiumBridgeAudioChannelSettingValue(', 'audioChannelSettingValue('),('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')]:b=b.replace(new,old)
if a!=b:raise SystemExit('save settings differs')
print('Sync and async settings methods match pre-extraction source modulo forwarding wrappers')
