#!/usr/bin/env python3
"""Compare the extracted integer/float WAV writers and the MAM dump method."""
from pathlib import Path
r=Path(__file__).resolve().parents[2]
old=(r/'tests/bridge_refactor_fixtures/wavio_before.inc').read_text().replace('static bool writeMono16WavFile(', 'bool writeMono16WavFile(').replace('ensureParentDirectoryForFile(', 'decodiumBridgeEnsureParentDirectoryForFile(')
new='bool writeMono16WavFile('+(r/'src/bridge/DecodiumBridgeWavIO.cpp').read_text().split('bool writeMono16WavFile(',1)[1]
if old!=new:raise SystemExit('WAV writer extraction differs')
old=(r/'tests/bridge_refactor_fixtures/mamwavdump_before.inc').read_text().replace('bridgeLog(', 'decodiumBridgeMamQueueLog(')
new='bool DecodiumBridge::mamDumpTestWav('+(r/'src/bridge/DecodiumBridgeMamWave.cpp').read_text().split('bool DecodiumBridge::mamDumpTestWav(',1)[1]
if old!=new:raise SystemExit('MAM WAV dump extraction differs')
print('WAV writers and MAM dump source A/B identical')
