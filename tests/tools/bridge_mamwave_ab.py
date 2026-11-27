#!/usr/bin/env python3
"""Check that multi-stream waveform generation was moved unchanged."""
from pathlib import Path
r=Path(__file__).resolve().parents[2]
old=(r/'tests/bridge_refactor_fixtures/mamwave_before.inc').read_text().replace('static QVector<float> generateMultiStreamFtxWave(', 'QVector<float> generateMultiStreamFtxWave(',1).replace('QString* errorOut = nullptr,','QString* errorOut,',1).replace('QVector<float> const& gains = {})','QVector<float> const& gains)',1)
new=('QVector<float> generateMultiStreamFtxWave('+(r/'src/bridge/DecodiumBridgeMamWave.cpp').read_text().split('QVector<float> generateMultiStreamFtxWave(',1)[1]).split('\nvoid decodiumBridgeMamQueueLog(',1)[0]
if old!=new:raise SystemExit('MAM waveform extraction differs')
print('MAM waveform source A/B identical; see WAV runtime comparison')
