#!/usr/bin/env python3
"""Check audio format selection is unchanged apart from exported helpers."""
from pathlib import Path
r=Path(__file__).resolve().parents[2]
old=(r/'tests/bridge_refactor_fixtures/txformat_before.inc').read_text()
for a,b in [('static QString audioFormatToString(', 'QString audioFormatToString('),('static bool sameAudioFormat(', 'bool sameAudioFormat('),('static QAudioFormat chooseTxAudioFormat(', 'QAudioFormat chooseTxAudioFormat('),('bridgeLog(', 'decodiumBridgeAudioLevelLog(')]:old=old.replace(a,b)
new='static QString sampleFormatName('+(r/'src/bridge/DecodiumBridgeTxFormat.cpp').read_text().split('static QString sampleFormatName(',1)[1].split('\nQString audioStateToString(',1)[0]
if old!=new:raise SystemExit('TX format selection differs')
print('TX format source A/B identical; see device and runtime labs')
