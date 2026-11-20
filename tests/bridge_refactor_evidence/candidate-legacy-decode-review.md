# Candidate indirect-call review: decode and legacy

Existing results only; no rerun. Synthetic and virtual results do not prove RF or all branches.

| Module | Evidence | Scope and limitation |
| --- | --- | --- |
| `DecodiumBridgeDecodeIndicators.cpp` | `decodeindicators-ab.txt`, `decodeindicators-runtime.txt` | indicator model and signals, not visual pixels |
| `DecodiumBridgeDecodeRowParsers.cpp` | `full-ab-20261110.log`, `decoderows-ft8-refactor.txt` | parser A/B and FT8 app; WSPR independent evidence in wspr_callback |
| `DecodiumBridgeDecodeSemantic.cpp` | `semantic-ab.txt`, `semantic-runtime.txt` | controlled classification and synthetic legacy decode |
| `DecodiumBridgeFilterControls.cpp` | `filtercontrols-ab.txt`, `filtercontrols-runtime.txt` | filter controls, synthetic app state |
| `DecodiumBridgeLegacyAudio.cpp` | `legacy_audio/legacy-audio-before-a.txt`, `legacy_audio/legacy-audio-after-a.txt` | synthetic PCM and recorded virtual samples |
| `DecodiumBridgeLegacyDecodeDelta.cpp` | `legacydelta-harness-ab.txt`, `legacydelta-pair-ab.txt` | first baseline attempt timed out; retry and refactor decoded |
| `DecodiumBridgeLegacyMirror.cpp` | `legacymirror-ab.txt`, `legacymirror-runtime.txt` | mirror simulation and isolated app state |
| `DecodiumBridgeLegacyOwnership.cpp` | `legacyownership-ab.txt`, `legacyownership-runtime.txt` | controlled platform routing and virtual RX |
| `DecodiumBridgeLegacyWaterfall.cpp` | `legacy_waterfall/before.json`, `legacy_waterfall/after.json` | synthetic rows; rendered waterfall not compared |
