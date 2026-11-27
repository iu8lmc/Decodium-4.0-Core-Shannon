# Sequencing and decode module evidence review

Existing results only; no test rerun. Indirect runtime results do not establish every branch or physical RF.

| Module | Retained evidence | Scope |
| --- | --- | --- |
| `DecodiumBridgeAutoCqCadence.cpp` | `autocq-cadence-v1.0.737-macos.txt`, `autocq-cadence-refactor-macos.txt` | Two-app virtual CQ cadence; earlier silent-input failure retained. |
| `DecodiumBridgeAutoLogSnapshot.cpp` | `autologsnapshot-full-ab.txt`, `autologsnapshot-app-followup.txt` | Synthetic snapshot and QSO prompt, no committed QSO. |
| `DecodiumBridgeAutoRxLevel.cpp` | `autorx-full-ab.txt`, `autorx-runtime.txt` | Controlled gain logic and two-app virtual gain reduction. |
| `DecodiumBridgeCpuProfile.cpp` | `cpuprofile-full-ab.txt`, `cpuprofile-runtime-ab.txt` | Thread limits and low-CPU settings; no cross-platform runtime. |
| `DecodiumBridgeDecodeBacklog.cpp` | `backlog-harness-ab.txt`, `backlog-runtime-ab.txt` | Queued work harness and app mode transitions; active worker covered separately. |
| `DecodiumBridgeDecodePersistence.cpp` | `decodepersistence-harness-ab.txt`, `decodepersistence-runtime-ab.txt` | Synthetic database persistence and closed session. |
| `DecodiumBridgeDecodeShutdown.cpp` | `decodeshutdown-ab.txt`, `decodeshutdown-runtime.txt` | Controlled shutdown and application state; active worker result separate. |
| `DecodiumBridgeDecodeUiSchedule.cpp` | `decodeuischedule-ab.txt`, `decodeuischedule-runtime.txt` | Scheduling harness and application state; not a rendered visual check. |
