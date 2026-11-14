# Decodium4 performance baseline

## Contract

Compare the same configuration before and after each optimisation. Preserve the
existing Low CPU profile and its intentional decoder trade-offs. Improvements
must also benefit that profile without changing its behaviour. Do not count
lower decode depth, narrower search, omitted passes or fewer decoded messages as
performance improvements.

Source baseline: `3529dae3e218ef978b77347795572d96a39e9a55`.
This first stage measures startup/idle and inventories lifecycle ownership. It
does not establish decode equivalence, old-hardware performance or RF operation.

## Repeatable startup/idle measurement

```sh
cmake --build build --target decodium_qml -j 2
python3 tests/tools/measure_runtime_baseline.py --output build/performance-baseline/run-01
```

The harness requires macOS. It alternates requested native and legacy backends
over three repetitions, uses fresh isolated bridge settings and a random test
identity, disables CAT/monitoring, and allows only loopback network traffic.
Legacy defaults may be inherited from `.qttest/Library/Preferences/decodium4.ini`;
its before/after fingerprints are recorded and its original contents are restored
after each run. It does not edit source or QML.
It retains executable SHA-256, source revision/status, raw logs, startup event
arrival times and main-process RSS/CPU samples. Run without compilation or other
heavy work in parallel. Keep the window visible and avoid user interaction.

Interpretation:

- These are fresh-profile launches, not controlled cold filesystem-cache boots.
- Event times are observed log-arrival times; internal startup timers are also
  retained. A visible loader is not equivalent to a ready main window or audio.
- RSS is sampled every 500 ms and is not private allocated memory. `ps %cpu` is
  an OS estimate, not an instantaneous interval counter; 100% means one core.
- Samples exclude child processes. A requested backend must be checked against
  its log; missing readiness or CAT isolation marks a run invalid.
- External networking is denied for repeatability: connected services need a separate
  representative run. Default test settings do not represent a large logbook.
- Do not run other Decodium instances alongside this harness: the current
  asynchronous diagnostic logger uses a shared location. Only the segment for
  the invocation's random identity is copied into its evidence directory.
- `--headless --runs 1 --seconds 10` validates the harness without opening visible
  windows. Its software-rendered timings are not a GUI performance baseline.
- A nonzero exit, timeout, missing backend/readiness evidence or unbound UDP
  socket warning stops the series immediately, avoiding repeated noisy runs.
- Existing MAINWATCH summaries cover delays over 90 ms, are emitted conditionally
  and omit non-rendering windows. No summary does not prove zero stalls. They do
  not provide a complete p95/p99 frame-time distribution.

## Lifecycle inventory from source inspection

| Area | Current behaviour | Next measurement / constraint |
| --- | --- | --- |
| Native FT8/FT4/FT2/Q65/MSK144/WSPR/JT/FST4 workers | Created lazily by `DecodiumBridgeDecodeWorkers.cpp`; threads are reclaimed by `DecodiumBridgeDestructor.cpp` | Measure first activation, repeat activation and resident memory after cycles; retained worker does not imply busy worker |
| FT/Q65 mode reset | `DecodiumBridgeDecodeBacklog.cpp` removes queued calls for relevant workers, invalidates FT serials, cancels FT8/FT2 work and toggles FT8/FT4 hash processing | Prove cancellation during active work separately for each mode; do not infer full coverage from the function name |
| RTTY/JTTY transitions | `DecodiumBridgeMode.cpp` stops native TX producers and output, coordinates legacy monitoring and dedicated PCM | Preserve audio ownership, PTT release and deferred-request invalidation |
| SSTV/HAMDRM | Facades/controllers are created at startup; RX ownership and workers have separate start/stop paths | Account separately for controller memory, storage, active RX and shared audio; changing application mode alone is not a universal stop contract |
| Waterfall | `DecodiumBridgeSpectrumTimer.cpp` already skips hidden consumers, stops its timer when appropriate and keeps remote consumers in consideration | Profile visible/minimised/remote cases before adding another visibility optimisation |
| Decode UI | Notifications are coalesced; `DecodiumBridgeDecodeUiSnapshot.cpp` moves processing off the GUI thread and budgets model updates | Measure remaining GUI apply time and queue age with crowded bands and large history |
| Startup services | `DecodiumBridgeStartupServices.cpp` defers lookup, audio-device refresh, reporting and CAT work until after QML readiness | Measure first usable RX, not only first visible window; avoid clustering background work at the first decode |
| Recurring timers | UTC 1 s; process usage 2 s; stall probe 100 ms; main watchdog 25 ms after QML; legacy state 250 ms when started; Q65 refresh 1 s | Measure callback cost before suppressing; timer existence alone is not evidence of significant CPU load |
| Thread budget | Existing core reservation and pressure/Low CPU policies in `DecodiumBridgeFtThreadBudget.cpp`, `DecodiumBridgeCpuPressure.cpp`, `DecodiumBridgeDecodeDepth.cpp` | Keep policies unchanged during infrastructure optimisations; record effective settings in decode A/B |

## Acceptance sequence for subsequent changes

1. Freeze binary, QML/resources, settings and input recordings for the reference.
2. Replay identical recordings with identical initial hash/AP state. Compare
   decoded message sets per recording, mode and slot, plus result latency and
   search parameters. Counts alone cannot detect one lost message replaced by
   another. Include weak/overlapping signals, drift, hash resolution and long modes.
3. Repeat through native and supported legacy routes, with normal and Low CPU
   profiles compared independently. Track deadline misses, audio discontinuities,
   cancelled work and automatic pressure interventions.
4. Exercise active-decode mode switches and return to the previous mode; separately
   cycle idle modes to measure retained memory and thread activity.
5. Run two local instances over virtual audio, then test on the target older
   hardware. Virtual audio is functional integration evidence, not RF proof.
6. Accept only measured improvements without baseline decode loss. Keep startup,
   UI responsiveness, DSP throughput and memory results distinct.

Priority: identify measured GUI stalls and unnecessary service work, then improve
resource lifecycle. Decoder arithmetic and search policies remain outside this
first stage. No percentage speed-up is claimed from static inspection.

## First measured reference — 2026-11-14

Release build, macOS arm64, 10 logical cores / 16 GiB RAM, Qt Quick Metal. Three
20-second launches per backend, monitor/CAT off, external network denied and
loopback allowed. All six launches passed, with zero unbound UDP errors and an
identical initial legacy-test INI fingerprint. Its restoration was verified.

| Main-process metric | Native | Legacy |
| --- | ---: | ---: |
| Main QML window, median | 2.220 s | 2.178 s |
| Main QML window, range | 2.193–2.509 s | 2.175–2.337 s |
| Live-map stage, median | 5.668 s | 6.616 s |
| Peak sampled RSS, median | 437.91 MiB | 499.33 MiB |

Raw local evidence: `build/performance-baseline/frozen-startup-3529dae3/`.
Executable/QML reference: `build/performance-baseline/reference-3529dae3/` (not a
standalone distributable bundle; host dynamic-library dependencies remain).

The initial `startup-3529dae3` series is explicitly invalid: its network policy
denied UDP binding, produced user-visible socket errors and a legacy timeout.
It must not be used to diagnose application performance. The corrected harness
was first smoke-tested offscreen in both backends, then measured visibly. An
intermediate `loopback-startup-3529dae3` series predates per-run INI restoration;
use the frozen series above for the reference.

The measurements establish startup reference values, not a speed-up. Early CPU
samples vary with staged background work; twenty seconds do not establish steady
RX CPU use. The map stage includes intentional delayed loading, not just work
duration. Next priority is profiling monitored RX and decode bursts; these data
do not justify changing DSP policies or destroying cached workers on every mode
switch.

Functional baseline: `bridge_modeswitch_decode_runtime_lab.py` passed using the
two existing FT8 WAV fixtures (`CQ K1ABC FN42`, then `CQ 9H1SR JM75`) across
FT8 → FT4 → FT8: lists cleared, second decode recovered, TX false. The temporary
build-QML probe was restored and its hash checked. This is native file-replay
evidence, not continuous audio, a weak-signal corpus comparison, or RF evidence.
Output: `frozen-startup-3529dae3/mode-decode.txt` under the evidence directory.

Harness checks: a non-application executable is rejected despite exit code zero;
an existing Decodium process prevents a concurrent benchmark; a synthetic UDP
bind error terminates the run in about one second and prevents further launches.
Build and Python syntax checks passed; `git diff --check` passed. Application
source, decoding settings and Low CPU policy were not changed in this stage.
