# Candidate indirect-call review: MAM, SSTV and TX preflight

Existing results only; no rerun. Simulated/local tests do not establish physical RF, third-party interoperability or every branch.

| Module | Evidence | Scope and limitation |
| --- | --- | --- |
| `DecodiumBridgeMamControls.cpp` | `mam-controls-baseline-a.txt`, `mam-controls-refactor-a.txt` | synthetic settings in isolated app |
| `DecodiumBridgeMamFrequencyPlan.cpp` | `mamplan-baseline-a.txt`, `mamplan-refactor-a.txt` | synthetic mode/status, not on-air |
| `DecodiumBridgeMamQueue.cpp` | `mamqueue/before.json`, `mamqueue/after.json` | synthetic queue state |
| `DecodiumBridgeMamWave.cpp` | `mamwave/FT8-before.json`, `mamwave/FT8-after.json` | virtual waveform, not RF |
| `DecodiumBridgeSstvControls.cpp` | `sstv-helpers-controls-repeat-ab.txt` | synthetic controls/profiles |
| `DecodiumBridgeSstvProducerTaps.cpp` | `hamdrm-taps-repeat-ab.txt`, `hamdrm-taps-monitor-off-ab.txt` | two-app virtual transfer; not EasyPal |
| `DecodiumBridgeSstvStorageState.cpp` | `sstv-storage-v1.0.737-macos.txt`, `sstv-storage-refactor-macos.txt` | isolated storage state, Qt versions differ |
| `DecodiumBridgeSstvTxState.cpp` | `sstvtxstate-baseline-a.txt`, `sstvtxstate-refactor-a.txt` | idle diagnostics; no PTT |
| `DecodiumBridgeStartTxPreflight.cpp` | `start-tx-preflight-notices-ab.txt`, `start_tx/before.json`, `start_tx/after.json` | guard notifications and virtual FT8; no physical rig |
