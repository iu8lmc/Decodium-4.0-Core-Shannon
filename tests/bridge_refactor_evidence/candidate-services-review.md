# Candidate indirect-call review: services and reception

Existing results only; no rerun. Simulated and local service results do not prove external deployment, network interoperability, RF or every branch.

| Module | Evidence | Scope and limitation |
| --- | --- | --- |
| `DecodiumBridgeStationTelemetry.cpp` | `telemetry-ab.txt`, `telemetry-runtime.txt` | synthetic state and FT2 rejection, no external service |
| `DecodiumBridgeStationWeather.cpp` | `stationweather-ab.txt`, `stationweather-runtime.txt` | network simulation and disabled app state |
| `DecodiumBridgeTeleprinter.cpp` | `teleprinter-full-ab.txt`, `teleprinter-runtime.txt` | synthetic RTTY/JTTY rows, not RF |
| `DecodiumBridgeUpdates.cpp` | `updates-full-ab.txt`, `updates-runtime-ab.txt` | disabled update branch only |
| `DecodiumBridgeWavDecode.cpp` | `wav-decode-baseline-a.txt`, `wav-decode-refactor-a.txt` | isolated WAV decode, not live audio |
| `DecodiumBridgeWebServer.cpp` | `webserver-full-ab.txt`, `webserver-runtime-ab.txt` | local service auth/restart, no public deployment |
| `DecodiumBridgeWorkingFrequencies.cpp` | `workingfreq-baseline.txt`, `workingfreq-refactor.txt`, `workingfreq-real-ab.txt` | synthetic station frequency settings |
| `DecodiumBridgeWorldMapConsumers.cpp` | `mapconsumers-baseline-a.txt`, `mapconsumers-refactor-a.txt` | consumer state, not rendered map |
| `DecodiumBridgeWorldMapSelection.cpp` | `worldmapselection-ab.txt`, `worldmapselection-runtime.txt` | controlled selection; runtime map model only |
| `DecodiumBridgeWsprCallback.cpp` | `wspr_callback/wspr-before-a.txt`, `wspr_callback/wspr-after-a.txt` | synthetic callback, not physical WSPR |
