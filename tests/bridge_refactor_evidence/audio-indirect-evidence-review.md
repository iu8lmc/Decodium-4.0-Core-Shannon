# Audio module evidence review

No test was rerun. These five modules had no call-shaped match in the
selected runtime-script index; their previous evidence is documented below.

- `DecodiumBridgeAudioCaptureStop.cpp`: `audiostop-runtime.txt` records a
  focused A/B with substituted audio backends and two-app virtual monitor
  stop/restart.
- `DecodiumBridgeAudioHealth.cpp`: `audiohealth-runtime.txt` records focused
  A/B with substituted callbacks and two-app virtual automatic gain response.
- `DecodiumBridgeAudioDeviceCacheAccess.cpp`: `audiocacheaccess-ab.txt`
  contains the before/after harness output; `audiocacheaccess-runtime.txt`
  reports a two-app legacy virtual-audio scenario.
- `DecodiumBridgeAudioDeviceCacheRefresh.cpp`: `audiocacherefresh-ab.txt`
  contains the before/after harness output with simulated device changes;
  `audiocacherefresh-runtime.txt` records the legacy virtual-audio scenario.
- `DecodiumBridgeAudioInputSelection.cpp`: `audioinputselection-ab.txt`
  contains the before/after selection harness output;
  `audioinputselection-runtime.txt` records the virtual-audio scenario.

The saved runtime summaries do not expose all individual method calls, and
the simulated Linux cases do not establish a Linux runtime result or physical
hot-plug. The detailed scenarios are described in `tests/BRIDGE_REFACTOR_AB.md`.
