# Direct-reference review: audio and CAT

Existing results only; no rerun. Virtual and simulated paths do not prove physical CAT, PTT, radio or every branch.

| Module | Evidence | Scope |
| --- | --- | --- |
| `DecodiumBridgeAudioFrequencies.cpp` | `audiofrequencies/frequencies-ab.txt`, `audiofrequencies/runtime-ab.txt` | synthetic audio frequency settings |
| `DecodiumBridgeAudioIdentity.cpp` | `audioidentity/ab.txt`, `audioidentity/runtime.txt` | virtual-device identity |
| `DecodiumBridgeAudioRecovery.cpp` | `audiorecovery-full-ab.txt`, `audiorecovery-runtime.txt` | mode-change recovery with virtual audio |
| `DecodiumBridgeAudioRuntimeChange.cpp` | `audioruntimechange/ab.txt`, `audioruntimechange/runtime.txt` | synthetic routing changes |
| `DecodiumBridgeCatBackendSwitch.cpp` | `catbackendswitch/switch-ab.txt`, `catbackendswitch/runtime-ab.txt` | simulated backend state |
| `DecodiumBridgeCatConnections.cpp` | `cat-rigctl-constructor-ab.txt`, `cat_connections_constructor/connect.txt` | local dummy rig and simulated callbacks |
| `DecodiumBridgeCatQsy.cpp` | `catqsy/ab.txt`, `catqsy/runtime.txt` | synthetic frequency, CAT disconnected |
| `DecodiumBridgeCatRecovery.cpp` | `catrecovery/ab.txt`, `catrecovery/runtime.txt` | simulated guard/reconnect |
| `DecodiumBridgeCatRigMode.cpp` | `catrigmode/ab.txt`, `catrigmode/runtime.txt` | mocked backend and disconnected state |
| `DecodiumBridgeCatSplitSync.cpp` | `catsplitsync/ab.txt`, `catsplitsync/runtime.txt` | simulated split state |
| `DecodiumBridgeCatTelemetry.cpp` | `cat-telemetry-constructor-current-ab.txt` | fake telemetry with CAT/TX off |
