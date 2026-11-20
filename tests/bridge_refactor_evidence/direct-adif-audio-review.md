# Direct-reference review: ADIF and audio

Recorded test evidence inspected; no rerun. A module-level row does not prove every method or every branch.

| Module | Evidence | Scope and limitation |
| --- | --- | --- |
| `DecodiumBridgeAdifEditing.cpp` | `adif-edit-baseline-a.txt`, `adif-edit-refactor-a.txt` | synthetic edit/export, not user logbook |
| `DecodiumBridgeAdifImport.cpp` | `adif-import-wrapper-ab.txt` | isolated synthetic import/export with dedup |
| `DecodiumBridgeAmplifierConfig.cpp` | `amplifierconfig/config-ab.txt`, `amplifierconfig/runtime-ab.txt` | simulated amp settings, not hardware |
| `DecodiumBridgeAudioAdapters.cpp` | `audio-adapters-runtime-current-ab.txt` | isolated app A/B with virtual device |
| `DecodiumBridgeAudioBuffers.cpp` | `audiobuffers-full-ab.txt`, `audiobuffers-runtime.txt` | controlled buffer harness, virtual audio |
| `DecodiumBridgeAudioCaptureStart.cpp` | `legacy-audiocapture-repeat-ab.txt` | non-silent virtual RX recording, not RF |
| `DecodiumBridgeAudioDeviceIdentity.cpp` | `audioidentity/ab.txt`, `audioidentity/runtime.txt` | virtual device identity, not physical hot-plug |
| `DecodiumBridgeAudioDeviceInit.cpp` | `audio_device_init/before.json`, `audio_device_init/after.json` | virtual FT8 and initializer observation |
