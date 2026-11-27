# Map and visual module evidence review

Existing results reviewed; no test rerun. Synthetic/WAV/model results do not demonstrate rendered pixels or RF.

| Module | Retained evidence | Scope and limit |
| --- | --- | --- |
| `DecodiumBridgeWorldMapEntry.cpp` | `worldmapentry-ab.txt`, `worldmapentry-runtime.txt`, `worldmapreplay-wav-isolated-ab.txt` | CQ map contact; no rendered-pixel comparison |
| `DecodiumBridgeWorldMapGrid.cpp` | `worldmapgrid-ab.txt`, `worldmapgrid-runtime.txt` | temporary CALL3 and grid lookup; map contact only in app |
| `DecodiumBridgeWorldMapQso.cpp` | `worldmapqso-ab.txt`, `worldmapqso-runtime.txt` | QSO path in harness; full live QSO not demonstrated |
| `DecodiumBridgeWorldMapQueue.cpp` | `worldmapqueue-ab.txt`, `worldmapqueue-runtime.txt` | queue and filter harness; WAV-derived app contact |
| `DecodiumBridgeWorldMapReset.cpp` | `worldmapreset-ab.txt`, `worldmapreset-runtime.txt` | reset harness and WAV application signal |
| `DecodiumBridgeVisualFeedScheduling.cpp` | `visualfeedscheduling-ab.txt`, `visualfeedscheduling-runtime.txt` | queue/timer harness and WAV app map contact |
| `DecodiumBridgeFullSpectrumSnapshot.cpp` | `fullspectrumsnapshot-ab.txt`, `fullspectrumsnapshot-runtime.txt` | model handoff and WAV-derived model rows |
| `DecodiumBridgeUiPreferences.cpp` | `uiprefs-baseline-a.txt`, `uiprefs-refactor-a.txt`, `uiprefs-restart-baseline.txt`, `uiprefs-restart-refactor.txt` | settings and isolated restart; appearance not compared |
