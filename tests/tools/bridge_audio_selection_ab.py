#!/usr/bin/env python3
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/audio_selection_before.inc').read_text();sig='void DecodiumBridge::setAudioInputDevice('
s=(R/'src/bridge/DecodiumBridgeAudioSelection.cpp').read_text();b=sig+s.split(sig,1)[1]
for new,old in [('decodiumBridgeResolveAudioDeviceByIdentity(', 'tryResolveAudioDeviceByIdentity('),('decodiumBridgeAudioDeviceIdForSettings(', 'audioDeviceIdForSettings('),('decodiumBridgeAudioDeviceIdSettingForLog(', 'audioDeviceIdSettingForLog('),('decodiumBridgeAudioDeviceSelectionForLog(', 'audioDeviceSelectionForLog('),('decodiumBridgeLinuxPulseSourceNameFromDisplay(', 'linuxPulseSourceNameFromDisplay('),('decodiumBridgeLinuxSetDefaultPulseSource(', 'linuxSetDefaultPulseSource('),('decodiumBridgeLinuxPulseSinkNameFromDisplay(', 'linuxPulseSinkNameFromDisplay('),('decodiumBridgeLinuxSetDefaultPulseSink(', 'linuxSetDefaultPulseSink('),('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')]:b=b.replace(new,old)
if a!=b:raise SystemExit('audio selection differs')
print('Audio selection methods match pre-extraction snapshot modulo existing and forwarding wrappers')
