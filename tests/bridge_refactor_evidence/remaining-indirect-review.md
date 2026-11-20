# Remaining indirect-module evidence review

This locates existing evidence, not a declaration that every function has complete app-level A/B proof. No test rerun.

| Module | Existing reference | Limitation |
| --- | --- | --- |
| `DecodiumBridgeAudioSinkRetire.cpp` | `bridge_audio_sink_retire_ab.py`; `audio sink retirement` | BlackHole sink; output only described in main report |
| `DecodiumBridgeCatModes.cpp` | `full suite`; `CAT modes` | controlled CAT backend decisions; no physical rig |
| `DecodiumBridgeDecodeMirrorKeys.cpp` | `bridge_decode_mirror_keys_ab.py`; `decode mirror keys` | model-level synthetic tests |
| `DecodiumBridgeGrid.cpp` | `full suite`; `grid: A/B OK` | controlled grid math; separate map app evidence |
| `DecodiumBridgeKeyboardAudio.cpp` | `full suite`; `keyboard audio: A/B OK` | controlled audio handlers; app coverage needs tracing |
| `DecodiumBridgeLinuxAudioHelpers.cpp` | `bridge_linux_audio_helpers_ab.py`; `Linux audio helpers` | pactl simulation on macOS; no Linux runtime |
| `DecodiumBridgeRtlSdrPlayback.cpp` | `full suite`; `RTL-SDR playback: A/B OK` | simulated input/output, not hardware playback |
| `DecodiumBridgeRtlSdrSamples.cpp` | `full suite`; `RTL-SDR spectrum: A/B OK` | synthetic PCM/IQ and FFT, no RTL hardware |
| `DecodiumBridgeSequenceOptions.cpp` | `sequence_options/seq-before-a.txt`; `Sequencer option extraction` | two synthetic app scenarios; main report |
| `DecodiumBridgeSstvTiming.cpp` | `full suite`; `SSTV timing enabled=0: A/B OK` | enabled/disabled test variants, not RF timing |
| `DecodiumBridgeTxWatchdog.cpp` | `full suite`; `TX watchdog: A/B OK` | controlled watchdog; app integration should be assessed separately |
| `DecodiumBridgeVisualTxCoalesce.cpp` | `bridge_visual_tx_coalesce_ab.py`; `visual TX coalescing` | model-level merge; not RF |
| `DecodiumBridgeWindowState.cpp` | `full suite`; `window state: A/B OK` | controlled state persistence; native window handling not established |

Window-state follow-up: `window-state-current-runtime.txt` records a current
isolated app save/read for synchronous and asynchronous state. The existing
full-suite harness provides before/after comparison; native window appearance
and an application-level baseline are not independently established.
