# Native SSTV required final report

Evidence snapshot: 2026-08-24. This document follows the 20-item final-report
contract in the canonical native-SSTV mission. It is an evidence report for the
current feature worktree, **not a declaration that the Definition of Done is
complete**. The exact remaining gates are recorded in
[DEFINITION_OF_DONE.md](DEFINITION_OF_DONE.md).

Rebase/current-verification note: this branch was rebased onto `upstream/main`
`0bcd8b04a` (release v1.0.584). Subsequent current-tree local verification
built SSTV+HAMDRM aggregate targets, analog-only and SSTV-off configurations,
then passed the complete normal CTest set: 120/120 in 170.89 seconds (83
SSTV-labelled and 37 non-SSTV). An independently built ASan/UBSan tree covered
the same 120 tests in two invocations without an address/undefined finding.
The prior numerical sanitizer, performance and independent-vector evidence
remains historical where no rerun is listed; current local tests do not prove
external platforms, packages, radio, providers or independent interoperability.

## 1. Starting branch and starting commit

- Branch: main
- Tag: v1.0.583
- Commit: 119947690e2d8a1df99a75f98b915f2115df99e7
- State: fetched origin and upstream heads agreed with the clean local checkout
  before the feature branch was created.

The untouched baseline built on macOS Apple Silicon and passed 37/37 registered
CTest tests in 96.64 seconds. Its documented fixture skips and the unregistered
hardware RTL-SDR test are recorded in
[ARCHITECTURE_AUDIT.md](ARCHITECTURE_AUDIT.md).

## 2. Final branch and final commit

- Working branch: feature/native-sstv
- Rebased target base: `upstream/main` `0bcd8b04a` (release v1.0.584)
- Rebased native implementation commit: `f5dbce00d`
- Rebased build/packaging/CI commit: `68ac4014b`
- Rebased documentation-evidence commit: `fbac29f80`
- Local storage/UI hardening commit: `4d05a5b9f`
- Local package hardening commit: `79db9fc5a`
- Audio-source handoff fence: `678f9ca70`
- Incoming-media boundary fuzzing: `9cbdf1d2e`
- Sanitizer fixes/coverage: `5733cbcc0`, `2114f4d21`, `62275bdd8` and
  `efd1bc720`

The rebase retained the native implementation and build/CI changes at the
immutable SHAs above. The current local build/test evidence described here
includes these local hardening commits. No push, release, package upload or
remote workflow has occurred; the visible SSTV workspace remains **BETA**.

Committed inventory so far:

~~~text
04c81fede Document native SSTV architecture and delivery contract
a249ef544 Add native SSTV protocol core foundation
2428542d2 Add SSTV streaming audio and TX foundations
97667147e Add SSTV RX acquisition and progressive image core
f9b4dc08c Add SSTV RX frontend and streaming WAV
a08b3e918 Add optional SSTV hum and impulse filtering
f5dbce00d Complete native SSTV workspace and HAMDRM integration
68ac4014b Add native SSTV build packaging and CI coverage
fbac29f80 Document native SSTV operation evidence and release gates
4d05a5b9f Harden SSTV workspace storage and beta UI
79db9fc5a Harden native SSTV release packaging
678f9ca70 Fence SSTV audio source handoffs
9cbdf1d2e Fuzz incoming SSTV image and range boundaries
5733cbcc0 Fix MSK144 shorthand hash tail overflow
2114f4d21 Fix FST4 test diagnostic bounds
62275bdd8 Run Decodium regressions in native SSTV CI
efd1bc720 Fix Base32 TOTP shift overflow
~~~

## 3. Architecture summary

SSTV is compiled into Decodium4 and uses the application's existing services:

~~~text
existing local / RTL-SDR / TCI / DecoPort PCM fan-out
                    |
          bounded Bridge audio relay
                    |
       SstvAudioIngress -> SstvRxRuntime worker
                    |
  preprocess/demodulate/VIS/N-VIS/AVT/timing fallback
                    |
      family RX session -> progressive immutable image
                    |
          storage worker / Gallery / QSO

Studio prepared image -> native family pull encoder
                    |
      SstvTxCoordinator -> existing SoundOutput/CAT/PTT

Gallery file -> provider-neutral sharing queue/inbox -> HTTPS provider

shared PCM / TX authority -> separate HAMDRM controller/backends
~~~

There is no second QAudioSource, independent serial PTT stack, external SSTV
process, Java component or Python runtime. DSP, image codecs, SQLite, file I/O
and networking remain in C++ workers; QML consumes bounded properties, models
and image-provider snapshots.

Analog SSTV, remote IP sharing and HAMDRM are separate modules. A downloaded
network item cannot key the radio: explicit acceptance produces a revalidated
Gallery object, and a later local Studio/TX action must pass the ordinary
Decodium TX interlocks.

## 4. File-by-file summary of significant changes

| File or cohesive path | Significant change |
|---|---|
| CMakeLists.txt; src/sstv/CMakeLists.txt; src/sstv/digital/CMakeLists.txt; tests/sstv/CMakeLists.txt | Target-scoped SSTV/HAMDRM feature gates, libraries, application linkage, tests, fuzzers and developer tools without changing the global C++17 level. |
| src/bridge/DecodiumAudioSink.h; DecodiumBridge.{h,cpp}; DecodiumBridgeSstv.cpp | Existing-audio fan-out, Bridge-owned RX/TX/storage/gallery/sharing/HAMDRM services, settings, QML properties, shutdown/actions and immutable Gallery records for prepared/transmitted TX images. |
| Audio/soundout.{h,cpp} | Bounded pull-source support for long SSTV/HAMDRM streams under the existing output lifecycle. |
| RTL-SDR, DecoPort, transceiver and legacy-backend integration files | Source-labelled PCM forwarding to the same SSTV relay; no additional capture ownership. |
| src/sstv/core/* | Canonical registry/specification, standard/wide/narrow VIS, FSK ID and fractional timing. |
| src/sstv/dsp/*; src/sstv/rx/* | Bounded preprocessing, hum/impulse options, resampling, demodulation/AFC, sync/slant, leader/VIS/FSK/timing detection, replay and explicit RX state. |
| src/sstv/analog/* | Table-driven native RX/TX sessions for Martin, Scottie, Robot, Wraase/Pasokon, PD, AVT and MMSSTV wide/narrow. |
| src/sstv/tx/*; integration/SstvTx*; SstvWav* | Phase-continuous encoding, preparation, optional FSK ID, calibration, atomic WAV, loopback and fail-safe existing CAT/PTT/SoundOutput coordination. |
| integration/SstvRx*; SstvAudioIngress.* | Worker runtime, bounded ingress, correction controls, retained-audio jobs and replay/re-decode. |
| src/sstv/storage/*; GalleryModel; ThumbnailProvider | QStandardPaths layout, atomic images/sidecars, versioned SQLite, safe narrow notes/tags editing with rollback, retention/delete recovery, paging and lazy thumbnails. |
| src/sstv/sharing/*; SstvShareController | Versioned manifests, durable schema-v3 queue/inbox, REST/WebDAV/pre-signed providers, validation, TLS/credential policy and bounded sessions. |
| src/sstv/digital/* | Separate HAMDRM profile, MOT/BSR/object, persistence, channel/PHY/waveform, OpenJPEG and controller layers; TX now requires a bounded immutable Gallery snapshot before acceptance. |
| src/sstv/diagnostics/* | Allowlisted event ring, bounded scalar snapshots and atomic export; stable source tokens, at-most-4-Hz active-TX refresh plus terminal update, explicit unavailable HAMDRM state and persistent guarded test-tone result. |
| qml/decodium/components/sstv/*; qml/decodium/Main.qml | Lazy workspace with Receive, Studio, Gallery, Sharing, HAMDRM, Settings and Diagnostics pages; shared Material/palette contrast and the visible `SSTV - image radio... (BETA)` menu label. |
| translations/decodium_it.{ts,qm} | Integrated Italian strings through the existing workflow; validation reports 6,857 finished and 0 unfinished entries. |
| tests/sstv/* | 83 SSTV-labelled protocol, DSP, mode, integration, QML, storage, sharing, security, HAMDRM, performance and fuzz-smoke tests; the final normal CTest invocation passed 83/83 SSTV-labelled tests within 120/120 overall. |
| workflows, scripts and packaging/docker files | Platform/feature matrices and explicit Qt image-format, QSQLITE, ShaderTools and optional OpenJPEG packaging checks. Workflow definitions are not executed platform evidence. |
| docs/sstv/*; doc/THIRD_PARTY_LICENSES_OPENJPEG.md | Architecture, modes, provenance, RX/TX, storage, QSO, sharing/OpenAPI, HAMDRM, security, test, performance, user/developer and release evidence. |

## 5. Complete analog mode matrix

The authoritative complete row-by-row matrix is
[MODE_MATRIX.md](MODE_MATRIX.md). Its generated columns are checked against the
canonical C++ registry by test_sstv_mode_docs; copying its 64 rows here would
create a second manually maintained table.

| Family | Implemented modes | Count | External evidence boundary |
|---|---|---:|---|
| Martin | M1, M2, M3, M4 | 4 | M2 has PySSTV PCM; M2/M3/M4 have libsstv landmarks; no on-air result |
| Scottie | S1, S2, DX, S3, S4 | 5 | S3/S4 have libsstv landmarks; no compatible external decoder/RF run |
| Robot colour | C12, C24, C36, C72 | 4 | C36 has PySSTV PCM; conflicting upstream profiles remain explicit |
| Robot monochrome | B/W 8, 12, 24, 36 | 4 | B/W 8 has PySSTV PCM; other rows and aliases remain externally unverified |
| Wraase | SC2-60, SC2-120, SC2-180 | 3 | SC2-120/180 have PySSTV landmarks; SC2-60 has native evidence only |
| Pasokon | P3, P5, P7 | 3 | PySSTV timing landmarks; no cross-application/RF result |
| PD | PD50, 90, 120, 160, 180, 240, 290 | 7 | pySSTV/libsstv landmarks; libsstv's defective suffix is rejected |
| AVT normal | AVT24, AVT90, AVT94 | 3 | Handbook/source landmarks and native loopback only |
| MMSSTV extended/narrow | MP73/115/140/175; MR73/90/115/140/175; ML180/240/280/320; MP73N/110N/140N; MC110N/140N/180N | 19 | pinned source landmarks, no independent PCM |
| Related/catalogue-only | FAX480, HFFAX, WEFAX; AVT Narrow/QRM variants | 12 rows beyond the 52 above | blocked or unavailable, and not advertised as implemented SSTV |

All 52 native rows implement RX, TX and automatic protocol detection and have
deterministic coverage. “Implemented” does not mean independently interoperable.

## 6. Digital/HAMDRM compatibility matrix

The authoritative table is
[HAMDRM_COMPATIBILITY_MATRIX.md](HAMDRM_COMPATIBILITY_MATRIX.md).

| Capability | Local native status | Independent status |
|---|---|---|
| 72 named A/B/amateur-E profile tuples | registry and validation implemented | no independent waveform for all tuples |
| MOT header/body, CRC, segmentation/reassembly | implemented and tested | no QSSTV object exchange |
| BSR, missing ranges, retransmission/resume | implemented and tested locally | EasyPal expectation only |
| JPEG2000 | bounded OpenJPEG 2.5.4 lossless local round-trip | no QSSTV JP2 exchange/package proof |
| FAC/MSC pinned subset and OFDM waveform | implemented for documented subset | no independent RF/QSSTV waveform |
| Existing-audio RX and existing-coordinator TX adapters | connected and self-roundtrip tested | no sound-card/radio/on-air run |
| Full broadcast DRM, SDC, MSC Part A, VSPP, hierarchical/soft/CSI decode | not claimed or implemented | none |
| KG-STV | not implemented | public authoritative specification/vector gate remains |

No QSSTV, EasyPal or live HAMDRM interoperability result is claimed.

## 7. Upstream code and licence provenance

Exact revisions, paths, licences and conflicts are in
[UPSTREAM_PROVENANCE.md](UPSTREAM_PROVENANCE.md): QSSTV 8c27d6d (GPL
lineage, behaviour only), SlowRX a50a4e2/ca6d701 (ISC), Robot36 75146a5
(0BSD), libsstv 193157a (MIT), pySSTV d998fad (MIT), MMSSTV mirror 8060b5f
(LGPL/GPL-labelled source, behaviour only), QT6SSTV 6ae74b7 (migration audit)
and OpenJPEG 2.5.4 (BSD-2-Clause).

The SSTV Handbook PDF consulted has SHA-256
e244de9d5cbba525d33b25906c3751ab0ed62af2a3b373feffda44de4f13909d.
The imported/adapted component ledger remains None: no upstream implementation
source was copied or adapted. Restricted/ambiguous DRM and Numerical
Recipes-derived material is explicitly excluded.

## 8. Build commands used

Untouched baseline:

~~~zsh
cmake -S . -B build
cmake --build build --parallel "$(sysctl -n hw.ncpu)"
ctest --test-dir build --output-on-failure -j "$(sysctl -n hw.ncpu)"
~~~

Current local builds:

~~~zsh
cmake --build /tmp/decodium-hamdrm.csmxg7 \
  --target decodium_sstv_test_binaries decodium_regression_test_binaries \
  --parallel 6

cmake --build /tmp/decodium-sstv-analog.biDO0t \
  --target wsjtx decodium_qml decodium_sstv_test_binaries translations \
  --parallel 6

cmake --build /tmp/decodium-sstv-off.Hu9Bvh \
  --target wsjtx decodium_qml translations --parallel 6

cmake --build /tmp/decodium-sstv-asan.xmrVk5 \
  --target decodium_sstv_test_binaries decodium_regression_test_binaries \
  --parallel 6
~~~

The current caches confirm Release/Ninja/deployment target 13.0 and respectively
SSTV=ON,HAMDRM=ON; SSTV=ON,HAMDRM=OFF; and SSTV=OFF,HAMDRM=OFF. The enabled
cache resolves OpenJPEG through /opt/homebrew/lib/cmake/openjpeg-2.5. The
separate ASan/UBSan cache uses the same
`DECODIUM_SSTV_EXTERNAL_VECTOR_DIR=/tmp/decodium-sstv-external-vectors` as the
normal tree. All four listed target builds completed successfully. Build elapsed
values are not reported because the invocations rebuilt different cached target
sets.

## 9. Platforms built

| Platform/configuration | Actual result |
|---|---|
| macOS 26.5.2, Apple Silicon arm64, Qt 6.11, Release, SSTV+HAMDRM | current application/test build succeeded; `decodium_sstv_test_binaries` and `decodium_regression_test_binaries` were built and normal CTest passed 120/120 |
| Same host, analog-only | `/tmp/decodium-sstv-analog.biDO0t` built `wsjtx`, `decodium_qml`, SSTV test binaries and translations successfully |
| Same host, SSTV disabled | `/tmp/decodium-sstv-off.Hu9Bvh` built `wsjtx`, `decodium_qml` and translations successfully |
| macOS Intel | workflow updated; not executed |
| Windows x64 | workflow updated; not executed |
| Linux x86_64 | workflow/package scripts updated; not executed |
| Linux ARM64 | workflow/package scripts updated; not executed |

No final DMG/AppImage/Windows package has been inspected for the current
worktree. macOS compilation cannot establish those platform claims.

## 10. Tests executed

The final normal current-tree CTest invocation was:

~~~zsh
QT_QPA_PLATFORM=offscreen ctest --test-dir /tmp/decodium-hamdrm.csmxg7 \
  --parallel 4 --timeout 300 --output-on-failure
~~~

It passed 120/120 in 170.89 seconds: 83 SSTV-labelled and 37 non-SSTV tests.
The SSTV portion includes the Gallery metadata/archive, HAMDRM snapshot,
offscreen-QML and incoming-image staging-boundary coverage. The latter's
deterministic fuzz-smoke target drives 259 valid/hostile cases through the real
private staging path.

The separately configured ASan/UBSan tree built the same two test aggregates
with the same external-vector directory. Its final ordinary CTest coverage was
split only because `test_qt_helpers` requires a longer instrumented timeout:

~~~zsh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
QT_QPA_PLATFORM=offscreen \
ctest --test-dir /tmp/decodium-sstv-asan.xmrVk5 \
  -E '^test_qt_helpers$' --timeout 300 --output-on-failure

ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
QT_QPA_PLATFORM=offscreen \
ctest --test-dir /tmp/decodium-sstv-asan.xmrVk5 \
  -R '^test_qt_helpers$' --timeout 900 --output-on-failure
~~~

The first sanitizer invocation passed 119/119 in 152.20 seconds; the second
passed 1/1 in 338.63 seconds. Together they cover all 120 current tests without
an ASan or UBSan finding. macOS ASan explicitly reports leak detection
unsupported, so `detect_leaks=0` is required: this is address/undefined
sanitizer evidence, not LeakSanitizer evidence.

Historical pre-rebase supplemental evidence includes:

- untouched baseline: 37/37 CTest tests;
- focused mode-doc, external-vector, performance, sharing-core and
  schema-v3 queue run;
- family protocol/RX/TX/WAV/Studio tests for all 52 native analog rows;
- offscreen Receive, Studio, Gallery, Sharing, Settings, QSO, Diagnostics and
  Digital QML tests in focused runs;
- storage/migration/retention/delete, 5,000-row Gallery, sharing provider/
  session/queue/inbox and incoming-import tests;
- TX success/failure/cancellation/PTT-release tests, including cancellation
  during header, image and FSK ID;
- HAMDRM object, BSR, partial store, OpenJPEG, channel, PHY, waveform, adapter
  and controller tests;
- deterministic parser fuzz smoke and focused ASan+UBSan repetitions.

The current enabled build registers 83 SSTV-labelled tests and 120 tests
overall. The full normal CTest invocation passed 120/120 in 170.89 seconds.
The full ordinary sanitizer coverage passed in its two documented invocations,
119/119 plus 1/1. This establishes executable current-tree normal and
address/undefined-sanitized coverage without claiming coverage-guided fuzzing
or a leak-sanitizer result.

## 11. Test results

Current results:

- SSTV+HAMDRM, analog-only and SSTV-off builds: success;
- normal full suite: 120/120 passed in 170.89 seconds (83 SSTV-labelled, 37
  non-SSTV);
- ASan/UBSan full ordinary suite: 119/119 passed in 152.20 seconds plus
  `test_qt_helpers` 1/1 in 338.63 seconds; `detect_leaks=0` on macOS because
  that ASan runtime does not support leak detection;
- real incoming-image staging fuzz smoke: 259 deterministic cases passed;
- native-SSTV CI definition now builds both SSTV and non-SSTV regression
  aggregates, but no GitHub workflow was run;
- package-script syntax, Windows workflow YAML, Italian TS XML, the SSTV QML
  pages and repository-layout validation: passed locally.

`qmllint` exited successfully for the target pages and `Main.qml`; the latter
still emits unrelated legacy layout/unqualified-access warnings, so it is not
represented as a warning-free application audit.

Historical focused evidence remains useful but is not relabelled as current:
the 5/5 mode-doc/external-vector/performance/sharing-core/schema-v3 run passed
in 6.16 seconds, and the schema-v3 queue tests cover migration/restart/rollback,
more than 10,000 closed inbox cycles, oldest-first terminal reclamation and
protection of active/retryable/file-owning rows.

## 12. Independent interoperability vectors used

The pinned PySSTV d998fad pack is independent of the Decodium encoder:

| Mode | WAV SHA-256 | Native replay result |
|---|---|---|
| Robot 36 | 6d5164a9294cbc597a7ef6494efea15a02d5a6267662e5ff98023acbed4bf0cb | complete 320x240, coverage 1.0, 18.024 dB PSNR |
| Robot B/W 8 | 660d52ca4427d4d3271281285336bc3feb86559615b005066667c4dc233ecaf0 | complete 160x120, coverage 1.0, 18.661 dB PSNR |
| Martin M2 | 4cad290aec3ee249541bcd56c85717263e3d03af18755d806d5e4418085152d5 | complete 320x256, coverage 1.0, 24.250 dB PSNR |

All reported zero ingress drops and zero processing failures. Compact
libsstv/pySSTV timing/hash landmarks and MMSSTV/AVT source-document fixtures are
developer oracles only where the matrix says so. They are not full independent
PCM, another decoder, live RF or on-air evidence. No independent receiver has
decoded Decodium TX output.

Robot B/W 8 now keeps canonical TX at a 10 ms sync plus 56 ms scan (66 ms) and
explicitly recognises the independent PySSTV compatibility waveform at 7 ms
plus 60 ms (67 ms). The 66/67 ms decoder selection passed 40 repetitions and
ASan; the pinned Robot B/W 8 WAV remained green alongside Robot 36 and Martin
M2. This strengthens that one RX interoperability vector without proving an
external decoder or RF path.

## 13. Performance measurements

Executed locally:

~~~zsh
/tmp/decodium-hamdrm.csmxg7/tests/sstv/sstv_performance
~~~

~~~text
audio_seconds:             15
dsp_wall_seconds:          0.040
dsp_realtime_ratio:        377.408
frequency_observations:    179968
inactive_wall_ms:          755.027
inactive_cpu_ms:           0.015
inactive_worker_running:   false
inactive_chunks_processed: 0
pass:                      true
~~~

This is one Release run on macOS Apple Silicon. It does not establish
cross-platform latency, long-duration stability, waterfall frame-rate
non-regression or real device callback behavior.

## 14. Security controls implemented

- strict image, WAV, JSON, manifest, path, response and HAMDRM object bounds;
- literal, nested and escaped-equivalent duplicate JSON key rejection;
- checked arithmetic and MIME/magic/hash/dimension/pixel/allocation/frame gates;
- private quarantine, metadata-free PNG normalization, QSaveFile, transactional
  SQLite and revalidation before Gallery import;
- HTTPS-only production policy, certificate validation, bounded redirects, no
  cross-origin credential forwarding and no runtime plaintext switch;
- direct fail-closed SecureSettings backend use, opaque leases and no secret
  values in QML models, SQLite or diagnostic export;
- bounded HTTP operations, sessions, queue rows, retries and response budgets;
- central diagnostic allowlists, a 512-event ring and 1 MiB scalar-only export;
- network-to-RF separation and existing TX ownership/watchdog/release.

E2EE is not implemented. TLS-only provider endpoints can read content, and the
UI/documentation says so.

## 15. Remote-sharing providers implemented

| Provider | Implemented capability | Important limit |
|---|---|---|
| Generic HTTPS REST v1 | capabilities, recipient lookup, upload/resume/complete/status, authenticated inbox/download/ack/reject/delete/block when advertised | requires a compatible deployed service; none is bundled |
| WebDAV over HTTPS | directory validation, upload/status/delete and bounded direct GET | no standard recipient/inbox/ack contract |
| Trusted pre-signed PUT | short-lived broker lease and bounded PUT without cloud SDK | no trusted broker is shipped |
| Process-local integration provider | deterministic upload/download/inbox/idempotency tests | developer/test only; no socket or production UI selection |
| Decodium peer/relay | protocol boundary documented | no existing channel met the secure inbox/object contract; no relay invented |

All sharing is opt-in. Automatic upload, public sharing and automatic content
download default off.

## 16. Required external backend/deployment steps

An operator or deployer must:

1. deploy/select a real HTTPS REST v1 or HTTPS WebDAV service;
2. for REST, implement the capability, identity, upload, inbox, download,
   acknowledgement/rejection, expiry and idempotency OpenAPI contract;
3. provision CA-valid TLS and stable recipient identities;
4. create credentials through Decodium's secure backend;
5. configure size, expiry, retry and provider limits;
6. for pre-signed PUT, deploy an authenticated broker issuing short-lived
   leases without exposing URLs to QML/settings/queues/logs;
7. test upload/download/restart/revocation/deletion against that deployment;
8. disclose that TLS-only service operators can read objects.

No unauthenticated public relay or fictional Decodium cloud endpoint is
included.

## 17. Known limitations with precise technical reasons

- Final Windows, Linux x86_64/ARM64 and macOS Intel build/package evidence is
  absent; workflow edits are configuration only.
- The feature was rebased onto v1.0.584. Its rebased implementation, build/CI
  and documentation-evidence SHAs are recorded above; normal and
  address/undefined-sanitized local test evidence has now been repeated on the
  rebased current tree, but maintained-platform verification remains absent.
- Only Martin M2, Robot 36 and Robot B/W 8 have independent full PCM
  encoder-to-native-decoder vectors. Most rows have native loopback or
  timing/source landmarks only.
- No Decodium TX waveform has been decoded by another application and no image
  has been exchanged over RF.
- No real sound card, RTL-SDR/TCI/DecoPort session, CAT interface or PTT line
  has been exercised for SSTV.
- HAMDRM has no independent QSSTV/EasyPal exchange and is not full broadcast DRM.
- No production sharing backend, real provider account or complete
  maintained-platform secure-store test exists.
- E2EE lacks an audited dependency, envelope/key lifecycle, packaging and vectors.
- FAX480 has unresolved 512x500 versus 512x480 geometry; HFFAX/WEFAX and AVT
  Narrow/QRM lack complete defensible semantics.
- Final packages have not been inspected for QSQLITE, Qt image plugins,
  ShaderTools/QML assets and OpenJPEG closure.
- Coverage-guided libFuzzer runs, longer sanitizer concurrency/shutdown stress,
  forced process-kill recovery and cross-platform performance remain open.
  Apple Clang lacks the libFuzzer runtime locally, and the configured Linux
  fuzz job has not been executed.

## 18. Manual radio tests still recommended

Before release, record:

1. RX from named radio/audio hardware for common, extended and long modes;
2. real DecoPort/RTL-SDR/TCI routing where advertised;
3. TX into a dummy load/monitor receiver, verifying PTT lead, level, tail and
   release;
4. cancellation during header/image/FSK ID plus CAT disconnect, device loss,
   sleep/resume and shutdown;
5. bidirectional analog decoding with QSSTV and another legal implementation;
6. HAMDRM object/corruption/BSR/resume exchange with QSSTV;
7. long Scottie DX/PD/ML runs for clock drift, underrun and watchdog behavior.

Each record should name OS, commit, radio/interface, audio device, sample rate,
mode, frequency, counterpart/version and observed result. Mock PTT cannot
replace this evidence.

## 19. Completed UI description and screenshot status

Access is through the top-left hamburger menu: **SSTV - image radio... (BETA)**.
The lazy workspace is titled **SSTV - Decodium** and contains:

- Receive: progressive image and VIS/mode/sync/level/AFC/slant/FSK controls;
- Transmit Studio: source/prepared/loopback views, edits, overlays/templates,
  mode/FSK, WAV, loopback, calibration and TX/cancel;
- Gallery: lazy thumbnails, filters/search, metadata and local actions;
- Remote Sharing: provider/recipient/privacy, queue, inbox and transfer actions;
- Digital HAMDRM: named profiles, object/missing/BSR/resume and RX/TX;
- Settings and Diagnostics with bounded scalar export, stable source tokens,
  explicit HAMDRM availability and a persistent test-tone result. Active TX
  refresh is capped at 4 Hz plus terminal state; the tone control warns that it
  keys PTT/transmits RF and repeats the normal TX safety guard.

Offscreen QML tests exercised 1040x700 and page-specific layouts. A local
offscreen Studio rendering was human-reviewed after the shared Material/palette
contrast correction; no final human-reviewed screenshot from a packaged build
is recorded, so this does not imply packaged visual evidence.

## 20. Suggested pull-request title and body

Suggested title while external gates remain:

~~~text
Draft: add native analog SSTV, secure sharing and separate HAMDRM support
~~~

This is documentation-only draft text. It does **not** authorize creating a
pull request, pushing the branch, uploading packages or publishing a release.

Suggested body:

~~~markdown
## Summary

Adds an in-process Decodium4 SSTV workspace with native analog RX/TX for the
52 implemented rows in docs/sstv/MODE_MATRIX.md, progressive reception,
Studio/WAV/loopback, existing SoundOutput/CAT/PTT coordination, Gallery/QSO
storage, opt-in provider-neutral sharing, diagnostics and separately gated
HAMDRM.

No second audio capture device or external SSTV/Python/Java runtime is used.

## Evidence

- untouched baseline: 37/37 CTest tests;
- macOS Apple Silicon builds: SSTV+HAMDRM, analog-only and SSTV-off;
- current rebased tree: normal full CTest 120/120 in 170.89 seconds (83
  SSTV-labelled and 37 non-SSTV);
- current rebased ASan/UBSan tree: 119/119 plus `test_qt_helpers` 1/1, with
  macOS leak detection explicitly unavailable; no address/undefined finding;
- real incoming-image staging fuzz smoke: 259 deterministic cases;
- three pinned PySSTV WAVs decoded through production replay/runtime;
- Robot B/W 8 canonical 66 ms and compatibility 67 ms paths passed 40 repeats
  plus ASan;
- Italian translations: 6,857 finished, 0 unfinished;
- focused sanitizer, QML, storage, sharing, TX and HAMDRM coverage.

See docs/sstv/FINAL_REPORT.md and DEFINITION_OF_DONE.md for exact limits.

## Required before merge/release

- record the immutable final SHA and commit inventory;
- execute/inspect maintained Windows, macOS and Linux CI/packages;
- run real radio/audio/CAT/PTT and cross-application trials;
- audit a production sharing provider and secure-store behavior;
- execute coverage-guided libFuzzer, longer sanitizer stress and package
  inspection.

Do not treat loopback, source landmarks or workflow YAML as on-air,
interoperability or platform-package evidence.
~~~
