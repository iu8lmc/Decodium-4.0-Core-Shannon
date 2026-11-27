# Legacy and decode module evidence review

Existing results only, no test rerun. A runtime summary does not independently establish every function or simulated branch.

| Module | Retained evidence | Scope and limit |
| --- | --- | --- |
| `DecodiumBridgeLegacyAdifMirror.cpp` | `legacyadifmirror-ab.txt`, `legacyadifmirror-realfile-ab.txt` | two-placement real temporary ADIF files; bridge collaborators substituted |
| `DecodiumBridgeLegacyDialogSync.cpp` | `legacydialogsync-ab.txt`, `legacydialogsync-runtime.txt` | recorded call order; virtual audio does not validate physical CAT |
| `DecodiumBridgeLegacyPcmRecovery.cpp` | `legacypcmrecovery-ab.txt`, `legacypcmrecovery-runtime.txt` | timed recovery with simulated collaborators; virtual legacy audio |
| `DecodiumBridgeLegacyRefresh.cpp` | `legacyrefresh-ab.txt`, `legacyrefresh-runtime.txt` | timer/reentrancy A/B, virtual legacy RX recovery |
| `DecodiumBridgeLegacyRxHandoff.cpp` | `legacyrxhandoff-ab.txt`, `legacyrxhandoff-runtime.txt` | handoff guards A/B, virtual legacy RX recovery |
| `DecodiumBridgeDecodeAlerts.cpp` | `decodealerts-ab.txt`, `decodealerts-runtime.txt` | alert logic with simulated playback; app decode does not prove audible output |
| `DecodiumBridgeDecodeDepth.cpp` | `decode-depth-baseline.txt`, `decode-depth-refactor.txt`, `decode-depth-ft8-baseline.txt`, `decode-depth-ft8-refactor.txt` | setting A/B and virtual FT8, not all modes |
| `DecodiumBridgeDecodeIdentity.cpp` | `decodeidentity-ab.txt`, `decodeidentity-runtime-a.txt`, `decodeidentity-runtime-b.txt` | A/B parsing harness and two synthetic FT8 WAV app checks |
| `DecodiumBridgeDecodeModelFilter.cpp` | `decodemodelfilter-ab.txt`, `decodemodelfilter-runtime.txt` | filter harness and app decode/clear, not every row |
| `DecodiumBridgeDecodeRxAppend.cpp` | `rxappend-ab.txt`, `rxappend-runtime.txt` | dedup/guard harness and app model state |
| `DecodiumBridgeDecodeUiNotifications.cpp` | `decodeuinotifications-ab.txt`, `decodeuinotifications-runtime.txt` | timer harness and app model state, not rendered pixels |
| `DecodiumBridgeDecodeUiSnapshot.cpp` | `decodeuisnapshot-ab.txt`, `decodeuisnapshot-runtime.txt` | synthetic rows and app model population, not every race |
