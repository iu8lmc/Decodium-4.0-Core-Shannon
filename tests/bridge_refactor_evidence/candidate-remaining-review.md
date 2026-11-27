# Candidate indirect-call review: remaining modules

Existing results and world-clock follow-up; no further rerun. A reference does not prove every method or all branches.

| Module | Evidence | Scope and limitation |
| --- | --- | --- |
| `DecodiumBridgeCallerQueue.cpp` | `full-ab-20261110.log`, `caller_scoring/callerscore-before-a.txt` | controlled queue and synthetic scoring, no RF |
| `DecodiumBridgeDecodeClick.cpp` | `decode_click/click-before-a.txt`, `decode_click/click-after-a.txt` | synthetic decode selections |
| `DecodiumBridgeDecodeDedup.cpp` | `full-ab-20261110.log`, `decode-dedup-ft8-loopback-macos.json` | controlled index plus virtual FT8 |
| `DecodiumBridgeDecodeMetrics.cpp` | `full-ab-20261110.log`, `overlay-runtime.txt` | synthetic counters; timing accuracy not established |
| `DecodiumBridgeDecodeTiming.cpp` | `full-ab-20261110.log`, `decode-timing-ft8-matched-baseline.txt`, `decode-timing-ft8-matched-refactor.txt` | matched virtual FT8; earlier failed attempts retained |
| `DecodiumBridgeDecodeWorkers.cpp` | `workers-full-ab.txt`, `workers-runtime-ab.txt` | worker creation and synthetic mode changes |
| `DecodiumBridgePanadapterFft.cpp` | `panfft-app-baseline.txt`, `panfft-app-refactor.txt` | synthetic FFT; pixels not compared |
| `DecodiumBridgePartnerMemory.cpp` | `partner-full-ab.txt`, `partner-runtime-ab.txt` | synthetic settings/state, no live QSO |
| `DecodiumBridgePortSettings.cpp` | `port_settings/port-before-a.txt`, `port_settings/port-after-a.txt` | isolated ports/settings, no physical rig |
| `DecodiumBridgeQsoCache.cpp` | `qsocache-baseline-a.txt`, `qsocache-refactor-a.txt` | synthetic QSO cache |
| `DecodiumBridgeRxTerminal.cpp` | `rxterminal-ab.txt`, `rxterminal-runtime.txt` | substituted process launch, app availability |
| `DecodiumBridgeSatelliteOptions.cpp` | `satellite-options-baseline.txt`, `satellite-options-refactor.txt` | synthetic option lists; no satellite hardware |
| `DecodiumBridgeSpectrumVisibility.cpp` | `spectrumvisibility-ab.txt`, `spectrumvisibility-runtime.txt` | controlled visibility and app model state |
| `DecodiumBridgeTxSlot.cpp` | `txslot/before.json`, `txslot/after.json` | synthetic slot selection, CAT/TX disabled |
| `DecodiumBridgeWorldClock.cpp` | `full-ab-20261110.log`, `world-clock-current-runtime.txt` | source A/B and current app; actual baseline app not rerun |
