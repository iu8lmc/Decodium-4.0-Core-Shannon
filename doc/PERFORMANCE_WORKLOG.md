# Decodium4 optimisation worklog

Active goal requested on 2026-11-14. Preserve decode output at identical settings,
including the intentional behaviour of the existing Low CPU profile. Baseline
and acceptance rules are in `PERFORMANCE_BASELINE.md`.

Design reference: **Windows 11, 8 GB RAM, third-generation Intel Core i5**.
The user clarified that no i5 is available: a returned Windows A/B report can
come from any available PC, with its actual hardware recorded.
Current user priority is **FT8, FT4, FT2 and general application stability**;
leave the other modes at their current implementation and stop expanding their tests.
Local arm64/macOS measurements and constrained-thread runs are not substitutes
for that machine. Check Ivy Bridge fallback paths as well as UI and memory costs;
do not introduce an AVX2-only requirement for the general executable.

## Current acceptance state (updated 2026-11-14)

The chronological sections below retain intermediate versions and invalid runs.
Do not interpret an older checkbox or binary hash as the latest candidate.
Current laboratory Windows branch and Extended Windows ZIP pin `21d4c70fe`.
The original separate ZIP pins `6f2bbb82`. Both remain unchanged. Latest local
binary before the release version bump is `43520240`, including named timer and
period-phase diagnostics plus receiver lifetime guards;
older performance results identify their own executable (often `eccb131b`).
BootLoader incubation experiment was reverted in source and runtime.

| Requirement | Verified scope | Still open |
| --- | --- | --- |
| Startup and first-use responsiveness | Original/current startup profiles; isolated role/settings/map costs; incubation trial rejected and reverted | No general startup speedup proven; representative external services and Windows interaction latency |
| Same-settings FT8/FT4/FT2 decode preservation | FT8 crowded/weak A/B and Low CPU; FT4 native and eight-slot live A/B; FT2 clean/noisy live pairs; settled three-mode full rows equal on clean files | FT2 clean-run uncertain extra rows vary; broader crowded/weak/drift/hash coverage; no universal no-loss claim |
| UI under RX load | FT8 20-slot 240-message soak; FT2 80-slot noisy soak with exact expected-slot coverage; FT4 eight-slot pairs | Initial unsampled FT4 QTimer stall unresolved; sampled Metal waits do not establish cause; hours-long sessions and large user data |
| Mode lifecycle and memory | FT8/FT4/FT2 three-cycle recovery and settled-row comparison; FT4 active cancellation; prior other-mode fixes documented below | Sampled RSS is not heap-leak exclusion; module eviction only with measured benefit and preserved state |
| Two-app functionality | FT8, FT4 and FT2 real TX/BlackHole/RX; earlier Q65 proof retained as history | Physical RF and Windows integration |
| Old CPU compatibility | Ivy Bridge dispatcher tests; frozen Windows pair compiled; follow-up run 43625741569 and installed runtime passed | No older PC available; any Windows report must record actual hardware; no target-hardware proof claimed |
| Isolation and UDP | Current labs reserve loopback UDP ports, use private settings, restore test INIs/QML; recent FT8/FT4/FT2 runs have no UDP warnings | User-environment diagnosis if errors persist outside the lab |

Other modes remain at their existing implementation per the user's latest
instruction; their historical coverage gaps below do not expand current work.
Recent evidence: `ft842-settled-comparison.json`, `ft4-live-strict-comparison.json`,
`ft2-live-repeat-comparison.json`, `ft2-noisy-live-comparison.json`,
`ft2-noisy-soak-80/acceptance.json`, `two-app-ft4-after/result.json`,
`ft4-stall-sampled/`, `qml-timer-attribution-test/attribution.json`,
`windows-extended-final-validation.json`, `windows-followup-artifacts/run.json`.
All are under `build/performance-baseline/` and retain specific versions/scopes.
The optimisation goal was subsequently cleared by the user. Remaining technical
limitations still apply; the user authorised publication of the accumulated changes
as 1.0.1003. No universal performance or decode-preservation guarantee is implied.

## 2026-11-14 — Decode model role access

`DecodeListModel::data()` previously scanned the role table and allocated a
QString field name on every delegate read. It now indexes immutable keys created
once. A compile-time check preserves the role-table ordering contract. Invalid
roles/indices, missing fields and whole-entry access retain their behaviour.
No DSP, scheduling, search depth or Low CPU policy was changed.

Isolated Release A/B builds used the same test driver and compile/link options,
with the original model source from `3529dae3` compiled into a separate binary.
Three alternating trials, each 4,000 iterations and five QtTest median samples:

| Trial | Before (ms) | After (ms) |
| --- | ---: | ---: |
| 1 | 0.0927 | 0.0539 |
| 2 | 0.0924 | 0.0544 |
| 3 | 0.0940 | 0.0544 |

Each iteration reads all roles for 50 visible rows against a 500-row history.
Median cost fell about 41%, or 0.0383 ms per iteration. This is a small local
cost reduction, **not** a 41% application speed-up. All 17 model tests passed.
Evidence and exact compiler commands: `build/performance-baseline/model-role-ab/`.

Native live-audio smoke validation before/after recorded three `CQ K1ABC FN42`
messages at confidence 100 in each build, no UDP errors and clean exits. The
first baseline run played a continuous repeated fixture; the candidate test
explicitly injected three complete slots. This establishes functional reception,
not yet a paired multistation or latency equivalence result. Evidence:
`rx-baseline-clean/` and `rx-role-cache-after/` under `build/performance-baseline/`.

Paired multistation RX: `busy-901.wav`, three injected slots, baseline and
candidate each persisted all 36 messages (12 per slot). The comparator confirmed
identical per-slot message multisets, no missing or extra rows, zero UDP errors
and clean exits. It also rejects same-count message substitutions and mismatched
input hashes; comparison is relative to each run's injected UTC slots.
Evidence: `busy-role-before/`, `busy-role-after/`, `busy-role-comparison.json`.

## Profiling and lifecycle observations

- A ten-second stack sample showed the main thread waiting for the Qt render
  thread and the render thread waiting for a Metal drawable. These are waiting
  samples, not CPU self-time; do not infer that they prove a CPU bottleneck.
- The initial sampled RX run decoded the fixture but a residual laboratory
  blacklist prevented persistence. It is excluded from decode-equivalence
  evidence. The RX harness now temporarily uses empty *test* legacy settings,
  restoring the original exact bytes at the end, including failure paths.
- FT8 worker objects themselves contain little state. Several downsample/FFT
  caches use heap-allocated thread-local pointers (`FtxDownsample.cpp`,
  `FtxBitmetrics.cpp`, `FtxSharedDsp.cpp`). Deleting/recreating worker threads is
  not automatically equivalent to reclaiming this memory. Audit ownership and
  measured high-water marks before changing teardown policy.
- Four frozen twelve-station recordings are available in
  `build/performance-baseline/ft8-corpus/`, with manifests and SHA-256 hashes.
  Two busy and two mixed weak-signal scenes use stored simulator output and
  seeded noise. Nominal synthetic SNR is not an RF sensitivity calibration.

The goal remains active. No overall performance or zero-regression claim is
made until the relevant paired and functional checks are completed.

## 2026-11-14 — Initialized profile lookup

`ensureSettingsProfileInitialized()` and `beginActiveSettingsProfile()` now read
the existing initialization marker before enumerating all keys. Marked profiles
take the short path; unmarked, false-marker and legacy profiles retain discovery
and precedence. There is no persistent routing cache, so deletion/replacement or
live profile changes remain visible.

The same test driver was saved before changing the header and then rebuilt after
the change. Three alternating Release A/B trials (2,000 iterations, five QtTest
median samples), reading one value from an initialized 2,000-key profile:

| Trial | Before (ms) | After (ms) |
| --- | ---: | ---: |
| 1 | 0.193 | 0.016 |
| 2 | 0.191 | 0.017 |
| 3 | 0.193 | 0.017 |

About 91% less time in this specific lookup benchmark, not overall application
speed. All nine profile tests passed, including cross-process persistence,
async handoff, direct/legacy precedence, false markers and same-process removal
of a direct profile in favour of its legacy counterpart. Evidence:
`profile-lookup-ab/`, `profile-lookup-tests.txt` under the baseline directory.
Combined live-audio validation: the mixed weak-signal scene `mixed_weak-901.wav`
returned 30 messages in each build over three slots, with identical per-slot
message multisets, no missing/extra rows, no UDP errors and clean exits. Both
runs used the same fixture hash and denied access to the macOS security
executable so the lab did not access operator credential storage. Evidence:
`weak-role-before/`, `weak-optim-after/`, `weak-optim-comparison.json`.

CPU compatibility checks: all nine `test_superldpc_cpu_*` cases passed, including
the Ivy Bridge feature combination. These are dispatcher tests with backend
stubs on ARM, not execution of a Windows binary on an Intel i5.

## Next execution steps

Current application SHA-256:
`49a8a17ad96439acf40824bbf71f65fd8b8e1070498ef27636dc1af48436a250`.
Reference executable is also staged at `build/decodium-performance-reference`,
beside the same runtime resources, SHA-256:
`c026d64841cedc7ae30e1f1d42557e417c66cdeaf32c722e9b3839bd3ffaa56b`.

1. Complete both-change busy-scene validation and other frozen noise seeds;
   use `profile_rx_runtime.py` sequentially, then `compare_rx_runtime.py` on the
   resulting `result.json` files. The earlier busy comparison covered only the
   first (model-role) optimisation.
2. Add explicit limited-thread/Low CPU fixture settings to the isolated harness
   and compare each profile against its own baseline. Do not equate fewer FT
   threads on this Mac with an Ivy Bridge PC.
3. Run the two-app BlackHole functional test with corrected UDP bind policy,
   local UDP sinks, isolated credential access and test-INI restoration.
4. Re-measure startup after both changes; investigate lifecycle/memory with
   repeated actual mode cycles before implementing any worker disposal.

No background application/test process remains at the checkpoint. No commit,
push, release or production configuration change has been performed.

## 2026-11-14 — Explicit two-thread profile

The RX harness now supports `--ft-threads` and `--low-cpu`, writes only a fresh
isolated profile, verifies the startup CPU profile and dispatch limits, and
records the requested settings in the result. The comparator rejects pairs with
different requested profiles or credential isolation. Default UDP destinations
2237/2239 are consumed by local sinks; occupied ports abort before app launch.

Both-change candidate versus original, `busy-902.wav`, manual two threads,
Low CPU off: **36/36 messages**, identical per-slot multisets across three slots,
no missing or extra messages, clean exits and no UDP errors. The reference log
explicitly confirms `low=0 ft_threads=2 reason=startup`; candidate verification
also checks this automatically. Evidence: `busy-two-threads-before/`,
`busy-two-threads-after/`, `busy-two-threads-comparison.json`.
This is a limited-thread test on Apple Silicon, not an emulation of Ivy Bridge.

The `threads_active=24` FT8 diagnostic field is the OpenMP maximum pool setting
(`omp_get_max_threads`), not measured concurrent CPU activity. The worker keeps
a fixed pool ceiling and passes the requested budget separately to native
regions; do not infer 24 busy cores from that diagnostic.

Low CPU on, manual two threads, `mixed_weak-902.wav`: original and combined
candidate both return **21/21 messages**, identical per-slot identities and
multiplicities across three slots. Both logs confirm `low=1 ft_threads=2` at
startup, with no UDP errors or timeouts. Evidence: `weak-lowcpu-before/`,
`weak-lowcpu-after/`, `weak-lowcpu-comparison.json`. Low CPU policy is unchanged.
Comparator negative checks reject a changed profile and detect replacement of
one message even when the total decode count is unchanged.

The existing two-app and mode-change laboratories now guard against an already
running Decodium, restore the shared test INI, deny credential-tool execution,
allow UDP binds, consume reporting destinations locally, and remove only their
own test directories. The two-app test remains virtual audio, CAT disabled.

Lifecycle follow-up from source inspection: FT4 invalidates queued/stale serials
and checks before/after acquiring the Fortran runtime mutex, but exposes no
cooperative cancellation for a decode already inside its native decoder. Q65
likewise has no cancellation method in its worker interface. Measure work still
running after a mode change before choosing a cancellation/retirement change;
retaining an idle reusable worker and retaining an active obsolete decode are
different costs. This is an audit finding, not a verified runtime regression.

Combined-candidate two-instance functional proof passed: one isolated Decodium
transmitted `CQ 9H1SR JM75` through BlackHole 2ch and the receiver persisted it.
Both processes exited zero, measured audio peak 0.402, onset at slot +0.478 s,
no UDP errors. QML and shared test INI hashes matched the originals after cleanup.
Evidence: `two-app-optim-after.txt`, `two-app-optim-after-artifacts/`,
`two-app-restoration-before.json`. This proves the virtual TX/audio/RX chain;
it is not physical radio, Windows, or complete protocol-interoperability proof.

Resource samples from these single paired runs do not establish a whole-app
CPU/RAM improvement. Busy/two-thread peak RSS was 512.6 -> 564.7 MiB; weak/Low
CPU 566.6 -> 561.5 MiB. Median sampled `ps %cpu` was approximately 35% in all
four runs. RSS includes retained/native/graphics allocations and needs repeated
controlled measurements; the busy increase must not be hidden behind the
microbenchmark gains. No claim of reduced application memory is made.

Combined-candidate FT8 -> FT4 -> FT8 file-decode check also passed:
`cleared=true, recovered=true, tx=false`; output in
`mode-decode-optim-after.txt`. Both the QML file and shared test INI again matched
their pre-test hashes. This check exercises mode clearing and FT8 recovery, not
an active FT4 decode or repeated-cycle memory plateau.

### Remaining execution queue

1. Re-measure whole-app startup with matched before/after laboratory settings;
   update startup isolation to the shared credential-denying policy and local
   UDP sinks before running new paired series (retain old evidence as historical).
2. Measure repeated FT8/FT4 cycles with actual decodes in both modes and idle
   dwell periods, RSS/high-water marks and obsolete work after mode changes.
   Investigate the busy-scene RSS difference above with repeated runs.
3. Choose further UI/service/lifecycle optimisations from those measurements,
   preserving DSP settings and per-slot A/B decode equivalence.
4. Windows 11 / 8 GB / third-generation i5 remains the acceptance hardware;
   local Apple Silicon tests are supporting evidence only. Physical RF and that
   machine's performance remain unverified.

Goal remains active. At this checkpoint all launched applications have exited,
syntax checks and `git diff --check` pass, and no commit/push/release occurred.

## 2026-11-14 — Paired startup and real FT4 cycles

Twelve visible startup runs completed with fresh bridge/legacy test defaults,
credential execution denied, UDP loopback sinks and exact test-INI restoration.
Three before/after pairs, both backends; all valid, no UDP errors. Median values:

| Backend / metric | Original | Two optimisations |
| --- | ---: | ---: |
| Native Main.qml ready | 2.154 s | 2.125 s |
| Native live-map stage | 7.630 s | 5.522 s |
| Native peak sampled RSS | 413.55 MiB | 432.78 MiB |
| Legacy Main.qml ready | 3.158 s | 2.172 s |
| Legacy live-map stage | 7.531 s | 6.901 s |
| Legacy peak sampled RSS | 503.11 MiB | 479.97 MiB |

Evidence: `startup-paired-{1,2,3}-{before,after}/` and
`startup-paired-summary.json`. These are small local series, with visible
render-event timing variability (including roughly one-second UpdateRequest
stalls on this Mac); they do not establish a universal startup speedup or RAM
reduction. Native initial-window time is effectively unchanged in this series.

`profile_mode_cycles.py` now measures actual alternating FT8 and FT4 file
decodes, four-second dwell periods, per-step clear/recovery, and sampled idle
RSS. The original completed six cycles / twelve expected messages with no UDP
errors. Its RSS rose during initial decoder use, plateaued around 580 MiB and
later fell; this run does not show monotonic per-cycle growth. FT4 uses a frozen
native-generator fixture with recorded SHA-256, not a mode toggle without data.

## 2026-11-14 — Static GPU map cadence

`WorldMapGpuItem` previously requested a scene-graph update on every animation
tick even with no contacts, no transmitting path, no earthquake pulses and no
viewport motion. Static maps now request the timed refresh once per UTC second
(the clock/greyline resolution). Direct setters still update immediately;
contacts, TX, earthquake overlays, pruning and viewport motion retain the
original animation cadence. No decoder/search/depth settings change.

Actual `updatePaintNode` calls in a standalone Qt Quick scheduling test:
**31 -> 3 frames in 2.5 seconds** for an idle, fixed viewport. The original source
was compiled with the same test driver and failed the idle-frame regression
assertion as expected. The candidate passed idle, contact animation (14 frames
in one second), earthquake animation, return to idle, immediate setting change,
viewport motion and deactivate/reactivate checks. This is a software-rendered
scheduler test, not a GPU speed or whole-app CPU percentage claim.
Evidence: `map-cadence-after.txt`, `map-cadence-ab/{before.txt,commands.json}`.
An initial test correctly kept animation-rate frames while home-grid auto-zoom
was still moving; the idle fixture now explicitly fixes its viewport and tests
motion separately. The application Release build also succeeded.

The two-optimisation candidate also completed six real FT8/FT4 cycles with all
twelve expected per-step messages, matching the original fixture hashes and
message identities. Its RSS likewise plateaued and then fell. Evidence:
`mode-cycles-two-optim-after/`. These tests do not cover cancellation of a decode
already running when the mode changes; that remains a separate measurement.

Static-map application pilot (native backend, monitor off, 45 seconds each,
25–42 s sampling window): process `ps %cpu` median **20.4 -> 4.9**, mean
**20.68 -> 13.41**. Before is the two-optimisation executable, so the incremental
change is the map cadence. Both runs valid, no UDP errors. This is one local
pair, not a Windows or total-system-CPU result. The fresh lab window used the
minimal default geometry: the GPU map was instantiated, but did not emit its
nonempty first-frame diagnostic; the independent 640x360 renderer test covers
actual map-node updates. Repeat with representative visible map geometry before
extrapolating this CPU reduction. Evidence: `map-idle-before/`, `map-idle-after/`.

The renderer test additionally passes TX animation without contacts and checks
that reactivation produces a new frame, not just a previously counted frame.

After the map change, the busy/two-thread live audio scene again returns all
**36/36 messages**, identical per-slot multisets against original 3529dae3,
with no UDP errors. Evidence: `busy-map-after/`, `busy-map-comparison.json`.
Current three-optimisation application SHA-256:
`787142580c5cbd5257d896da8d8d03b465e5d6d791d385513bfc42d5eb30b126`.
The incremental two-optimisation reference remains available as
`build/decodium-two-optim-reference` (SHA-256 `49a8a17ad96439acf40824bbf71f65fd8b8e1070498ef27636dc1af48436a250`).

Follow-up map pilots **do not confirm a whole-app CPU improvement**:

| Layout | Median process %CPU before/after | Mean before/after |
| --- | ---: | ---: |
| Minimal window, empty map rectangle | 20.4 / 4.9 | 20.68 / 13.41 |
| 1600x900, map 356x38 | 27.8 / 30.7 | 22.19 / 20.45 |
| 1600x900, waterfall 180, map 356x208 | 11.1 / 31.6 | 15.73 / 21.57 |

All use the same 25–42 s window, one pair per layout, no UDP errors; the latter
two verify a nonempty map frame on Metal with the production shaders. The
instantaneous/smoothed `ps %cpu` samples are too variable and include results
worse after the change. Do not select only the favourable minimal-window pair.
Evidence: `map-visible-*`, `map-layout-*`, `map-idle-summary.json`.
The harness now also captures cumulative process CPU time, to measure actual CPU
seconds over matched intervals and reverse the execution order in the next pair.
The renderer scheduling reduction remains verified; application-level savings
are still under evaluation.

Cumulative-CPU follow-up, reversed order (candidate then reference), visible
356x208 map, same production Metal shaders and fixed layout: over ~26.3 seconds
inside the 15–42 s window, CPU time was **5.46 s before / 5.08 s after** (20.74%
vs 19.31% of one logical CPU). This pilot is consistent with a modest saving,
not the large reduction suggested by the first instantaneous-CPU median. More
repetitions are needed to quantify whole-app benefit. Evidence:
`map-cputime-before/`, `map-cputime-after/`, `map-cputime-summary.json`.

### Current continuation queue

- Keep the goal active. Three candidate optimisations are implemented and built:
  model role lookup, initialized profile lookup, idle GPU-map scheduling.
- Rendering regression checks pass, and the three-change build preserves 36/36
  busy-scene messages at two threads. Weak/Low CPU and two-instance functional
  proof currently cover the two-change build; extend those to the final set.
- Six actual FT8/FT4 cycles pass before/after the first two changes, without
  monotonic memory growth in these runs. No worker destruction policy was added.
- Measure cancellation of already-active FT4/Q65 work during a mode change;
  investigate timer/UI work still dominating process CPU. Do not infer a leak
  or safely reclaimable cache merely from retained RSS/thread-local pointers.
- Whole-app CPU measurement now uses cumulative CPU seconds. Any further paired
  runs must match layout profile, fixture, CPU settings, rendering mode and
  credential/network isolation. Do not compare sampled runs with build activity.
- A text question is pending about how the Windows reference PC can run the
  eventual A/B package. Continue local work independently; no access is assumed.
- At this checkpoint all lab applications are stopped, QML/test settings have
  been restored by the harnesses, Python syntax and `git diff --check` pass.
  No commit, push or release has been performed.

## 2026-11-14 — FT4 cooperative cancellation on mode exit

The existing test-mode hook proves the gap: with a live FT4 worker, changing to
FT8 completed in 7 ms, while the now-obsolete FT4 call continued until 97 ms.
The old result was already rejected by the mode/serial checks. A worker-owned
cancellation generation now advances only on explicit invalidation (the sentinel
used by mode reset) or shutdown. Ordinary new serials/slots do not advance it.
The native FT4 decoder polls at subpass/candidate/segment/AP-pass boundaries;
partial output is discarded when cancelled. The thread-local RAII scope restores
its predecessor and does not affect other decoder callers/threads. FFTW buffers
and reusable workers are retained; this change does not attempt unsafe teardown.

In the same active-worker laboratory, the invalidated FT4 call now completes in
**8 ms**, and the restored FT4 decode completes normally (102 ms in that run).
Both runs finish all six expected per-step messages, including recovery, with
no UDP errors. Evidence: `ft4-active-switch-before/`,
`ft4-active-switch-after/`. This is one local timing comparison, not a bound on
all cancellation latencies: a native operation between checkpoints must finish.
The pre-change binary includes the first three performance optimisations; its
FT4 decoder/worker implementation was still unchanged from 3529dae3.

`test_ft4_decode_cancellation` passes four QtTest checks covering known-message
recovery, nested cancellation scopes and independent-thread isolation. The
initial std::thread test hit its small default stack's guard on macOS in
`generate_ft4_reference`, confirmed by the crash report. The test now uses an
8 MiB QThread stack, matching the production worker, and passes. This was a test
stack sizing failure, not hidden as a passing result.

A/B native FT4 corpus: seven frozen fixtures (clean plus three nominal noise
levels and two seeds), depths 1 and 3, **14/14 identical complete outputs**.
Comparison includes message, SNR, DT, frequency, AP and quality fields. The corpus
contains both successful weak decodes and below-threshold zero-decode cases;
noise -20 seed 902 decodes only at depth 3, identically in both builds. Nominal
noise is not RF-calibrated sensitivity. Evidence: `ft4-cancel-ab/manifest.json`
and per-case before/after text. The pre-change CLI binary is retained as
`build/performance-baseline/ft4-stage-before`.

The three-change build also passed Low CPU weak-scene live audio: **21/21**
identical per-slot messages against 3529dae3, evidence
`weak-lowcpu-map-after/`, `weak-lowcpu-map-comparison.json`.
Current four-change app SHA-256:
`3ae6b48e77b1b681122e15e2261d4711ac4239a3b55e7f60b3485d48785e9454`.

The native A/B reference was strengthened by compiling
`git show 3529dae3:Detector/FtxFt4Decoder.cpp` with the current target's exact
compiler flags and linking that original object into the comparison CLI. All
**14/14 outputs still match** the candidate byte-for-byte. The older pre-existing
CLI was only a preliminary comparison; canonical evidence is the
`*-3529dae3.txt` reports, `reference-commands.json`, and the updated manifest.

Final four-change two-instance BlackHole test passed: both processes exited
zero, RX persisted `CQ 9H1SR JM75`, TX onset was slot +0.410 s, no UDP errors.
QML and the shared test INI again match their original hashes exactly. Evidence:
`two-app-four-optim-after.txt`, `two-app-four-optim-artifacts/`.

### Latest checkpoint / remaining work

Four candidate optimisations are built and verified to the scopes above.
No application/test process remains running. Syntax checks and diff whitespace
checks pass. No commit, push, release or production-settings change was made.
The goal remains active; do not call the overall work complete.

Next: audit/measure already-running Q65 and long-period worker behaviour across
mode changes, complete the resource-lifecycle inventory beyond FT8/FT4, and
investigate remaining UI/service CPU costs using cumulative CPU time rather than
instantaneous percentages. Retaining idle threads is still intentional because
native thread-local cache destruction/recreation has separate safety costs.
Keep the frozen fixtures, original app, canonical FT4 reference and per-build
hashes when adding further changes. Windows/RF acceptance remains unverified;
the question about executing the Windows A/B package is still pending.

### Session-boundary audit (2026-11-14)

Hardware reference confirmed: Windows 11, 8 GiB RAM, third-generation Intel i5.
No Windows runtime result is implied by the macOS measurements.

Long-period audit found a correctness prerequisite to queue optimisation:
WSPR and legacy JT callbacks can accept a result from an invalidated session
when request metadata has already been cleared. The first Q65-only QML probe
was not callable (private method), and the first C++ probe's fresh positive
control failed; neither is acceptance evidence. A subsequent Q65 positive
control also correctly failed because the test used `Q65` rather than the
canonical current mode `Q65-60A`. These failed attempts remain in the artifacts.

The corrected temporary C++ hook uses the existing invokable `logSstvQso`,
guarded by test mode and a specific probe key. Source is restored after build;
this instrumented binary is laboratory-only. The normal app must be rebuilt
after the probe. `profile_mode_cycles.py` now rejects a callback test unless
all three fresh positive controls pass, CAT/TX stay off, and WSPR temporary-file
cleanup succeeds. `--expect-late-rejected` additionally checks every old result.

Baseline `context-probe-before3/` is valid: real FT8 and FT4 fixture decodes pass;
Q65 old result is already rejected, WSPR and JT65 old results are accepted;
all three fresh results are accepted, and WSPR removes its temporary file.
No UDP error. The WSPR/JT65 fresh controls intentionally omit metadata to cover
slow valid results after the existing 60-second metadata expiration.

Candidate fix records the highest issued serial at each existing session
invalidation (mode, band, RX restart, native-to-legacy handoff). Old serials are
rejected independently of the expiring maps. WSPR still removes its temporary
WAV before rejection; stale results cannot clear the newer session's busy flag.
JT callbacks now also discard their finished request metadata. Decoder search,
iteration depth and Low CPU policy are unchanged.

Instrumented A/B now passes: `context-probe-after/` uses the same three controls
and requires stale rejection. Q65/WSPR/JT65 all reject old results and accept
fresh ones; WSPR temporary WAV and map entry are removed. Real FT8/FT4 fixture
decodes also succeed, CAT/TX remain off, and UDP errors are absent. This is a
callback/session regression test, not a sensitivity test of the WSPR/JT/Q65
signal-processing engines, nor an active-worker cancellation test. Before/after
instrumented binary hashes and fixture hashes are recorded in each result JSON.
The first failed attempts are not used for this conclusion.

Further lifecycle work remains: WSPR currently holds the shared decoder runtime
mutex through `wsprd_run`, with no cooperative cancellation API; queued WSPR
calls cannot simply be removed because their completion cleans the temporary
WAV. FST4 and Q65 also use shared runtime state; JT/Q65 averaging state must be
preserved deliberately. No claim of cancellation or unloaded memory for these
workers is made by the callback fix.

The normal application was rebuilt after removing the temporary hook.
SHA-256: `fbe7f6facde54523f2471d4eeb1872a8bfad9986bcde899775d8bbbcaf1a078d`.
Four focused CTest suites pass (profile restart, decode model, map cadence,
FT4 cancellation); see `context-focused-tests.txt`. The final build contains
only the existing macOS deployment-target linker warnings.

Two normal app instances pass the BlackHole TX/RX check again: both exit zero,
RX persists `CQ 9H1SR JM75`, TX onset is slot +0.385 s, no UDP errors. Evidence:
`two-app-context-after.txt` and `two-app-context-artifacts/`. This verifies
virtual-audio operation, not physical radio or Windows hardware behaviour.

Final weak-scene A/B against original 3529dae3: **21/21 identical per-slot
messages**, two threads and Low CPU enabled in both builds. Requested settings
were verified in runtime diagnostics. No missing or extra messages, no UDP
errors. Evidence: `weak-lowcpu-context-after/` and
`weak-lowcpu-context-comparison.json`. This preserves Low CPU's existing policy;
it does not claim parity with a different CPU/depth configuration.

Latest checkpoint: four performance optimisations plus session-result isolation
are built. QML and the shared test INI match their pre-lab hashes exactly;
no Decodium process remains running. Source instrumentation is absent, Python
syntax checks and `git diff --check` pass. No commit, push or release.
The goal remains active: long-period queued/active-work cancellation, broader
resource lifecycle and UI/service profiling, and Windows i5/8 GiB acceptance
are still outstanding. This checkpoint does not claim completion of those tasks.

### Requested Windows test package

User requested a package to run on the Windows i5 machine. Added a PowerShell
5.1-compatible runner (no Python dependency on the target), CMD launcher,
Italian instructions, frozen FT8/FT4 fixtures and a temporary-QML probe. Runs
are A-B-B-A, with two threads and Low CPU off by default; optional Low CPU is a
separate matched run. It records cumulative CPU seconds, working set/private
bytes, first observed rows, complete per-case message multisets and raw logs.
The 15-second minimum per-case settling interval is explicitly not decode time.
CAT/monitor are disabled. Portable apps are copied before QML instrumentation;
legacy Qt test settings are backed up and restored. Ports 2237/2239 must be free.
Only the test-area files decodium4.ini and ft2.ini are temporarily emptied.
Qt Windows test/config path mapping was checked against the Qt 6.11 source.

The bundle manifest records every fixture and portable-build file hash and a
candidate source patch generated through a temporary Git index (no user index
changes, commit or push). Missing builds cause the runner to stop before
launching Decodium. No Windows binary exists locally for the current changes;
the staged ZIP is deliberately `ready:false`, not a ready-to-run application.
Windows executable/runner acceptance still requires Windows execution.

PowerShell parser check passes using an official SHA-256-verified macOS
PowerShell runtime. Comparator tests pass for reordered equal messages,
same-count changed messages, missing duplicates and empty results, for both
A/B pairs. This checks portable script logic, not Windows process APIs.
The first local run of the exact QML probe passes all four fixtures:
clean FT8=1, busy FT8=12, weak FT8=9, clean FT4=1; no UDP errors. Evidence:
`windows-kit-probe/`. A small ordering fix starts the observation timer before
opening the WAV so very fast zero-result callbacks are also observed.

Final staged kit: `build/performance-baseline/Decodium4-Windows-AB-kit.zip`,
SHA-256 `0d680881c6a5540abc49877514098fdaa0199e35b215f24298d54a1c04f130a4`.
The corrected exact packaged QML probe passes again (1/12/9/1 messages) in
`windows-kit-final-probe/`. Packaged PowerShell files parse, the comparator's
four positive/negative cases pass, ZIP and fixture/patch hashes verify. The
incomplete-build preflight was exercised and correctly refuses to launch or
create results. Original QML/shared test settings were restored exactly.
These are macOS probe/script checks; Windows process/runtime validation is open.
The ZIP freezes the source patch before this documentation-only checkpoint.

Pending user approval: create/push two laboratory branches
`perf/windows-ab-baseline` and `perf/windows-ab-candidate`, then dispatch the
existing Windows workflow with no release tag. GitHub authentication is
available and those remote branch names are unused. No branch, commit, push,
workflow dispatch or release has been performed. Approval is needed to move
currently local candidate code onto GitHub; absent Windows binaries are not
silently replaced with unrelated releases. The optimisation goal stays active.

### WSPR queue retirement and authorised Windows builds (2026-11-14)

User authorised the two laboratory branches and Windows builds, without a
release. Baseline branch now points to exact 3529dae3; workflow run
https://github.com/elisir80/Decodium-4.0-Core-Shannon/actions/runs/43613527579
is building. The first dispatch used gh's inferred upstream repository and was
rejected (422, no matching ref); explicit `--repo elisir80/...` dispatched the
correct run. No workflow ran on the upstream repository.

WSPR previously executed old queued requests after mode/session changes, even
though their callback was discarded. `invalidateDecodeSession()` now updates
the existing retired-serial boundary and notifies the WSPR worker. The worker
checks it before and after acquiring the shared runtime mutex. A skipped request
still emits completion, so the bridge's existing pre-gate WAV cleanup runs.
No active `wsprd_run` is interrupted, no worker is destroyed/recreated, and no
depth/Low CPU policy changes. The two small legacy worker files were normalised
from CRLF to LF to avoid new trailing-whitespace errors.

`test_wspr_worker_queue` exercises an actually blocked native entry point with
old/fresh requests behind it, plus retirement while waiting for the runtime
mutex. All four QtTest checks pass. Compiling the original 3529dae3 worker against
the same driver makes the test fail (three native calls instead of two). A
separate deliberate mutant without the post-lock check fails the lock-wait
case. These are controlled-worker tests, not measurements of native WSPR speed.
Evidence: `wspr-queue-tests.txt`, `wspr-queue-ab/` including compiler commands.

Native application A/B uses WSJT recording `150426_0918.wav`, SHA-256
`0f1e41e6a94fefd3df9efd8234dd6342dfd7e6675700f70e79e69eec955537da`.
The exact original application 3529dae3 and the candidate both produce **8/8**
identical message/DT/frequency multisets at depth 3, with no UDP errors.
Evidence: `wspr-native-3529dae3/`, `wspr-native-after/`,
`wspr-native-comparison.json`, `wspr-queue-source-record.json`.

The Windows kit generator now includes this WSPR fixture alongside FT8/FT4,
with provenance in its manifest. The QML probe reads the actual `db` SNR field.
The candidate Windows workflow additionally runs the five focused performance/
lifecycle test suites before creating its portable bundle. Earlier staged ZIPs
remain frozen checkpoints; the final package must use the new candidate patch
and the successful Windows artifacts, not an older staging ZIP.

### Windows laboratory isolation and build retry (2026-11-14)

The initial original-baseline build succeeded, but must not be delivered: early
application storage migration occurred before test-mode setup. Both laboratory
branches now share identical early test storage isolation. Normal launches retain
normal migration. The baseline changes only laboratory support, tests and workflow;
decoder/performance code remains original 3529dae3. The candidate also fixes the
standalone model test's missing Windows logging symbol using a test-only sink.

Current baseline f037f84e40356997ddfdf46a11c247a468fc7077:
https://github.com/elisir80/Decodium-4.0-Core-Shannon/actions/runs/43616677624
Current candidate 6f2bbb82bd43dfc75b44d8c70553c2f69cea8c38:
https://github.com/elisir80/Decodium-4.0-Core-Shannon/actions/runs/43616679233
Both are still building at this checkpoint. Candidate CAT, all six focused
regression suites and Windows PowerShell harness verification passed. No release
was requested or published. The collector verifies source commits, workflow
conclusions, artifact identities and executable hashes before packaging.

The runner uses the direct graphics worker so CPU/memory samples belong to the
application, with the same explicit graphics backend and private temporary
directory for both builds. It requires the early-isolation log marker. The
local five-fixture application probe passed with 1/12/9/1/8 messages and no UDP
errors after early isolation; this remains macOS evidence, not Windows PC proof.
A subsequent harness-only cleanup fix ensures failed process startup does not
prevent restoring test settings; PowerShell parsing passed locally.

Runner cleanup negative control: executing the actual extracted finally block
with a real System.Diagnostics.Process whose Start fails reproduces a secondary
Kill exception with the previous runner. The corrected runner restores existing
INI bytes and removes a test file that was originally absent. Both outcomes are
recorded in runner-cleanup-check/results.json. This check uses local PowerShell
7/.NET on macOS; Windows 5.1 end-to-end execution remains unproven.

### WSPR native resource cleanup, separate follow-up (2026-11-14)

While the pinned Windows builds run, a native allocation/file-handle probe
confirmed a leak in the original wsprd implementation: 329 bytes per successful
invocation (apmask, cw, grid), and 1,114,812 bytes plus three output FILE handles
per missing WAV failure. Three consecutive invocations accumulate those amounts.
This accounting wraps calloc/free/fopen/fclose in the real wsprd translation unit;
it is not process RSS or an audit of FFTW's internal allocations.

The local follow-up frees those three buffers and funnels pre-FFT errors through
common cleanup, keeping variable-length FFT arrays in a separate lexical scope.
Source CRLF line endings are preserved. Decoder arithmetic, passes and thresholds
are unchanged. This change is NOT in the two frozen Windows build commits.

The reusable tests/wspr_resource_probe.c verifies expected return status and no
remaining counted allocations or open files after repeated calls. Six native
cases pass, three calls each: valid reference WAV, missing WAV, missing C2, empty
WAV, unsupported suffix and inaccessible/nonexistent output directory. The same
probe compiled against original 3529dae3 fails the missing-WAV regression. Three
valid decodes before/after produce identical complete native output.
Evidence: build/performance-baseline/wspr-memory-audit/{comparison.json,
regression-results.json,negative-control.json}; compiler command is retained in
negative-control.json (omit WSPR_SOURCE_FILE to test current source).
The full local app rebuild passes; application file-replay verification is in
progress at this checkpoint. No Windows or RF claim is made for this follow-up.

Application follow-up verification completed: the five frozen fixture cases pass
with 1/12/9/1/8 messages, exactly matching the message multisets before cleanup;
zero UDP errors, no timeout, exit 0. Evidence: wspr-memory-app-probe/ and
wspr-memory-audit/app-verification.json. Shared QML and test-INI fingerprints
match the pre-existing restoration reference. This is local file replay.

The resource probe is now integrated into CMake/CTest as
test_wspr_resource_valid and test_wspr_resource_missing; both pass (1.40 s).
The valid-input regression uses the existing short FT8 WAV only to exercise
successful WSPR input processing and cleanup; it is not a WSPR decode-sensitivity
test. The separate actual WSPR recording remains the native equivalence evidence.
Windows baseline run 43616677624 completed successfully and its portable artifact
14124015328 was downloaded (2,096 files, 366,527,327 bytes). Candidate run
43616679233 has finished compiling and is preparing its portable bundle.

Final-kit preparation adds CREA_REPORT.cmd / Export-Report.ps1. The exporter
whitelists machine/comparison JSON and each run's result JSON/console log into
report.zip, excluding app copies, restore-private and unrelated files. An actual
ZIP export with planted excluded files passes locally under PowerShell 7; this
is not yet a Windows 5.1 end-to-end run. Ready packages now state that both builds
are included instead of retaining the staging-kit warning.

### Executable Windows A/B kit delivered (2026-11-14)

Both current workflows completed successfully, including the installed-runtime
check; release publishing was skipped. Collector session 68444 finished normally.
Baseline: f037f84e40356997ddfdf46a11c247a468fc7077 / run 43616677624.
Candidate: 6f2bbb82bd43dfc75b44d8c70553c2f69cea8c38 / run 43616679233.
Candidate six focused regression suites and Windows PowerShell harness gate pass.

Ready artifact: Decodium4-Windows-AB-2026-11-14.zip (198960227 bytes).
SHA-256: bcbf4baf2c37aa6e67e813a333a69ebe376bf56b48d8e1776d15a52bf51a0e0f
Actual ZIP contents verified: 4,192 portable build files, five fixtures, both
source patches, required scripts and ready-state instructions. Packaged PowerShell
scripts parse locally; packaged comparator positive/negative cases pass. Evidence:
windows-ab-artifacts-v2/ci-evidence.json, windows-final-package.json and
windows-final-validation.json. This replaces all incomplete staging ZIPs.

Run AVVIA_TEST.cmd on the Windows 11 / third-generation i5 / 8 GB reference PC,
then CREA_REPORT.cmd to export diagnostic-only report.zip. Actual Windows runner
execution and old-PC CPU/memory/decode comparisons remain pending. The kit's
application commits are frozen: the subsequent local wsprd native resource-cleanup
fix and its new CTest cases are not in these binaries. They remain a separately
verified follow-up. The broader optimisation goal is not complete.

### Q65 worker profiling checkpoint (2026-11-14)

Added profile_q65_worker.py to time real worker.decode calls inside the existing
continuity tests, excluding synthetic-signal generation. The helper temporarily
instruments only test source, saves raw logs and results, restores the exact
source bytes, and rebuilds the normal test executable in finally. No application,
audio device, CAT, UDP sender or radio is opened.

Three independent runs (15 calls total) all retain the expected decoded messages.
On the local arm64 Mac, strong 15A inputs take 2.704-5.737 ms; strong 60A/60B
inputs take 9.302-12.299 ms at depth 3, averaging off. Evidence:
build/performance-baseline/q65-worker-current/results.json and raw run logs.
These easy inputs do not justify changing cancellation/state retention. Weak or
crowded inputs, mutex contention, actual mode changes and older hardware are not
covered. Next lifecycle measurement must exercise those conditions instead of
using this fast strong-signal path as cancellation evidence.

Source inspection reconfirms Q65 holds the shared runtime mutex inside the native
entry point and FST4 holds it inside decodeFst4Lines. The existing FST4 explicit
reset function clears both persistent_state and sync_cache (not just persistent
state); no claim of automatic invocation on mode change follows from its presence.
No Q65/FST4 production policy was changed in this checkpoint.

Removed three empty root-directory WSPR output files created by the earlier
invalid-argument resource probe; their creation times matched that invocation.
Subsequent resource tests run with explicit private output directories.

### Weak-input Q65 profiling (2026-11-14)

Built the maintained utils/q65sim.cpp as q65_performance_fixture (without enabling
unrelated utility targets). Frozen Q65-15A inputs at generator SNR -15/-22/-28 dB,
one signal, no Doppler, one file each; command lines and WAV/generator hashes are
in q65-weak-corpus/manifest.json. Noise is one frozen realization at each level,
not a statistical sensitivity evaluation.

profile_q65_worker.py now accepts a frozen 12 kHz mono 16-bit 15-second WAV,
records complete output rows, and can explicitly allow misses for profiling.
In this mode expected-message assertions are disabled only in temporary test
instrumentation; serial/completion structure remains checked. This must not be
presented as a passing sensitivity or no-loss acceptance test. Source and the
normal test executable are restored afterward. Decoder production code is unchanged.

Three process runs, three worker calls each, depth 3, averaging off:
- -15 dB: expected message 9/9 calls, 2.747-5.826 ms.
- -22 dB: expected message 0/9 calls, 1179.340-1187.284 ms.
- -28 dB: expected message 0/9 calls, 639.258-659.961 ms.
Raw output and fixture identities: q65-weak-profile-{snr}/; aggregate:
q65-weak-summary.json. These are current native decoder observations, not before/
after optimisation results or old-hardware results. The slow failed-search path
justifies the next measurement of actual mode invalidation and runtime-mutex
contention; fast strong inputs were not representative. No cancellation change
has yet been made to Q65 or FST4.

### Native Q65 shared-mutex contention (2026-11-14)

The profiler's explicit --contention option runs the real worker on another
thread, observes the actual runtime_mutex becoming busy via tryLock, then measures
a second caller's blocking lock acquisition. It joins before reading emitted
results and restores/rebuilds the ordinary test source afterward. This is a
mutex-level measurement, not an application mode-switch/cancellation test.

Nine weak -22 dB calls all observe the native decoder holding the mutex. The
second caller waits 1208.543-1233.871 ms (median 1221.487 ms). Complete emitted rows
match the prior single-thread weak-input profile. Evidence:
q65-runtime-contention/results.json, summary.json and run logs. No production
code changed. This establishes an actual shared-resource delay rather than only
CPU duration. Next acceptance must observe a real bridge mode invalidation while
this slow path is active, then verify recovery/averaging before changing policy.

Cancellation audit: decode_q65_session updates Q65RuntimeState, including spectral
averages, cached spectra and caller history. Candidate/AP pass boundaries exist,
but arbitrary thread termination or mid-accumulator return is not justified.
Do not infer state safety from callback rejection alone.

### Actual application Q65 mode change (2026-11-14)

profile_q65_mode_switch.py temporarily instruments the native Q65 entry immediately
after its runtime lock is acquired, then requests a real bridge mode change via
the GUI thread 40 ms later. QML opens the frozen weak -22 dB WAV, observes FT8,
waits three seconds, returns to Q65-15A and opens the recoverable -15 dB WAV.
The final verifier requires timestamp evidence that the original decode was still
active when the mode setter began, plus successful recovery, no stale rows in the
intermediate mode, no TX and the standard isolated harness checks.

Final evidence q65-mode-switch-before-v3/: valid=true, active_at_switch=true;
mode setter 6 ms; original Q65 native decode continues 1,154 ms after setter
completion. On return, native Q65 decodes in 5 ms and the expected message reaches
the QML decode list. Zero UDP errors. This demonstrates retained obsolete work,
not a 1.15-second GUI freeze: the mode setter itself completed quickly. No
production cancellation policy was changed. The measurement is macOS file replay,
not RF or Windows hardware proof.

First two attempts failed the recovery matcher: actual Q65 rows retain the q0
decode annotation in their message field. Diagnostic callbacks reported one
accepted row, and the second attempt captured the full model row. The final
matcher accepts only the exact expected message with its optional documented
Q65 q<type>[average-count] suffix; original rows remain in evidence. Those earlier
runs are not complete acceptance evidence. No parser/UI behaviour was changed.

All temporary source/QML hooks are restored and the normal application rebuilt.
Core/SSTV sources have no diff; QML and legacy-test INI hashes match their exact
pre-lab reference fingerprints. The delivered Windows kit remains unchanged.

### Cooperative retirement of non-averaging Q65 work (2026-11-14)

Q65DecodeWorker now receives the same monotonic retired-serial boundary as WSPR
from invalidateDecodeSession. New slots do not move this boundary. A scoped,
thread-local cancellation context is enabled only for requests without coherent
averaging. Retired queued requests finish empty before audio/decoder preparation;
the native entry checks again before and after acquiring runtime_mutex. Active
requests check retirement after complete spectrum/cache updates and between
complete candidate/AP passes. No thread termination, signal search reduction,
changed depth or Low CPU policy is introduced. Retired completions cannot update
the coherent-count UI. Coherent-average requests keep native completion semantics
(including their existing state updates); their obsolete results remain discarded.

Actual app A/B: q65-mode-switch-before-v3 versus q65-mode-switch-after. The same
weak recording, depth 3 and averaging off show residual native work after the mode
setter falling from 1,154 ms to 20 ms. Both prove the decoder was active when the
mode change began and recover the expected Q65 message afterward, with no stale
rows/TX/UDP errors. This is an obsolete-work latency improvement, not whole-app
speedup or sensitivity improvement.

The separate --coherent-average app control also passes and retains 1,177 ms of
residual native work, confirming the deliberate opt-out rather than silently
changing averaging behaviour. It recovers the strong message after return. This
is not a complete sensitivity/accumulation test of weak coherent sequences.

Native A/B was rebuilt from the original 3529dae3 Q65 engine/worker/header and
continuity test versus current candidate, using the same compiler/build settings
and frozen WAVs. Two process runs cover strong 15A/60A/60B and -15/-22/-28 dB
15A inputs: 28 calls per side, identical complete output rows. Weak-input profiling
explicitly permits missing messages; it does not relabel them as successful
decodes. Evidence: q65-cancellation-native-ab/comparison.json, per-run logs and
source/binary hashes. run-q65-native-ab.py retains the reference-build procedure.
Q65-only candidate.patch/source-record.json are an incremental delta, not a full
source reproducer of the app's other local optimisations.

Expanded Q65 CTest covers retired requests followed by fresh slots, monotonic
retirement, nested cancellation scopes, averaging opt-out and thread isolation.
Nine focused CTest suites pass (24.99 s); see q65-final-tests.log. Temporary
instrumentation is removed, sources restored and the normal app rebuilt. This
follow-up is local and is not in the already delivered Windows A/B kit. Windows
compilation/runtime and wider live/coherent weak-signal acceptance remain open.

### Two-application Q65 virtual-audio integration (2026-11-14)

Extended the existing BlackHole two-app lab to Q65-15A while retaining FT8 as
default. Each instance has fresh private settings and fixed virtual input/output
devices, CAT disabled, loopback UDP sinks and independent test identities.
Inherited decoder/Qt/OMP overrides are cleared; both legacy decodium4.ini and
ft2.ini test-area files are restored. The verifier checks the actual runtime mode.

The first Q65 run is INVALID: both apps exit normally and no UDP errors occur,
but an arbitrary 10-second TX trigger hits the late-slot guard, leaving audio
peak zero and no received messages. This is not decoder sensitivity evidence.
No production TX policy was altered to bypass this result.

Aligned Q65 now uses a temporary QML timer in the laboratory build's shared QML,
checks the TX callsign/mode and requests ordinary sendTx(6) at the next even UTC
15-second slot after startup readiness. It does not force immediate TX. QML bytes
are restored by the lab cleanup. The second run passes: actual Q65-15A on both
instances, exits 0/0, audio peak 0.40198, onset approximately slot +1.017 s,
receiver persistence contains CQ 9H1SR JM75 with the native q0 annotation, zero
UDP errors. Evidence: two-app-q65-after-v2.log and
two-app-q65-after-artifacts/{result.json,pair-aligned-rx.log,pair-aligned-tx.log,
source-record.json}. QML and legacy-test INI hashes match the exact pre-lab
reference. This validates native TX/audio/RX integration over virtual audio,
not RF, older Windows hardware, weak-signal sensitivity or active cancellation
during live reception. The separate native A/B and mode-change tests cover their
respective cases; no broader equivalence claim follows from this single loopback.

### FST4 resource-lifecycle inspection (2026-11-14)

Static inspection only; no FST4 production change or speed claim. The explicit
ftx_fst4_reset_runtime_state_c function clears PersistentState and SyncCache
under the shared runtime mutex. Repository callers found in test_qt_helpers.cpp
are test paths, not evidence of a mode-switch reset. SyncCache retains four
complex<float> vectors of 8*nss elements; supported periods give nss 36 to 42,
so the logical element payload is at most 10,752 bytes (allocator capacity and
other persistent state excluded). Clearing this small cache alone cannot be
presented as a substantial memory optimisation.

Large localAudio and bigfft buffers in decodeFst4Lines are automatic objects
released when the call returns. Long-period peak allocation and queued requests
need runtime measurement separately; process RSS need not immediately fall when
an allocator receives freed memory. Existing FST4 coverage lives in the general
Qt helper test, including a documented skip when the synthetic 15-second fixture
produces no stable decode. Such a skip is not sensitivity or decode-parity proof.
Next FST4 work needs a frozen decodable corpus and active-work measurements before
changing lifecycle or cancellation. Windows kit remains frozen and unchanged.

### FST4 frozen-recording native profiling (2026-11-14)

Added opt-in fst4_performance_probe and profile_fst4_worker.py. The probe invokes
the production native worker directly, with private temporary data storage and
no GUI/audio/network. The runner validates a 60-second mono 12 kHz 16-bit WAV,
records binary/input hashes, fixes OMP_NUM_THREADS=2, retains complete native
rows and requires identical rows across three calls plus CQ K9KFR EN71.
Input: WSJT sample FST4+FST4W/210115_0058.wav; SHA256
0dd5f25cfba45835bcde54ee60fbff7500c95c4f7b9b25f98a57fe319013987f.

Two independent processes, three calls each: 2389-2424 ms per native decode,
all six calls return the same five rows, including the expected CQ K9KFR EN71.
Additional rows are preserved, not certified as valid transmissions. The first
process measured maximum RSS 31,342,592 bytes via macOS time -l; this includes
the probe/runtime and is not a measurement of the full GUI or retained cache.
Evidence: fst4-native-profile/ and fst4-native-profile-repeat/ under the
performance artifact directory. Build and whitespace checks pass.

This supplies a decodable frozen input and a repeatable native performance
baseline for forthcoming FST4 lifecycle work. It is not original-3529 A/B,
Windows performance, a sensitivity sweep or a production optimisation. No FST4
production source was changed. Investigate obsolete queued/active work next:
the observed roughly 2.4-second call is a more relevant target than the small
synchronisation cache, subject to preserving current-session decodes.

### FST4 session retirement candidate (2026-11-14)

Added worker-owned monotonic atomic retirement and scoped thread-local
cancellation, wired to explicit bridge session invalidation. New slots do not
retire earlier valid work. Checks occur before/after the shared runtime lock,
between blanker passes and between candidate operations. No forced thread stop,
mid-FFT interruption or decode-setting changes. Requests without a worker scope
retain legacy completion. FST4W opts out because it persists learned callsigns;
retired results still return an empty completion. This exclusion remains open.

The frozen 60-second recording returns identical full rows before and after
(three reference calls, six candidate calls including recovery sequence). The
pre-change FST4 worker/header are identical to original 3529dae3, verified by git;
this is native FST4 source-level A/B, not a rebuild of the entire original app.
The active-work probe observes the shared runtime busy, waits 40 ms, retires the
request, and measures join latency 1.257 ms. Stale and queued retired callbacks
are empty; lowering the retirement boundary does not revive the queued request;
a fresh serial recovers CQ K9KFR EN71. Follow-on calls preserve every baseline
row. Probe now rejects residual latency above 500 ms. This timing threshold is
a lab check, not a universal old-hardware guarantee.

Evidence: fst4-cancellation-after/, fst4-cancellation-active.json and
fst4-cancellation-comparison.json (source hashes and full-row comparison).
Actual GUI mode-change timing, broader FST4/FST4W corpus, negative-control
validation and Windows runtime remain pending. Frozen Windows ZIP unchanged.

Final app build passes (pre-existing deployment-target linker warnings). Rebuilt
probe with the 500 ms gate also passes: 0.653 ms residual, recovery and complete
row equality confirmed; fst4-cancellation-active-final.json records this run.

### FST4 cancellation negative control and application path (2026-11-14)

The temporary negative control disables only the worker cancellation scope.
The native active-work gate then exits 3 as required; restoring exact source
bytes and rebuilding passes with exit 0. Artifacts and restore hash are in
fst4-cancellation-negative/. This proves the test can reject missed cancellation,
not merely empty stale callbacks. No mutant is left in sources or binaries.

Added profile_fst4_mode_switch.py using the isolated existing application harness.
The first application run recovers the expected message, emits no stale FT8 rows
and has zero UDP errors, but fails the strict active-at-switch proof because
native-end and setter-start coincide at millisecond resolution. It is retained
as an invalid timing run, not a successful acceptance result.

The second run uses a steady-clock fractional-millisecond native/setter clock;
QML event timestamps remain wall-clock and are not compared across domains.
The strict ordering now proves decode active at mode-change start. Native work
ends 2.306 ms after setter start, during the setter (5.729 ms total). Thus the
signed end-minus-setter-return value is negative; it is not negative execution
time. FT8 has zero stale rows; return to FST4-60 decodes CQ K9KFR EN71; no TX,
zero UDP errors, exit 0. Evidence: fst4-mode-switch-after-v2/results.json and
native-1.log. All temporary hooks/QML restored byte-for-byte and normal app
rebuilt successfully. This is a real bridge/UI file-replay test, not RF or Windows
hardware evidence. Broader FST4/FST4W sensitivity/lifecycle coverage remains open.

### Four-mode application lifecycle A/B (2026-11-14)

Extended profile_mode_cycles.py with paired optional FST4/Q65 inputs. Original
FT8/FT4 flow remains the default; extended mode cannot mix legacy injected
callback/interrupt hooks. The probe explicitly uses depth 3, two threads, Low CPU
off, coherent average off and deep search off. Both test-area decodium4.ini and
ft2.ini are backed up/restored byte-exactly, along with QML. One process per
version performs three FT8/FT4/FST4-60/Q65-15A cycles with four-second dwell.

Original full app SHA c026d648... and current eccb131b... both pass all twelve
expected-message recoveries, cleared display checks, no TX and zero UDP errors.
Fixture hashes match. Both show RSS rising and falling rather than monotonic
retention over these short runs: reference idle RSS 502.38-594.77 MiB, candidate
497.98-590.02 MiB; final values 502.38 and 497.98 MiB. This does NOT establish
memory savings or absence of leaks. Per-thread activity/allocator retention
remain unmeasured; no worker eviction is justified from RSS alone.

FST4 expected-row latency is approximately 3.18-3.19 seconds in both versions.
First candidate Q65 row takes 467 ms versus 97 ms reference, then 101/98 ms versus
94/97 ms. The 100 ms QML polling, initialization and single-process ordering
limit inference; this first-activation discrepancy requires repeated controlled
measurement, not a claim of either speedup or proven regression.

Evidence: four-mode-lifecycle-before/, four-mode-lifecycle-after/ and
four-mode-lifecycle-comparison.json. This tests expected-message recovery only,
not equality of every decode row; native A/B supplies separate scoped evidence.
No production changes this stage. Windows kit stays frozen.

### Repeated first-activation discrepancy check (2026-11-14)

Repeated the same four-mode lifecycle sequence in four fresh app processes,
ordered original/candidate/candidate/original, one cycle each. All 16 expected
message recoveries pass, with zero UDP errors and restored QML/test settings.
Original executable hash c026d648... and candidate eccb131b... match the prior
three-cycle measurements. No production edit or compilation occurred during
these timings. Evidence: four-mode-repeat/{1-before,2-after,3-after,4-before}/
and summary.json; repeat-four-mode-ab.py preserves the sequential procedure.

First Q65 expected-row latency is 98/98 ms original and 106/90 ms candidate.
FT8 is 208/206 ms original and 208/206 ms candidate; FT4 167/183 versus 169/169;
FST4 3188/3188 versus 3191/3192. These are 100 ms QML-polled row observations,
not precise native execution times. The earlier candidate Q65 467 ms outlier
does not reproduce here. This resolves the immediate need to investigate a
repeatable regression in this sequence; it does not prove that occasional
scheduling/startup stalls cannot occur. No speedup follows from these numbers.
Further priorities remain representative RX/UI/service profiling, FST4W and
broader corpus coverage, and target Windows hardware feedback.

### Current candidate busy live-RX profiling (2026-11-14)

Updated the RX harness to restore both legacy test INIs (decodium4.ini/ft2.ini)
byte-for-byte. Current candidate eccb131b... receives busy-902 over BlackHole,
two explicit FT threads, Low CPU off, five consecutive slots, 105-second process.
One run samples stacks for 10 seconds; the following run disables sampling.
Both pass with 60 rows: all twelve manifest messages exactly once in each of
five slots, verified by full per-slot multisets, zero UDP errors, exit 0 and
verified runtime profile. This is current-candidate functional load evidence,
not a new original-versus-candidate speed comparison or long-duration soak.

Sampled main thread shows 301/598 observations waiting for render synchronisation;
render thread shows 526/598 waiting for CAMetalLayer nextDrawable. Most other
thread samples are condition/poll waits. These are wall-time stack observations,
not CPU attribution or proof of GPU saturation. No service hotspot or idle-worker
CPU problem is established by this capture; removing threads is not justified.

A single 680 ms watchdog stall occurs during the sampling window (sample begins
19:10:12, stall reported 19:10:15); the unsampled repeat reports no watchdog stalls
above its 90 ms threshold. Sampling may perturb execution: causal attribution is
not proven from two runs. Absence of watchdog reports is not a complete frame-time
or interaction-latency distribution. No decode is lost in either run.
Evidence: latest-busy-rx-profile/, latest-busy-rx-unsampled/ and
latest-busy-rx-check.json. Target i5/Windows, RF, service behaviour with external
networks and broader workloads remain separate acceptance requirements.

### Portable FST4 cancellation regression gate (2026-11-14)

Added test_fst4_decode_cancellation to CTest, using QtCore/QtTest and standard
C++ threading/futures with no downloaded WAV, macOS APIs, GUI, audio or network.
It checks nested scope restoration, thread isolation, fresh/legacy serial and
FST4W-disabled scope semantics, plus actual worker retirement while the native
runtime mutex is held. RetireThrough(4) followed by (2) cannot revive request 4.
The worker must emit one empty completion without waiting for the busy native
runtime. The test releases the mutex and joins before assertions, including
failure paths, so a broken cancellation does not deadlock the runner.

Negative control disables the actual worker cancellation scope: CTest correctly
fails (exit 8). Exact production bytes restored, rebuild and test pass (exit 0).
Evidence: fst4-ctest-negative/; source restoration SHA 28b3df230d92efd2f32aa02f7146e23df42eae90721b812e0cc218db37e81a1e.
Seven focused suites pass sequentially in 4.34 seconds: Q65 continuity, FT4
cancellation, WSPR queue, lab storage, two WSPR resource cases and new FST4 gate.
See decoder-lifecycle-final-tests.log. This supplements, rather than replaces,
the real recording recovery and active application mode-switch tests.

The new target uses portable APIs, but Windows compilation/runtime of these
follow-ups is still unverified. Frozen Windows kit is not modified. No additional
production code change in this step; whitespace checks pass.

### Windows follow-up compilation launched (2026-11-14)

Updated the already authorised perf/windows-ab-candidate laboratory branch from
6f2bbb82 to 21d4c70fe with only the reviewed WSPR cleanup, Q65/FST4 retirement,
associated regression/profiling targets and expanded Windows test gate. Root
worktree stays uncommitted; baseline branch and delivered ZIP are unchanged.
The workflow is manually dispatched with no release_tag: release publishing
remains excluded by its existing condition. No main push, merge or release.

Run 43625741569:
https://github.com/elisir80/Decodium-4.0-Core-Shannon/actions/runs/43625741569
Current evidence snapshot: windows-followup-run.json. Compilation and new Windows
runtime tests are pending; dispatch success is not build success. The original
Windows kit still pins candidate 6f2bbb82 and its verified artifact hashes.

### Twenty-slot candidate RX soak (2026-11-14)

Current candidate eccb131b... ran for 330 seconds with busy-902 via BlackHole,
20 consecutive 15-second audio slots, explicit two threads, Low CPU off and no
stack sampler. Acceptance checks every slot against all twelve fixture messages,
including multiplicity: 240/240, no extras or missing rows. All 20 logged primary
dispatches have effectiveDepth=3. Exit 0, zero UDP errors, no watchdog stall report
over its 90 ms threshold; private settings restored by the completed harness.

One-minute median process RSS after seconds 30/90/150/210/270: 548.17, 475.83,
477.00, 477.81, 478.17 MiB. Initial RSS falls; the later small rise may include
normal retained history/allocator state and is not proof of a leak or its absence.
This 5.5-minute run extends local sustained-load evidence, not an hours-long soak,
a comparison proving speedup, hardware/RF acceptance or full frame-time coverage.
Artifacts: busy-rx-soak-20slots/{result.json,acceptance.json,stdout.log,
rx.diagnostic.log}; verifier verify-busy-soak.py in the performance directory.

The same live Windows run 43625741569 advanced from MSYS2 setup to mandatory CAT
gates during the soak. It has not been restarted; final lifecycle tests, packaging
and upload remain unconfirmed until the workflow finishes.

### Windows follow-up gates passed, full app still building (2026-11-14)

Run 43625741569 reports success for mandatory CAT gates, expanded performance/
decode-lifecycle regressions and Windows PowerShell harness. It is now in Build
Decodium. Clarification: the earlier CAT-stage observation established compiled
test dependencies, not completion of the full application build. Full executable,
installed-runtime verification and final non-empty artifacts remain pending.
Authoritative step snapshot: windows-followup-status.json. The run remains live;
no restart/cancellation or release action was taken.

### Follow-up artifact collection armed (2026-11-14)

collect-windows-followup.py watches only existing run 43625741569 and verifies
exact commit 21d4c70feb5fddb6ab54c11013e5e12920dfc6aa. It reuses the established
collector's success-only download, non-empty portable artifact, executable/QML/
DLL and executable-hash checks, writing windows-followup-artifacts/ separately.
Observation errors trigger re-observation of the same run, never cancellation,
restart or workflow dispatch. Full workflow logs are retained after completion.
Collector execution session at launch: 42574; authoritative live/terminal status
must be rechecked before assuming it remains active. Latest observation is full
Build Decodium in progress, not final build or artifact success.

### Q65 coherent-enabled native A/B extension (2026-11-14)

Extended profile_q65_worker.py with an explicit coherent-average option requiring
frozen WAV input. Its temporary test uses three distinct UTC slots (12:00:00,
12:00:15,12:00:30) with coherentAvgEnabled=true. Original 3529dae3 engine/worker/
header/test are rebuilt against the same native toolchain versus current sources.
Six calls per version over frozen -15 and -22 dB WAVs return identical complete
rows. The -15 input yields one expected message each call; -22 yields none in
both versions. No miss is relabelled as a successful decode.

This checks unchanged output with the averaging option enabled, not independent
noise accumulation/sensitivity, long-session cache retention or cancelled coherent
work (which deliberately keeps its completion semantics). Repeated identical
noise in one frozen WAV is an explicit limitation. Temporary sources restored
byte-exactly and normal test/app rebuilt. Evidence: q65-coherent-native-ab/ with
input/source/binary hashes, raw logs, comparison.json and restore-build.log;
run-q65-coherent-ab.py preserves the procedure. Windows collector session 42574
continues observing the same full-app build; no new workflow was dispatched.

### FST4W fixture discrepancy reproduced on original (2026-11-14)

Added an opt-in maintained fst4_performance_fixture simulator target and a native
probe --fst4w option (local tooling only, not part of in-flight Windows commit).
Generated 120-second K1ABC FN42 33 at 1500 Hz, SNR -10, zero Doppler/delay with
WSPR hint T. Generator log confirms that message and its encoded channel symbols.
All three candidate calls instead emit YY1AIB FG74 20. The expected-message
assertion correctly fails; this is NOT a passed functional FST4W test.

Rebuilt original 3529dae3 FST4 worker/header against the same frozen PCM and
native probe with cancellation-test code omitted. All three original full rows
are identical to the candidate's unexpected rows. The discrepancy therefore
reproduces before these optimisations. This does not identify whether generator,
decoder or their bit-format contract is responsible, and it is not valid sensitivity
evidence. Do not change decoder arithmetic or accept empty/equal-output tests to
hide it. A known-good independent FST4W fixture/encoding reference is needed next.

Evidence: fst4w-frozen-profile/{generator.log,manifest.json,result.json,
original-result.json,comparison.json,original-build.log,restore-build.log} and
check-fst4w-original.py. Sources restored byte-exactly and normal app/probe rebuilt.
Windows run 43625741569 remains live in Build Decodium according to collector
42574; neither run nor delivered ZIP has been replaced.

### FST4W message-bit isolation (2026-11-14)

Added native probe --roundtrip-fst4w for the discrepancy investigation. It calls
the maintained FST4W encoder, takes its first 50 payload bits, appends the exact
27-bit suffix used by unpack_fst4w_50bit, then invokes decode77 with fresh context.
It successfully returns K1ABC FN42 33, exit 0. Evidence:
fst4w-frozen-profile/message-roundtrip.json and roundtrip-build.log.

This rules out the simple payload-to-text roundtrip for this message as the
source of the observed wrong audio decode. It does not independently validate
channel coding, waveform synthesis, synchronisation or LDPC bit recovery; the
remaining discrepancy lies along that broader signal path or their interaction.
No production encoder/decoder arithmetic changed. A known-good independent
reference remains needed before accepting this FST4W fixture for sensitivity.
The existing Windows collector continues to report Build Decodium in progress.

### FST4W frozen encoder and ideal-channel isolation (2026-11-14)

Rebuilt and ran test_qt_helpers::fst4_encoder_matches_frozen_reference: pass,
zero skips. The current encoder's 74 message/CRC bits and 160 tones match the
repository's frozen K1ABC FN42 33 vector (also matching the simulator log).
This is a frozen repository reference, not newly verified independent RF data.

Added probe --ldpc-fst4w: extract the four 30-tone payload blocks at offsets
8/46/84/122, invert the documented mapping used by the maintained LDPC simulator,
and feed ideal +/-10 bit likelihoods to decode240_74_native with the same
66/2/3 search parameters as the native FST4W path. It returns success, zero hard
errors and zero differing payload bits. Neither encoder nor LDPC is modified.

Evidence: fst4w-frozen-profile/reference-encoder-test.log and
perfect-channel-ldpc.json, with their build logs. This narrows the unexpected WAV
decode to the audio/synchronisation/metric path or its interaction with later
search; it does not prove which component is wrong. The full synthetic audio
case remains failed and is not used as successful FST4W sensitivity evidence.

### FST4W signal isolation and independent reference identified (2026-11-14)

Generated three further 120-second cases: simulator noiseless mode (SNR=99),
noiseless with DT=1 s, and SNR=0. Three native calls per case all miss the intended
K1ABC FN42 33 and return unexpected rows. Audio is nonempty; FFT peaks fall around
1500/1501.46/1502.93/1504.39 Hz as expected for these generated tones. The failure
is therefore not explained merely by the -10 dB test being too weak. This is not
a sensitivity test or proof of the root cause. Evidence: fst4w-signal-isolation/.

Official WSJT-X guide provides an independent recording:
https://wsjt.sourceforge.io/wsjtx-doc/wsjtx-main-2.7.0.html (section FST4W)
201230_0300.wav, T/R 1800 s, Deep, NB 0%, centre 1500 Hz and tolerance 100 Hz.
The guide's example shows DL0HOT JO60 30 (also visible in the official 2.6.0 PDF).
Download started from the WSJT SourceForge sample collection into
fst4w-independent-reference/; do not treat a partial file as accepted input.
Native probe --fst4w-reference now explicitly uses the documented 1800-second
period, tolerance 100 and NB=0 encoding (nexp_decode=768), depth 3. The reference
has not yet been decoded; no success is claimed. Production decoder unchanged.

### Independent FST4W recording A/B passes expected-message recovery (2026-11-14)

The first download timed out at 24,927,558 bytes (curl exit 28); resumed the same
file with HTTP range. Completed download is 43,200,254 bytes and contains exactly
21,600,000 mono 12 kHz 16-bit samples (1800 seconds), verified before decoding.
SHA256: 5c3c84dbbdcae734776ee8b681666ff26bf3399b4b2aa5cc02a7c1ca0278fd17.

With documented depth 3, NB 0, centre 1500 Hz, tolerance 100 Hz, all three candidate
calls decode DL0HOT JO60 30 at -44 dB, DT 0.3, frequency 1433 Hz, matching the
WSJT-X guide example. Candidate native calls take 1.05-1.10 s on this Mac; no
speedup or old-hardware inference. Two additional messages also appear; their
validity is unconfirmed and they are retained in the raw result.

Rebuilt original 3529dae3 worker/header with the same reference probe and PCM:
all three original calls return exactly the same full rows, including both
additional messages. Thus expected-message recovery AND complete native-row
parity pass for this independent FST4W reference. This does not resolve the
separate synthetic 120-second discrepancy or certify extra decodes as real.

Evidence: fst4w-independent-reference/{manifest.json,acceptance.json,result.json,
original-result.json,comparison.json,original-build.log,restore-build.log};
run-fst4w-independent.py and check-fst4w-reference-original.py preserve procedures.
Temporary original sources restored byte-exactly and normal app/probe rebuilt.
No production arithmetic or FST4W retirement policy changed.

### Extended Windows kit preflight (2026-11-14)

Inspected the existing kit before adding Q65/FST4 inputs. Its fixture iteration
and exact full-row comparator are generic, but Run-Test.ps1 checks the expected
message with exact text equality. Q65 retains padded q0 annotations, so a plain
expected message would reject a correct Q65 result. Any extension must narrowly
normalise that expected-message check while leaving full-row A/B comparison
unchanged, and validate both positive and substituted-message negative cases.
No fixture or harness change has yet been made; original five-input ZIP unchanged.
The live collector still reports Build Decodium for run 43625741569.

### Q65 expected-message gate for extended kit (2026-11-14)

Added Test-ExpectedDecode to the shared PowerShell comparator file and load it
before Run-Test validates cases. Only Q65 expected-message smoke checks normalise
whitespace and permit an anchored optional q plus one/two digits annotation.
Wrong messages, prefixes, trailing text, malformed annotations and lowercase
substitutions are rejected. Other modes preserve the existing equality check.
Full A/B message multiset comparison is unchanged and still rejects q0 versus q1.
Local PowerShell positive/negative cases pass alongside the original comparator
checks. These local harness edits are not in the already running Windows commit
or the delivered ZIP; extended fixture packaging and Windows execution remain
pending. No production code change.

### Extended seven-fixture kit staging and local probe (2026-11-14)

prepare_windows_performance_kit.py now accepts --extended-modes, adding the
verified FST4-60 recording and Q65-15A frozen -15 dB input. Default remains five
fixtures. The seven-fixture staging manifest has ready=false and no executable
builds; all fixture hashes verified. It is not delivered as an executable kit.
Run-Test computes whole-app timeout from fixture count (minimum retains original
450 seconds, plus the existing 20-second outer allowance), avoiding premature
termination of extended runs. Extended instructions state 10-15 minutes.

PowerShell parsing and comparator positive/negative tests pass locally. The exact
seven-fixture QML probe also passes on the normal current macOS app; Q65 expected
message verification allows only its documented narrow annotation pattern, and
raw rows remain intact. Probe-result and logs are under windows-extended-probe-after/.
This is local probe logic validation, not Windows runner validation or a new
original/candidate equality result. Delivered five-fixture ZIP stays unchanged.

Windows run 43625741569 has now completed Build Decodium and is preparing the
portable bundle; installed runtime and upload remain pending. Collector 42574
is still attached to the same run.

### Windows follow-up complete and extended kit verified (2026-11-14)

Run 43625741569 concludes SUCCESS on exact commit
21d4c70feb5fddb6ab54c11013e5e12920dfc6aa. Full log confirms 10/10 performance/
lifecycle tests, CAT gates, PowerShell harness, SSTV sharing, installed Windows
runtime and portable/installer uploads. Publish release assets is SKIPPED.
Collector session 42574 finished exit 0. Portable artifact ID 14126713553,
99,304,006 bytes; executable SHA256
d62021ef811cf7554863ffe65f132165b0f872c028071b2d90c82a32b1a61a39.
Metadata, full logs and downloaded files: windows-followup-artifacts/.

The extended exact-QML sequence also ran on the original macOS application;
all seven per-fixture message multisets match the current app, including duplicate
messages and Q65 annotations. Counts 1/12/9/1/8/6/1 in both; expected-message
checks pass, no UDP errors, both exits 0. See windows-extended-probe-comparison.json.

Packaged a NEW separate ready ZIP, not an overwrite:
Decodium4-Windows-AB-Extended-2026-11-14.zip
200,469,290 bytes; SHA256
3bfbf5467b99a308f366f06c9182354a137ae59eaed38685b195f38561e0a89d.
All 4,192 Windows build files, seven WAVs, both source patches and ZIP CRC checked.
Original ZIP SHA remains bcbf4baf2c37aa6e67e813a333a69ebe376bf56b48d8e1776d15a52bf51a0e0f.
Final validation: windows-extended-final-validation.json. Baseline stays f037f84e;
new candidate is 21d4c70fe. Extended local PowerShell changes are parser/comparator
tested; the in-flight CI tested the earlier harness version, so actual extended
Windows runner execution remains a target-PC acceptance step, not a claimed CI
result. No release published and root worktree remains uncommitted.

For new target-PC tests use the extended ZIP: extract, close Decodium, run
AVVIA_TEST.cmd, then CREA_REPORT.cmd and return report.zip. Keep any earlier
report as evidence of its separate pinned build. Hardware/RF and broader
stateful/long-session acceptance remain open; goal is not complete.

## FT8/FT4/FT2 priority — three-mode lifecycle comparison

Added optional --ft2-wav to profile_mode_cycles.py, preserving existing defaults
and rejecting incompatible fixed-step instrumented probes. Generated clean
CQ K1ABC FN42 at 1500 Hz with ft2_make_test_wav (45,000 samples, 600 ms offset).
Original 3529dae3 and current eccb131b app each complete three cycles, nine
expected-message recoveries, with cleared lists, no TX, no UDP warning, exit 0.
Both QML and legacy test INIs restored with byte assertions. Evidence:
ft842-cycles-before/result.json, ft842-cycles-after/result.json and
ft842-cycles-comparison.json. These are expected-message recovery checks, not
full decoded-multiset equality or an active-decode cancellation test.

Final idle RSS approximately 661 MiB original versus 643 MiB candidate in this
one sequential pair. Both rise during warm-up and flatten over the final cycle;
no general memory saving or long-session leak claim follows from this short run.
The user's latest scope is FT8/FT4/FT2 and general stability; no further work
on other modes is planned. No i5 is available; hardware-specific acceptance
remains explicitly unverified, and any available Windows PC can supply a report.

### Settled full-row FT8/FT4/FT2 comparison

Added --settled-rows: wait at least 150 timer ticks (100 ms), 30 stable
row-list ticks and decoder idle before accepting the case; retain the 30-second
timeout and existing restoration/isolation. Capture raw messages and row fields.
Original/current each complete one clean sample for FT8, FT4 and FT2; all
message multisets and full rows (message/mode/SNR/dt/frequency) identical,
one row each. No TX or UDP errors, exit 0. Exact step/mode ordering and
idle state validated on both saved reports. Evidence: ft842-settled-before/,
ft842-settled-after/, ft842-settled-comparison.json. This closes the early-first-
message observation gap for these clean files only; crowded/weak FT2 and
sustained live-audio stress remain separate work. No production DSP changed.

### FT2 continuous virtual-audio first pair — difference retained

Extended profile_rx_runtime.py with --mode FT2, 3.75-second fixture/slot
validation and alignment, FT2 decoder-metric thread verification, explicit mode
in reports; original FT8 defaults preserved. Played the identical clean 45,000-
sample PCM eight times through BlackHole, 65 seconds per app, two threads,
Low CPU off. Both original and current persist eight exact CQ K1ABC FN42 rows,
exit normally, no UDP warning, both test INIs restored. Current also persists
two additional CQ K1ABC FN42 rows marked '?'; original does not. Full multiset
equivalence therefore FAILS in this pair; do not hide extra rows or call this
an overall no-regression pass. Startup logs show async and synchronous FT2
workers, depth 3 and requested/active threads 2. Injection offsets differ by
about 2 ms; this alone does not establish the cause. Next investigate repeatability
and timing sensitivity with alternating runs and retain full raw rows.
Evidence: ft2-live-before/, ft2-live-after/, ft2-live-comparison.json. This is
virtual audio, not two transmitting apps, RF or a long-duration soak.

### FT2 live reversed repeat pair

Repeated baseline then candidate with the same eight-slot fixture/settings,
65 seconds each, no sampling. Both now persist exactly eight CQ K1ABC FN42
rows and no '?' extras; no UDP errors, normal exit, isolation restored. Across
the chronological candidate/baseline/baseline/candidate series all four recover
eight expected rows. The first candidate's two additional uncertain rows do
not repeat. Preserve the first failed full-multiset comparison: this establishes
run variability, not its cause or universal equivalence. Logs identify the
extra rows as async_raw a8 at 1481/1483 Hz; FT2DecodeWorker marks '?' for nap
nonzero and qual<0.17. FtxFt2Stage7's a8 path checks previously received message
bits. Both source files and FT2 ready handlers are unchanged from 3529dae3;
indirect timing effects remain possible. Evidence: ft2-live-before-repeat/,
ft2-live-after-repeat/, ft2-live-repeat-comparison.json. Next use frozen noisy/
crowded PCM and longer sessions, keeping these clean high-SNR observations
separate from sensitivity measurements.

### FT2 noisy virtual-audio comparison

Generated frozen FT2 CQ K1ABC FN42, 1500 Hz, gain .03, noise -12 dB relative
to generator signal RMS, seed 20261114, 600 ms offset; 45,000 mono 12 kHz
samples, peak 12383, no saturated samples. SHA256
eb50d60d74d15514958bfbe509f74876e469b34095f26400ff6b2d5b2ad8d491.
This SNR parameter is not a calibrated 2500-Hz sensitivity rating.
Both baseline and candidate recover eight exact messages at eight distinct
reported times over eight repeated slots. Both also persist two '?' messages;
full message multisets match (10 rows each), numerical row equality not asserted.
Baseline logs identify its extra rows as async a8 after the injected sequence.
Thus uncertain extras also exist in original 3529dae3, while the first clean
pair's variability remains unresolved. No decoder thresholds or AP policies
changed. Both exit 0, profile verified two threads/Low CPU off, no UDP warning,
test INIs restored. Evidence: ft2-noisy-k1abc.json, ft2-noisy-live-before/,
ft2-noisy-live-after/, ft2-noisy-live-comparison.json. Repeated frozen noise is
not independent trials, crowded-signal coverage or an extended soak.

### FT2 noisy 80-slot soak (candidate)

Current eccb131b app ran 330 seconds, eight-slot test extended to 80 repeated
3.75-second noisy frames, two threads and Low CPU off, no stack sampling.
All 80 expected messages occur exactly once in their respective slots (not just
an aggregate count). Two additional '?' rows appear after the final transmitted
slot and remain in the report. Normal exit, no UDP warning, no recorded
MAINWATCH stalls above the existing 90-ms threshold. Both legacy test INIs
restored by byte assertion. RSS minute medians beginning at 30/90/150/210/270 s:
466.69/473.16/355.73/356.02/346.78 MiB. These sampled resident values do not prove
absence of a heap leak or a general memory saving. This is one candidate soak,
not a 330-second A/B comparison, independent-noise trials, RF or hours-long use.
Evidence: ft2-noisy-soak-80/result.json and acceptance.json.

Added summarize_ft2_soak.py to check UTC slot coverage while retaining all
extra rows and reporting sampled RSS/watchdog entries. On the prior eight-slot
report it passes; controlled copied-report mutations for missing messages,
wrong messages and a same-count duplicate all fail coverage as intended.
Evidence: ft2-soak-verifier-checks.json. Python syntax and diff whitespace checks
pass. No production decoder code or Low CPU behaviour changed in this step.

### FT4 continuous audio A/B

Extended profile_rx_runtime.py to FT4 (7.5-second slots and FT4 metric thread
validation). Original 72,576-sample clean CQ 9H1SR JM75 WAV padded with 17,424
silent samples to 90,000; original samples unchanged. Frozen WAV SHA256
1ef0cf7f7f7fdbb1f673352c2f56c5c265a35352ff2445bafa770840c1e4383c.
Original/current app each ran 90 seconds with eight repeated slots, two threads,
Low CPU off, BlackHole audio, no stack sampling. Exactly one expected message
in each of slots 0..7, eight total, no extras. Both exit 0, no UDP warning, test INIs restored by byte assertion. Baseline
has no watchdog report; candidate records TWO stalls (average 357 ms, maximum
368 ms, threshold 90 ms) while monitoring. This responsiveness difference
requires investigation; the message-slot comparison alone passes; not full numerical-row equality, weak-signal sensitivity,
long soak or whole-app speed proof. Evidence: ft4-live-before/, ft4-live-after/,
ft4-live-comparison.json and ft4-live-9h1sr.json. Python syntax and diff checks pass.

### FT4 timer-stall attribution diagnostics

The first FT4 candidate run has a 327-ms QTimer dispatch with unnamed receiver,
reported near two 357/368-ms watchdog gaps while monitoring. Existing core
spectrum/UTC/CPU/async timers are already named; source inspection alone cannot
identify this unnamed timer. Added pre-dispatch QTimer interval and owner-class
snapshots to the existing slow-event log (same 90-ms threshold). QML context is
now queried after dispatch only through a QPointer guard, avoiding a dangling
receiver if a callback destroys it. No decoder policy or timer intervals changed.
Incremental build and final rebuild passed; existing deployment-target linker
warnings remain. Whitespace check passes.

New LOCAL diagnostic binary SHA256:
229c412fba43869d2118eb3ec3dfaf8aedfa0ffb63c5c849e438f04e064399b5.
The delivered Windows ZIPs and GitHub candidate remain unchanged and do not
contain this diagnostic update. A 90-second FT4 repeat receives the same eight
messages, exactly once in each slot, as the original; exit 0, no UDP warning,
settings restored. No monitoring watchdog stall in this repeat. A distinct
188-ms QQmlTimer startup event points to BootLoader.qml, before the audio
sequence, and remains recorded. The original 327-ms unnamed QTimer event did
NOT recur, so its cause is still unproven; no claim that diagnostics fixed it.
Evidence: ft4-timer-diagnostics-after/result.json and acceptance.json,
ft4-timer-diagnostics-final-build.log. Diagnostic metadata for a slow QTimer
still needs an observed recurrence; the slow QML-source path did execute.

### BootLoader asynchronous creation experiment — not retained

Inspection confirmed asynchronous component compilation followed by synchronous
createObject(null). Tried Qt Component.incubateObject with retained incubator,
graphics-before-show finalisation, error handling and existing timeout watching
creation too. Same local 229c412f executable, software renderer, FT8/FT4/FT2
file recovery sequence passes in both. Synchronous comparison logs a 162-ms
BootLoader event; asynchronous run has no such event but records later custom
QQmlTimer events of 363/376 ms and a 383-ms watchdog gap. These later events
do not prove incubation caused them, but overall benefit is unproven. Rolled
back BOTH source and runtime BootLoader byte-for-byte. No incubation change
is left in the application. Preserved trial as BootLoader-incubation-experiment.qml,
results in boot-creation-pair.json and boot-incubation-disposition.json.
Do not compare old/new printed creation milliseconds directly: the old log
captured elapsed before createObject, the trial measured after object creation.
Qt API reference consulted: https://doc.qt.io/qt-6/qml-qtqml-component.html.
Next attribution gap: custom QQmlTimer_QML_* subclasses are not recognised by
the existing exact-class diagnostic, so their source URL is missing. This is
separate from the original unnamed QTimer FT4 event, which remains unresolved.

### Derived QML timer attribution verified

Replaced exact QQmlTimer class matching with QObject::inherits("QQmlTimer")
for the guarded source-context lookup. QML-generated derived timer classes now
retain attribution; threshold, timer frequencies and decoder paths unchanged.
Build succeeded. Local executable SHA256:
405352205bd374f359d4ca9bce1f8ca93cfe4add4551a8e02b9003f0484119d0.
Windows kits/branches remain frozen at their earlier versions.

Controlled runtime test injected one derived QML timer, named labSlowDerivedTimer,
with a deliberate 140-ms busy interval five seconds after Main creation. Log
records 139 ms, receiver QQmlTimer_QML_244 and the correct Main.qml source URL.
FT8/FT4/FT2 expected-message sequence passes, no UDP error, exit 0. Outer finally
restored exact original Main.qml bytes; inner lab restored both test INIs.
This is diagnostic verification, NOT a production stall or performance result.
Evidence: qml-timer-attribution-test/attribution.json, result.json and logs;
verify-qml-timer-attribution.py records the temporary injection/restoration.
The initial unnamed QTimer FT4 stall remains unexplained; BootLoader remains
fully restored to the pre-incubation implementation.

### FT4 sampled attribution and multi-mode comparison guard

90-second FT4 virtual-audio run with ten-second sampling beginning 12 seconds
after injection: eight expected rows, valid profile, no UDP warning, normal
exit, settings restored. During the sample window the watchdog records a
653-ms gap; no corresponding slow unnamed-QTimer dispatch. Sample main thread
has 287/605 observations in QSGThreadedRenderLoop::polishAndSync waiting on a
condition; active render thread has 555/605 in CAMetalLayer nextDrawable.
These are sampled waits, not CPU-cost percentages and not proof of the cause
of the original unsampled 327-ms QTimer event. Profiler perturbation remains
possible; this sampled run must not be used as speed/A-B timing evidence.
Distinct startup BootLoader dispatch: 167 ms. Evidence: ft4-stall-sampled/.

Updated compare_rx_runtime.py to validate mode/period and support 15/7.5/3.75-s
FT8/FT4/FT2 slots, preserving default FT8 interpretation for older reports.
UTC slot matching now tolerates integer-second storage quantisation; raw mode
and message multiplicities remain checked. Out-of-injection rows still cause
rejection (so FT2 post-sequence '?' rows must not silently disappear).
Prior FT4 eight-slot baseline/candidate pair passes strict comparison. Controlled
mutations reject mixed modes, same-count duplicates and substituted messages.
Evidence: ft4-live-strict-comparison.json. No further production changes.

### FT4 two-application virtual-audio proof

Extended the existing two-app lab with FT4 support and UTC boundary-triggered
normal sendTx(6), private per-instance settings, CAT disabled and owned UDP
sinks. Local 40535220 executable in both sessions, Cocoa/software UI; real
TX waveform via BlackHole 2ch, RX on the other instance. Both exit 0, mode
verified FT4 in both logs, receiver persists CQ 9H1SR JM75. Captured peak
0.40198 with 52 active 100-ms windows. No UDP warning. Runtime Main.qml exact
original SHA af17681d restored after probe. Settings restoration follows the
existing lab cleanup; strengthened future runs with byte assertions too.
Artifacts copied without private settings to two-app-ft4-after/. No RF, real
CAT/PTT, Windows, weak-signal sensitivity or two-app baseline equivalence claim.
This complements the earlier original/current eight-slot file-playback A/B.

### FT2 two-application virtual-audio proof

Added FT2 to the two-app lab mode selector. Immediate normal TX6 path on local
40535220 app: two isolated Cocoa/software sessions, RX via BlackHole 2ch and
TX output to BlackHole 2ch, no CAT. TX/RX exit 0, FT2 mode verified in both
logs, receiver persists CQ 9H1SR JM75. Audio peak .40198, 26 active 100-ms
windows. No UDP warning. Cleanup byte assertions verify restoration of both
legacy test INIs; this immediate mode does not inject QML. Artifacts copied
without settings to two-app-ft2-after/. This proves local TX/RX functionality,
not RF, older-PC performance, sensitivity or two-app baseline equivalence.

### Focused Windows FT8/FT4/FT2 kit delivered

Added mutually exclusive --ft-modes-only option to packager; default and earlier
extended kit preserved. Six fixtures: clean/busy/weak FT8, clean FT4, clean/noisy
FT2. README clarifies any available Windows PC is suitable; actual CPU/RAM are
reported, i5 is only a design reference. Local exact-QML validator now also
backs up, clears and restores ft2.ini in addition to the run_case protection of
decodium4.ini. Original macOS and local 40535220 app both pass six cases with
full raw message multisets equal (counts 1/12/9/1/1/1), no UDP warnings, exit 0.
Evidence: windows-ft842-probe-before/, windows-ft842-probe-after/ and
windows-ft842-probe-comparison.json. This is QML logic verification, not Windows
runner execution. Windows binaries remain f037f84/21d4c70, not local diagnostics.

New separate archive Decodium4-Windows-AB-FT842-2026-11-14.zip:
196,895,582 bytes; SHA256
503339ee14dc634e1d66caf5fd8f9e1fcdb789bc8152b0de2f43995fcfa877da.
Ready true; ZIP CRC, all 4,192 build-file hashes, all six WAV hashes and both
source patches verified directly from archive. Evidence:
windows-ft842-final-validation.json. Previous ZIPs remain unchanged. No new
CI run, push or release. Hardware report remains pending; goal is not complete.

### Returned Windows report analyser validated

Completed synthetic unit checks for analyse_windows_performance_report.py:
12 tests pass for valid metrics, same-count message substitution, missing
duplicates, wrong binary/configuration/manifest, invalid/non-monotonic numeric
samples, missing files, unexpected/traversal/normalised duplicate ZIP members,
UDP warning and out-of-order events. Recomputes both A/B pairs rather than
trusting comparison.json. Event order must match fixture start/result pairs
followed by done. CLI now exits nonzero for unequal message multisets while
still writing detailed missing/extra data. No extraction or app execution.
Tests: python3 tests/tools/test_windows_report_analysis.py. Synthetic evidence
only; no Windows hardware report has been received or certified by these tests.

Analyse a returned FT842 report with:
python3 tests/tools/analyse_windows_performance_report.py --report /path/to/report.zip --manifest build/performance-baseline/Decodium4-Windows-AB-FT842-2026-11-14/manifest.json --output /path/to/analysis.json
Use the matching trusted local manifest for an older kit. This local analyser
is not an update to the already-delivered ZIPs. It checks reported evidence,
not cryptographic attestation of the remote machine or hardware identity.

### FT4 24-slot unsampled soak and narrowed stall evidence

Local 40535220 ran 210 seconds, 24 clean FT4 transmissions, two threads/Low CPU
 off. All 24 expected messages appear exactly once in slots 0..23, no extras,
no UDP warning, normal exit, test INIs restored. RSS minute medians after
30/90/150 s: 513.13/475.22/478.66 MiB; no hours-long leak/speed inference.
Evidence: ft4-soak-24-after/result.json and acceptance.json.

No MAINWATCH summary is present, but this is NOT a no-stall pass: MAINDISPATCH
records a 331-ms QTimer event (interval 250 ms, owner DecodiumBridge), with an
adjacent UIDBG gap 382 ms. This occurs at the FIRST FT4 early dispatch, serial 1,
before the injected audio sequence. Worker decoding subsequently takes 89 ms,
so cannot simply be equated with the 331-ms main-thread duration. Separate
BootLoader startup event is 184 ms. Multiple bridge timers use 250 ms; period
timer is a strong candidate because onPeriodTimer calls maybeDispatchFt4EarlyDecode,
but owner/interval plus log adjacency do not uniquely prove that identity.
Static inspection: early dispatch snapshots audio under m_audioBufferMutex,
then constructs metadata and queues work. Worker constructor itself is empty.
Next narrow attribution to the specific timer/callback and its phases before
changing locking or scheduling. The original isolated 327-ms FT4 event and
this 331-ms event may be related; not yet proven identical in cause.

### Named bridge timers and period-phase diagnostics

Named period, legacy-state, performance-metrics and RX-list notification timers
at construction, without changing their intervals or callbacks. onPeriodTimer
now records monotonic elapsed phases for progress notification and FT8/FT4 early
dispatch, logging only phases >=90 ms. This separates synchronous UI signal
work from early-decode dispatch; audio-buffer lock detail is still a next step
only if the FT4 phase is implicated. Build/final rebuild and whitespace checks
passed. Latest LOCAL binary:
4352024029d76dd0823167cabba81d3f423a611d8b035fc5f4ad2fa9906ba7dc.
Windows kits/branches remain unchanged.

90-second unsampled FT4 run recovers exactly the baseline's eight messages in
slots 0..7. No UDP warnings, normal exit, test INIs restored. No period phase
>=90 ms and no RX watchdog gap in this repetition; only the existing 184-ms
BootLoader event. Therefore no causal attribution or fix claimed yet. Evidence:
ft4-period-phase-after/, ft4-period-phase-comparison.json and build logs.

### Bounded FT4 first-dispatch reproduction series

Static inspection finds no direct expensive QML binding attached to periodProgress;
Main.qml references it in startupVisualStageCanRun for FT8. This does not exclude
other synchronous signal work or indirect costs. legacyDecodeQsoProgress and
legacyDecodeCqHint are simple value accessors, not backend/network calls.

Ran four fresh 40-second unsampled FT4 sessions, one transmitted fixture slot
each, same two threads/Low CPU off. All four receive exactly CQ 9H1SR JM75,
exit 0, profile verified, no UDP warning; settings restored. No named QTimer
slow dispatch or PERIODPHASE >=90 ms in any run. Evidence:
ft4-attribution-repeat-{1,2,3,4}/ and ft4-attribution-repeat-summary.json;
repeat-ft4-attribution.py bounds the series at four and stops early on evidence.
The earlier stall remains unproven in cause, not fixed. Do not continue identical
restarts indefinitely or count this as a speed improvement. Retain diagnostics
for a recurrence during other substantive FT8/FT4/FT2 coverage.

### Eight-station FT2 crowded virtual-audio A/B

Added prepare_ft2_busy_fixture.py: uses production TX fixture generator for eight
standard calls at 400/700/1000/1500/1800/2100/2400/2800 Hz, gain .025 each,
frozen seeded Gaussian noise RMS .03. One 45,000-sample frame, peak .24718,
no clipping; not calibrated RF SNR. Frozen SHA256
05a317020cb17d139bc0d8ca6f8d14ecdbe2714d0413918389b8d6ea10a8b5fe.
Generation parameters and individual clean waves retained in ft2-busy-corpus/.

Original 3529 and latest local 43520240 each ran eight repeated slots over
65 seconds, two threads, Low CPU off. Exact expected payload coverage: all
64 messages once per station per slot in BOTH. Full-output equivalence does
NOT pass: original has 7 additional '?' rows; candidate has 11, with different
messages/multiplicities. Keep these differences visible; do not conflate expected
coverage with identical full output. No RX watchdog reports or slow period phases;
only BootLoader startup events 179/177 ms. No UDP warnings, exit 0, test settings
restored. Evidence: ft2-busy-live-before/, ft2-busy-live-after/ and
ft2-busy-live-comparison.json. These are repeated frozen noise, not independent
noise trials, overlapping-carrier capture or sensitivity statistics.

### FT2 crowded baseline-only repeat demonstrates existing variability

Repeated original c026d648 executable, same frozen PCM, two threads/Low CPU off.
All 64 expected station/slot pairs recovered, but this run has 23 '?' rows and
two additional exact N6ABC rows in slots 4 and 6 (89 total). Original first run
had 7 uncertain rows; candidate prior run had 11. Thus full-output variability
exists within the original executable. This does not prove no indirect effect
from optimisations; it prevents attributing differences to them from one pair.
Initial exact-once assertion correctly failed on the baseline repeat; report
records coverage separately from duplicates rather than discarding them.

First original/candidate extras all mapped to slot 8 after injected slots 0..7;
repeat baseline uncertain rows also occur inside slots 1/3/4/5/6/7. Therefore
a tail-only explanation is insufficient. No decoder/AP/filter changes made.
Repeat exits 0, no UDP warnings, settings restored. Evidence:
ft2-busy-baseline-repeat/ and ft2-busy-variability.json. Keep full-output
equivalence unproven, while retaining exact expected station/slot coverage.

### Crowded FT2 direct-file isolation

Used the same eight-station frozen PCM through openWavForDecode, exact settled
Windows-probe QML (15-second minimum, stable rows/decoder idle), in original
and latest local apps. Both produce exactly the eight expected messages with
no extras. Full row fields also match, not only message count. Both exits 0,
no UDP warnings, runtime QML and both legacy test INIs restored. Evidence:
ft2-busy-file-before/, ft2-busy-file-after/, ft2-busy-file-comparison.json.
This separates direct-file decoding from observed live-audio variability; it
does NOT establish a cause for live '?' rows or guarantee identical stateful
async outputs. No production change in this step.

### Focused accumulated-regression suite

Sequential CTest selection completed: 15/15 pass, 59.66 seconds. Covers FT8
TX decode gate/mycall AP, FT2 async ring, profile restart, FT8 BICM fallback and
runtime dispatch, weak/deep and hinted/very-deep decode, FT2 QSO simulation and
legacy-CPU path, decode-list model and QML reset, map cadence, FT4 cancellation,
and laboratory storage isolation. Exact command/output retained in
ft842-final-regressions.log. Python Windows-report analyser suite also 12/12
passes; diff whitespace check clean. These focused automated checks complement
runtime evidence but do not certify RF, Windows target hardware, complete
live-output equivalence or the unresolved intermittent FT4 timer stall.

### FT2 crowded direct-file Low CPU profile A/B

Added --low-cpu to the local exact-QML probe validator, checking the observed
start event's profile and retaining it in the report. Default remains false.
Both original and latest local app decode the identical crowded FT2 WAV with
Low CPU true, two requested threads, nominal depth 3. All eight full rows match
(message/mode/SNR/dt/frequency), no UDP warning, exit 0. Runtime QML and both
legacy test INIs restored. Evidence: ft2-busy-lowcpu-before/, after/ and
ft2-busy-lowcpu-comparison.json. This verifies equivalence for this fixture
within the intentional profile; it does not compare Low CPU sensitivity against
normal mode or reinterpret its known tradeoff. No production policy changed.

### Completion audit — not achieved

Read back current comparison, soak, two-app and package validation records,
confirmed local binary 43520240 and original HEAD 3529dae3 with uncommitted
work preserved. BootLoader rollback matches original bytes. Found only
report-export-check/report.zip, containing empty machine.json and an incomplete
run list: it is an export fixture, NOT returned Windows hardware evidence.
Focused CTest log confirms 15/15 pass. Structured requirement-by-requirement
state saved in completion-audit-2026-11-14.json.

Goal remains incomplete: general Windows speed/interaction benefit is not yet
measured, intermittent FT4 timer stall remains unexplained, full live FT2 output
varies within baseline, and short RSS soaks cannot establish hours-long memory
behaviour. Do not add identical local restarts merely to manufacture progress.
Highest-value next evidence is a returned focused Windows report and a natural
stall recurrence with named timer/phase logs, plus representative user settings
if fresh-profile startup differs from actual use. Hardware/RF boundaries remain
explicit; do not silently turn this into a completed local-only goal.
