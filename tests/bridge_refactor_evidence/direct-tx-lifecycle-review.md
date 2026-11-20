# Direct-reference review: TX lifecycle

Existing results inspected; no rerun. Virtual audio and synthetic guards do not prove every branch, actual hardware PTT or RF.

| Module | Evidence | Scope and limitation |
| --- | --- | --- |
| `DecodiumBridgeCompleteTxPlayback.cpp` | `complete-tx-guards-ab.txt`, `complete-tx-hold-timer-repeat-ab.txt` | synthetic completion guards/timer |
| `DecodiumBridgeHalt.cpp` | `halt/direct-fix-cw-ab.txt`, `halt/direct-fix-cw-audio.txt` | intentional HALT correction; audio follow-up current version |
| `DecodiumBridgeModulatorFinish.cpp` | `modulator_finish/before.json`, `modulator_finish/after.json`, `cw-natural-modulator-fix-ab.txt` | virtual FT8, intentional CW correction |
| `DecodiumBridgeStartTx.cpp` | `start_tx/before.json`, `start_tx/after.json`, `start-tx-selection-current-ab.txt` | virtual FT8 and synthetic selection guards |
| `DecodiumBridgeStopTx.cpp` | `stop-tx-routes-repeat-ab.txt` | routing with intercepted collaborators; audio follow-up in main report |
| `DecodiumBridgeTxAudioStartup.cpp` | `tx_audio_startup_constructor/before.json`, `tx_audio_startup_constructor/after.json` | virtual FT8; idle callback not observed there |
| `DecodiumBridgeTxOutputDevice.cpp` | `tx_output_device/before.json`, `tx_output_device/after.json` | virtual output device and FT8 |
| `DecodiumBridgeTxPrecompute.cpp` | `txprecompute-wave-hash-ab.txt` | FT2 sample/hash and cache A/B |
| `DecodiumBridgeTxPrepare.cpp` | `txprepare/single-before.json`, `txprepare/single-after.json` | synthetic preparation; other modes in same directory |
