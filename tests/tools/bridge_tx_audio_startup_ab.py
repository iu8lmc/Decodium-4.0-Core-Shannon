#!/usr/bin/env python3
"""Verify startup connection bodies and constructor ordering."""
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=(r/'src/bridge/DecodiumBridgeConstructor.cpp').read_text();module=(r/'src/bridge/DecodiumBridgeTxAudioStartup.cpp').read_text();fixture=(r/'tests/bridge_refactor_fixtures/tx_audio_startup_before.inc').read_text();assert module.split('void DecodiumBridge::initialiseTxAudioOutput()\n{\n',1)[1]==fixture+'}\n';assert main.index('    initialiseUiStartupSettings();')<main.index('    initialiseTxAudioOutput();')<main.index('    loadSettings();');print('TX audio startup: identical connection bodies and constructor ordering')
