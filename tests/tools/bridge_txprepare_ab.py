#!/usr/bin/env python3
"""Check TX preparation was moved without changing cache or audio policy."""
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/txprepare_before.inc').read_text()
new='bool DecodiumBridge::ensureTxAudioPrepared('+(r/'src/bridge/DecodiumBridgeTxPrepare.cpp').read_text().split('bool DecodiumBridge::ensureTxAudioPrepared(',1)[1]
for a,b in [('decodiumBridgeTxPreparePcm(wave, format, m_audioOutputChannel)','buildTxPcmBuffer(wave, format, boundedAudioChannel(m_audioOutputChannel))'),('decodiumBridgeTxPrepareTimelineLog(','txTimelineLog('),('decodiumBridgeTxPrepareAudioFormat(','audioFormatToString('),('decodiumBridgeTxPrepareSameFormat(','sameAudioFormat('),('decodiumBridgeTxPrepareChooseFormat(','chooseTxAudioFormat('),('decodiumBridgeAudioDeviceIdSettingForLog(','audioDeviceIdSettingForLog('),('decodiumBridgeAudioDeviceIdForSettings(','audioDeviceIdForSettings('),('decodiumBridgeAudioLevelLog(','bridgeLog(')]:new=new.replace(a,b)
if old!=new:raise SystemExit('TX audio preparation differs')
print('TX audio preparation source A/B identical; see runtime labs')
