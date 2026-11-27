# TX module evidence review

Existing results reviewed; no execution. App evidence with virtual audio or synthetic collaborators does not prove every branch or physical RF.

| Module | Retained files | Scope / limitation |
| --- | --- | --- |
| `DecodiumBridgeTxSelection.cpp` | `txselection/before.json`, `txselection/after.json` | matching six-slot app states |
| `DecodiumBridgeWavIO.cpp` | `wavio/FT8-before.json`, `wavio/FT8-after.json` | matching FT2/FT4/FT8 WAV hashes; int writer tested by harness |
| `DecodiumBridgeTxWave.cpp` | `txwave/real-modes-ctest.txt`, `txwave/FT8-before.json`, `txwave/FT8-after.json` | real encoders A/B plus virtual audio |
| `DecodiumBridgeTxTimingMath.cpp` | `txtimingmath/ctest.txt` | real timing helpers in waveform A/B |
| `DecodiumBridgeTxPolicy.cpp` | `txpolicy/before.json`, `txpolicy/after.json`, `txlead/before.json`, `txlead/after.json` | matching virtual FT2 and controlled timing |
| `DecodiumBridgeTxPcm.cpp` | `txpcm/ctest.txt`, `txpcm/before.json`, `txpcm/after.json` | 192 format combinations A/B and virtual app result |
| `DecodiumBridgeTxFormat.cpp` | `txformat/ctest.txt`, `txformat/before.json`, `txformat/after.json` | format selection A/B and virtual application |
| `DecodiumBridgeSatelliteHalfDuplex.cpp` | `satellitehd/guards-ab.txt`, `satellitehd/before.json`, `satellitehd/after.json` | simulated CAT guards and disconnected app state |
| `DecodiumBridgeTxScheduling.cpp` | `txscheduling-runtime.txt`, `txscheduling-full-ab.txt` | controlled scheduling and two-app virtual audio |
| `DecodiumBridgeQsyGuard.cpp` | `qsyguard-ab.txt`, `qsyguard-runtime.txt` | guard A/B; virtual-audio app does not exercise CAT guard itself |
| `DecodiumBridgeStartTxSelection.cpp` | `start_tx/before.json`, `start_tx/after.json` | virtual start-TX pair; specific selection-branch app output now retained in `start-tx-selection-current-ab.txt` |
