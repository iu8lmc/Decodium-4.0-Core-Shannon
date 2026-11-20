# Direct-script report-name exceptions

These seven module references were absent from a literal script-name search in the main report; existing artifacts were located separately. No rerun.

| Module | Evidence | Scope / limitation |
| --- | --- | --- |
| `DecodiumBridgeAudioBuffers.cpp` | `audiobuffers-full-ab.txt`, `audiobuffers-runtime.txt` | virtual audio does not observe release callback directly |
| `DecodiumBridgeDecodeSecondary.cpp` | `secondary-harness-ab.txt`, `secondary-isolated-app-ab.txt` | native WAV app; not every legacy side effect |
| `DecodiumBridgeFileIO.cpp` | `fileio-runtime-ab.txt` | synthetic binary read/write in isolated release/current apps |
| `DecodiumBridgeLegacyHash.cpp` | `legacyhash-app-ab.txt` | synthetic ALL.TXT resolution and wrong-mode guard |
| `DecodiumBridgeLoadSettings.cpp` | `load_settings/load-before-a.txt`, `load_settings/load-final-a.txt` | synthetic profiles, not every setting |
| `DecodiumBridgeRemoteActivity.cpp` | `remote-activity-harness-ab.txt`, `remote-activity-websocket-ab.txt` | local authenticated WebSocket; no external peer |
| `DecodiumBridgeWorldMapReplay.cpp` | `worldmapreplay-wav-isolated-ab.txt`, `worldmapreplay-synthetic-app-ab.txt` | synthetic/WAV map signal, no rendered pixels; earlier failed attempt retained |
