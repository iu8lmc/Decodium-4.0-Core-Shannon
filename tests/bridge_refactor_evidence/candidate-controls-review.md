# Candidate indirect-call review: controls and audio

Existing evidence only. Matching text and report summaries are not exhaustive method coverage; hardware/network and unobserved branches remain outside scope. No rerun.

| Module | Evidence | Scope |
| --- | --- | --- |
| `DecodiumBridgeAdvancedDecode.cpp` | `advanced-decode-baseline-a.txt`, `advanced-decode-refactor-a.txt` | synthetic decode settings |
| `DecodiumBridgeAlcCalibration.cpp` | `alc_calibration/alc-before-a.txt`, `alc_calibration/alc-after-a.txt` | simulated CAT/calibration state |
| `DecodiumBridgeAppInfo.cpp` | `appinfo-ab.txt`, `appinfo-runtime.txt` | mocked desktop opening; app diagnostics |
| `DecodiumBridgeAudioLevels.cpp` | `audiolevels-full-ab.txt`, `audiolevels-runtime.txt` | synthetic levels; virtual audio |
| `DecodiumBridgeAudioSelection.cpp` | `audio_selection/audio-select-before-a.txt`, `audio_selection/audio-select-after-a.txt` | settings and virtual device selection |
| `DecodiumBridgeAutoCqDedupe.cpp` | `autocqdedupe-baseline-a.txt`, `autocqdedupe-refactor-a.txt` | synthetic duplicate detection |
| `DecodiumBridgeCatSplitMath.cpp` | `cat_split/catsplit-before-a.txt`, `cat_split/catsplit-after-a.txt` | synthetic split/CAT, no rig |
| `DecodiumBridgeCpuPressure.cpp` | `cpupressure-harness-ab.txt`, `cpupressure-runtime-ab.txt` | synthetic pressure and app limits |
| `DecodiumBridgeCwMonitorSettings.cpp` | `cwsettings-ab.txt`, `cwsettings-audio-runtime.txt` | settings and virtual sidetone |
| `DecodiumBridgeCwRig.cpp` | `cwtune-ab.txt`, `cwtune-runtime.txt` | synthetic/disconnected rig |
| `DecodiumBridgeDecodeClear.cpp` | `decodeclear-harness-ab.txt`, `decodeclear-runtime-ab.txt` | controlled clear and app model state |
