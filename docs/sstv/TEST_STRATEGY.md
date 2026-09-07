# Native SSTV test strategy

Status: Milestone 0 contract with current local-verification addendum,
2026-08-24.

## Baseline evidence

The unmodified source at
`119947690e2d8a1df99a75f98b915f2115df99e7` was configured and built on macOS
ARM64 with:

```zsh
cmake -S . -B build
cmake --build build --parallel "$(sysctl -n hw.ncpu)"
ctest --test-dir build --output-on-failure -j "$(sysctl -n hw.ncpu)"
```

Configuration and the 734-step build completed successfully. CTest passed
37/37 tests in 96.64 seconds. This is a regression baseline, not SSTV evidence:
there were no SSTV tests or sources. `test_qt_helpers` reported 154 subtests
passed and 12 skipped because several FST4/MSK144/FT8/FT2 fixtures or comparators
were unavailable. The accepted-limit FT8 case also permits a missed decode via
`DECODIUM_WEAK_TEST_ACCEPT_MISS=1`. Hardware RTL-SDR input is built but not
registered in CTest without explicit hardware opt-in.

The build emitted existing CMake policy/Qt private-header warnings and macOS
deployment-target warnings for newer Homebrew libraries. Ninja recovered a
premature dependency-file EOF before rebuilding; no concurrent build may use
the same build directory.

## Current final local verification snapshot

On the current local tree through `efd1bc720`, macOS Apple Silicon Release
builds succeeded for SSTV+HAMDRM, analog-only and SSTV-off configurations. The
full SSTV+HAMDRM build explicitly built both `decodium_sstv_test_binaries` and
`decodium_regression_test_binaries`, then normal CTest passed 120/120 in 170.89
seconds (83 SSTV-labelled and 37 non-SSTV tests).

The same two aggregates also built in an ASan/UBSan configuration with the same
`DECODIUM_SSTV_EXTERNAL_VECTOR_DIR`. Its ordinary CTest coverage passed 119/119
excluding `test_qt_helpers` in 152.20 seconds and `test_qt_helpers` 1/1 in
338.63 seconds under a 900-second timeout. That covers all 120 current tests
without an address/undefined finding. macOS ASan does not support leak
detection, so this run used `ASAN_OPTIONS=detect_leaks=0`: it is not a
LeakSanitizer result.

This is local build/test evidence only. Apple Clang has no libFuzzer runtime in
this environment, and neither the configured Linux fuzz job nor any GitHub
workflow, package, hardware/radio, provider or independent-interoperability
test was executed.

## Evidence classes

Every result is labelled as one of:

1. **unit**: exact pure-code behaviour;
2. **self-generated**: Decodium encoder and decoder through a channel model;
3. **independent synthetic**: another implementation generated the waveform;
4. **independent real recording**: received audio with source/permission;
5. **integration-local**: Qt threads/files/SQLite/local HTTP/mock PTT;
6. **rendered UI**: actual QML application interaction and screenshot;
7. **hardware/radio**: named devices/radio and observed PTT/audio result;
8. **platform/package**: actual OS build and installed/bundled runtime check.

No stronger claim may be inferred from a weaker evidence class. In particular,
self-round-trip does not prove interoperability, a mock PTT test does not prove
a radio transmission, and a macOS build does not prove Windows/Linux support.

## Test target layout

SSTV tests live below `tests/sstv` and are registered in CTest. Pure protocol
and DSP tests link `decodium_sstv_core`; Qt integration tests link only the
smallest relevant integration target. Every SSTV target explicitly requests
C++17 because `tests/CMakeLists.txt` currently defaults to C++11.

The native foundation runs on 2026-08-24 added and passed twelve labelled CTest
targets:

```text
ctest --test-dir build -L sstv --output-on-failure
12/12 passed: mode registry, timing accumulator, VIS codec, FSK ID codec,
resampler, audio/replay buffers, tone generator/TX pull stream, RX state
machine, tone detector, progressive image/colour core, RX preprocessing/
frequency demodulation/signal metrics and streaming PCM16 WAV output
```

The second tranche also passed 33 resampler/buffer QtTest cases and 10 tone/TX
stream cases in standalone sanitizer runs (ASan/UBSan, plus TSan for the audio
buffer). The RX state machine/tone detector add 34 QtTest cases, while the
progressive image/colour core adds 14; their standalone ASan/UBSan runs and
strict-warning builds also passed. The RX frontend adds 19 QtTest cases and the
WAV stream writer adds 11; both also passed standalone strict-warning and
ASan/UBSan runs. The in-tree CTest execution passed every
registered SSTV executable. These
remain protocol/DSP unit results: they prove bounded chunk-independent sample
rate conversion, queue/replay policy, fractional duration scheduling and
phase-continuous pull generation, deterministic acquisition state transitions,
discrete-tone classification, bounded progressive frame assembly, conditioned
12 kHz acquisition, bounded frequency demodulation/metrics and a seekable
streaming RIFF writer. They do
not prove a complete analog mode encoder/decoder, live sound hardware, CAT/PTT
sequencing or independent application interoperability.

Planned groups:

- `test_sstv_mode_registry`: IDs, names, dimensions, VIS uniqueness/conflicts,
  rational timing, component ordering, capability/evidence invariants;
- `test_sstv_timing`: fractional sample accumulation for all supported rates,
  bounded accumulated error and encoder total duration;
- `test_sstv_vis`: standard/wide-extended VIS and four-group narrow N-VIS bit
  order, parity/checksum, framing, drift, offset, truncation, noise, false
  positives and repeated headers;
- `test_sstv_fskid`: allowed alphabet, sanitisation, raw symbols, malformed
  input, confidence and deterministic RX/TX framing;
- `test_sstv_colour`: fixed reference pixels for every distinct colour system
  and range, including Robot/PD alternating chroma rules;
- `test_sstv_encoder`: header/VIS/segments/line count/level/phase continuity for
  every TX-capable mode;
- `test_sstv_decoder`: legally redistributable independent fixtures;
- `test_sstv_roundtrip`: waveform passed through an independent channel model;
- `test_sstv_impairments`: acquisition, degradation and graceful partial exit;
- `test_sstv_wav`: RIFF boundaries, streaming writes and hostile imports;
- `test_sstv_storage`: paths, atomic files, schema/sidecar favourite migration
  and restart, separate quota buckets, retention ordering/protections,
  confirmation, auto opt-in and deletion-journal crash recovery;
- `test_sstv_sharing`: deterministic local server and persistent queues;
- `test_sstv_security`: resource ceilings, redirects, JSON/path validation and
  secret/log redaction;
- `test_sstv_tx_integration`: exclusive TX ownership and PTT release invariants;
- `test_hamdrm`: profiles, objects, CRC, BSR, retransmission and malformed data.

The operator-facing Studio/TX path additionally uses
`test_sstv_image_preprocessor` for every preparation control/overlay and the
bounded local codec boundary; `test_sstv_studio_controller` for immutable
snapshots, template persistence/migration, WAV lifecycle and real PCM into a
real `SstvRxRuntime`; `test_sstv_tx_audio_device` for the four calibration
frequencies and peak/clipping counters; `test_sstv_tx_coordinator` for prepared
calibration PTT/cancel lifecycle; and `test_sstv_studio_qml` for instantiated
controls and offscreen rendering. These are unit, integration-local,
self-generated and rendered-UI evidence, not live radio or cross-application
evidence. See [STUDIO_TX.md](STUDIO_TX.md).

The preprocessing, coordinator and Studio tests also assert low-rate
structured-event coverage for validation rejection, TX/calibration lifecycle
and loopback lifecycle. They verify exact `sstv.security`/`sstv.tx` categories,
allowlisted scalar fields, bounded fixed reason codes, and absence of injected
paths or free-form failure details.

## Registry and protocol correctness

For every catalogued mode, data-driven rows verify:

- stable unique ID and non-empty family/name;
- analog/digital/related-FAX classification;
- dimensions, line/display counts and scans-per-line;
- standard/extended VIS encoding, with documented duplicate/conflict handling;
- component order, colour system and subsampling;
- positive segment durations whose sum matches line and image duration within
  an explicit independent-reference tolerance;
- RX/TX/auto-detect flags that cannot become `verified` without named evidence.

VIS testing samples exact nominal events first, then randomized block boundaries,
frequency offsets, timing drift, bad parity, bad start/stop bits, damaged break,
back-to-back headers and noise-only false-positive runs. FSK tests preserve raw
symbols and reject or replace invalid characters deterministically.

## AVT normal-family evidence

Normal AVT24/90/94 use one clean-room implementation and the compact
documentary/source-landmark fixture
`tests/sstv/fixtures/avt-handbook-qsstv-landmarks.json`. The fixture pins the
SSTV Handbook PDF at SHA-256
`e244de9d5cbba525d33b25906c3751ab0ed62af2a3b373feffda44de4f13909d`,
QSSTV at `8c27d6d169d8c6c197eb47c2089870e39bc06a02` and the MMSSTV mirror at
`8060b5f1e9727b0052d74108081c6db7b26babad`. It contains no audio or copied
implementation.

Deterministic coverage is split across:

- `test_sstv_avt_sync`: protected 17-symbol words, exact inverses, every mode
  prefix/counter and all standard variant VIS identities;
- `test_sstv_avt`: 128/320 prepared geometry, AVT90's separate 256 effective
  width, exact triple VIS plus 32-frame countdown, cumulative no-line-sync RGB
  mapper, bounded/chunk-invariant encoder/decoder, real one-sample-hop
  frequency-demodulator acquisition, counter timing and full AVT24 PCM
  loopback within both detector and decoder consume bounds;
- `test_sstv_avt_rx_session`: progressive publication, completion,
  cancellation, hostile bounds and fail-closed discontinuity without invented
  phase;
- `test_sstv_mode_registry`: implemented claims only for normal VIS 64/68/72,
  explicit sync-free timing fields, 320 prepared versus 256 effective AVT90,
  and catalogue-only/unimplemented Narrow/QRM/Narrow-QRM VIS 65-75;
- `test_sstv_rx_runtime`: the actual in-process preprocessor/demodulator,
  standard-VIS detector, protected-countdown detector and progressive session
  select every normal mode; a full AVT24 128x120 frame completes from native
  PCM, and reset permits a later non-AVT VIS without stale suppression;
- `test_sstv_tx_coordinator`, `test_sstv_wav_exporter` and
  `test_sstv_studio_controller`: native shared audio/PTT source selection,
  exact 8.0425 s header/total frame counts, RIFF export and visible prepared
  geometry without any external process.

The AVT90 negative test requires prefix `101` and rejects copying the pinned
MMSSTV transmitter's conflicting `010` prefix. Narrow, QRM and Narrow-QRM
headers remain catalogue identities only: no test or document promotes them
to executable picture modes without complete semantics and independent
evidence.

All AVT PCM used end-to-end is generated by Decodium itself. There has been no
independently captured AVT waveform, live audio device, radio/PTT, on-air
contact, packaged-platform execution or MMSSTV/QSSTV cross-application decode.

## MMSSTV extended-family evidence

The required extended family is represented by one native implementation and
one canonical source-landmark fixture:
`tests/sstv/fixtures/mmsstv-8060b5-extended-mode-landmarks.json`. The fixture
pins the MMSSTV source mirror at
`8060b5f1e9727b0052d74108081c6db7b26babad` and QSSTV at
`8c27d6d169d8c6c197eb47c2089870e39bc06a02`. It records exact dimensions,
scan counts, layouts, timing/frequency boundaries, raw wide extension octets
and narrow payloads for all nineteen modes; it does not contain audio.

The deterministic coverage is split across:

- `test_sstv_mmsstv_extended`: all mode specifications, cumulative sample
  mapping, fractional boundaries, MP vertical chroma, MR/ML horizontal chroma
  and terminal holds, MC sequential RGB, streaming TX chunk invariance,
  progressive bounded decode and hostile-input limits;
- `test_sstv_mmsstv_vis`, `test_sstv_narrow_vis` and
  `test_sstv_narrow_vis_detector`: exact wide extended-VIS/N-VIS framing,
  parity/checksum, fragmentation, invalid candidates and false positives;
- `test_sstv_mode_registry`: unique lookup and implemented RX/TX/autodetect
  claims for all nineteen stable IDs, including the fixture-backed MP73-Narrow
  payload `0x02`;
- `test_sstv_rx_runtime`: encoder-to-PCM-to-detector-to-native-session
  acquisition and progressive image publication for every wide and narrow
  mode, plus coexistence with standard VIS;
- `test_sstv_tx_coordinator`, `test_sstv_wav_exporter`,
  `test_sstv_studio_controller` and `test_sstv_studio_qml`: native streaming
  source selection, exact phase/frame boundaries, RIFF length and visible mode
  selection without launching an external process.

MR175 explicitly accepts raw extension `0x4C`, as transmitted/listed by the
pinned original MMSSTV source and Handbook, and rejects QSSTV's duplicate
MR140 `0x4A` typo. MC110-Narrow follows the executable 140 ms component and
428.5 ms structural scan rather than the conflicting 143 ms prose. These are
documented implementation resolutions, not interoperability proof.

All end-to-end waveforms in this suite are produced by Decodium itself. The
pinned source fixture is a clean-room protocol-landmark oracle, not independent
PCM. No externally captured MMSSTV recording, live audio device, radio/PTT,
on-air contact, packaged-platform run or independent application decode has
been exercised; those remain separate release evidence.

Current MMSSTV gates on macOS ARM64 (2026-08-24):

- direct syntax builds of the new core, session, N-VIS codec/detector and the
  RX runtime/TX source/coordinator/Studio integration pass with
  `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`;
- the five protocol/registry targets pass 25 consecutive CTest executions each
  (125 runs, 101.17 s); TX coordinator, WAV exporter, Studio controller and
  offscreen Studio QML pass five consecutive executions each (20 runs,
  56.15 s); the complete multi-family RX runtime passes twice consecutively
  (156.23 s);
- the nine targeted protocol/registry/runtime/TX/WAV/Studio targets pass
  9/9 under combined ASan+UBSan with halt-on-error enabled (284.51 s).

The linker emitted the existing deployment-target warning because the local
Homebrew Qt frameworks target a newer macOS release than the configured 13.0
minimum. No compiler, test, ASan or UBSan failure was reported.

## Encoder proof

For every TX row, encode a fixed colour bars/grid/edge test card at all required
sample rates. Measure event boundaries from PCM, not from private encoder state.
Assertions cover:

- leader/break/VIS/wide-extended-VIS/N-VIS tones and durations;
- phase continuity across every segment and pixel;
- exact transmitted lines and component sequence;
- porch, separator, sync and pixel frequency bounds;
- average fractional timing error and complete waveform duration;
- default headroom, peak and absence of integer clipping;
- optional FSK ID placement and duration;
- streaming cancellation and valid atomic WAV output.

At least one external oracle must agree before the mode can be marked TX
interoperable. pySSTV/libsstv are suitable only for their implemented families;
QSSTV-derived expected values require licence/provenance review and do not count
as a second independent implementation by themselves.

## Decoder and impairment proof

Independent fixtures are decoded with randomized input block sizes. The clean
gate requires correct mode, dimensions, ordering, completion and a documented
image metric. Analog impairment sweeps combine:

- frequency offset and slow drift;
- ±300 ppm and wider exploratory sample-clock error;
- AWGN at recorded SNR values;
- gain changes, clipping and DC offset;
- 50/60 Hz hum and narrow interferers;
- impulses, short drop-outs, missing/false sync and echo;
- resampling and stereo imbalance;
- truncated header/start/end and back-to-back images.

Initial common-mode acquisition target is at least ±100 Hz. Severe cases need
not produce a complete image, but must terminate within a mode-specific bound,
preserve valid lines as partial, stay within memory limits and return to leader
search. Noise-only corpora measure false acquisitions explicitly.

PSNR/SSIM and per-channel error are used only with documented thresholds. Exact
pixels are required for pure colour-conversion vectors, not realistic analog
channels.

## Threading and performance proof

Tests randomize producer block sizes and cancellation times while measuring:

- bounded input/replay queue depth and reported drop count;
- bounded database queue current/peak depth, rejection and shutdown
  cancellation, plus last/average/maximum image-save time;
- no image/file/SQL/network operation from the audio callback;
- worker affinity and no QObject cross-thread warnings;
- GUI update throttle and dirty-rectangle size;
- average/max DSP block time below the available real-time budget;
- clean stop on workspace exit, mode change and application shutdown;
- negligible inactive-path callbacks/CPU/allocation;
- no regression in an existing panadapter cadence benchmark.

Performance reports record machine, build type, sample rate, mode, corpus and
commit. One fast run is not a cross-platform performance claim.

Storage counter tests use exact injected nanosecond values for arithmetic,
multi-threaded updates for synchronization and a stalled/queued worker path for
lifecycle. Real PNG timing assertions check attempt/success/failure accounting
and invariants rather than a machine-specific speed threshold. The snapshot is
also constrained to scalar values so paths and record metadata cannot enter a
diagnostic export. See [PERFORMANCE_COUNTERS.md](PERFORMANCE_COUNTERS.md).

## Storage, parser and network security proof

Hostile tests impose hard compressed bytes, dimensions, pixel/memory, WAV
duration/chunk, JSON depth/field and response-byte ceilings. Cases cover integer
overflow, truncated RIFF, decompression bombs, invalid MIME, traversal,
absolute/reserved names, Unicode normalization, symlink escape and interrupted
atomic writes.

Gallery export tests use a real indexed PNG/sidecar, require worker-thread
verification and an exact streamed byte copy, inspect owner-only permissions,
and prove that an existing destination, the indexed source path and a non-local
URL are refused without modifying either file. The offscreen Gallery render
also instantiates the per-record action menu, the destructive-delete control
and its separate confirmation popup. A focused storage/model test proves that
physical deletion removes the verified PNG, sidecar, thumbnail and exclusive
retained WAV only after the row transaction, leaves index-only files untouched,
and leaves no staging directory on the success path. A restart test constructs
the exact bounded journal and proves both pre-commit restoration and post-commit
staged-file cleanup. Shared-reference/fault-injection coverage, a real forced
process kill and full user-dialog automation on each maintained desktop remain
separate evidence.

The local network server verifies HTTPS policy separately from localhost test
exceptions and covers origin-changing redirects, credential forwarding,
timeouts, rate limits, malformed JSON, duplicate completion, wrong hashes,
expiry, cancellation, restart/resume, permanent-auth failures, malformed and
mismatched `Content-Range` responses, and log redaction.
The incoming-media fuzz target drives the real private staging boundary with
valid PNG/JPEG seeds and hostile bytes, rechecking hash/MIME, decode allocation,
normalisation and restart inspection. Its deterministic ordinary-test smoke
executes 259 cases through that boundary. Fuzz targets retain crashing inputs
and run with ASan/UBSan where supported; this smoke test is not a substitute
for coverage-guided libFuzzer execution.

## TX safety proof

An instrumented fake existing Decodium coordinator—not a second SSTV PTT
implementation—exercises PTT confirmation, timeout, audio failure, CAT/radio
disconnect, underrun, sleep/shutdown simulation and cancellation during header,
image and FSK ID. Every path asserts:

- no PCM before policy approval;
- no overlap with weak-signal TX;
- bounded watchdog lifetime;
- exactly one ownership release and final PTT-off request;
- restoration of the previous RX state.

Real-device tests separately record OS, radio, interface, serial settings,
audio device, observed lead/tail and measured output. They are recommended
before release and never replaced by the fake coordinator.

## Continuous integration and packaging

Update workflow path filters for `src/sstv/**`, `tests/sstv/**`,
`qml/decodium/sstv/**`, the integration files and this documentation. Required
jobs are:

- pure core/codec tests on maintained Windows, macOS and Linux runners;
- full Decodium build with analog SSTV enabled;
- analog-only and analog+HAMDRM configurations when dependencies are present;
- QML lint plus a headless startup smoke test;
- sanitizer/fuzz job on a suitable Linux runner;
- package inspection for QML files, Qt image plugins and optional OpenJPEG;
- separate opt-in jobs for large pinned fixture packs and hardware.

The native-SSTV workflow definition builds both `decodium_sstv_test_binaries`
and `decodium_regression_test_binaries`, then runs the SSTV-labelled and
non-SSTV CTest partitions so an SSTV-only test build cannot mask ordinary
regressions. That definition, including its Linux libFuzzer job, has not been
executed for this snapshot.

The final report lists the exact jobs, artifacts, test totals, skips and platform
limitations. A green packaging workflow with `BUILD_TESTING=OFF` is build/package
evidence only.
