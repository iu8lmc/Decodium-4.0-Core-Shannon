# Native SSTV Definition of Done ledger

Evidence snapshot: 2026-08-24, branch `feature/native-sstv`. This ledger mirrors
the 33-item Definition of Done in the canonical mission. It distinguishes an
executed local result from a maintained-platform build, a release artefact, a
real radio, a production provider and another SSTV implementation.

Status values:

- `pass`: the complete wording of the requirement is proved by current evidence;
- `local-pass`: useful executable evidence exists, but a required platform,
  hardware, provider or independent-interoperability gate is still open;
- `in-progress`: implementation exists, but the final current-tree verification
  is not yet complete;
- `pending`: the required evidence does not yet exist.

Current local evidence is macOS 26.5.2/Apple Silicon, Release, Qt 6.11 and
OpenJPEG 2.5.4. On the final locally verified tree, the SSTV+HAMDRM aggregate
targets `decodium_sstv_test_binaries` and `decodium_regression_test_binaries`
built successfully; analog-only and SSTV-off configurations also built
successfully. A full normal CTest invocation passed 120/120 tests in 170.89
seconds: 83 SSTV-labelled and 37 non-SSTV. This is one actual aggregate result,
not an inferred sum of partitioned runs.

The same two aggregates also built in a separate AddressSanitizer/Undefined-
BehaviorSanitizer configuration using the same external vector directory. Its
CTest set passed in two deliberate invocations: 119/119 excluding
`test_qt_helpers` in 152.20 seconds and `test_qt_helpers` itself 1/1 in 338.63
seconds with a 900-second timeout. Together they cover all 120 current tests
without an ASan/UBSan finding. macOS ASan reports leak detection unsupported,
so that local run used `ASAN_OPTIONS=detect_leaks=0`; it is address/undefined
sanitizer evidence, not leak-sanitizer evidence. Earlier pre-rebase
timing/build outputs remain historical context only.

A focused run of mode-document synchronisation, the three pinned PySSTV WAV
vectors, performance, sharing-core security and the schema-v3 queue additionally
passed 5/5 in 6.16 seconds. The post-fix performance executable processed 15
seconds of audio in 0.040 seconds (377.408 times real time) and measured the
inactive path at 0.015 ms CPU during 755.027 ms wall time, with no worker and no
processed chunks. These numbers are one local run, not a cross-platform
benchmark.

| # | Requirement | Status | Current evidence and remaining gate |
|---:|---|---|---|
| 1 | Build on every maintained desktop platform | pending | On the final locally verified macOS Apple Silicon tree, SSTV+HAMDRM aggregate targets, analog-only and SSTV-off builds succeeded. Workflow definitions cover Windows x64, macOS Apple Silicon/Intel, Linux x86_64/ARM64 and Linux feature-off, but those jobs and their produced packages have not been executed or inspected for the final tree. |
| 2 | Existing Decodium tests pass or pre-existing failures are recorded | pass | The final local normal CTest invocation passed 120/120 tests in 170.89 seconds: 83 SSTV-labelled and 37 non-SSTV. The independently built ASan/UBSan configuration covered the same 120 tests in its two documented invocations without an address/undefined finding. |
| 3 | No second competing audio-capture engine | pass | Production SSTV consumes the existing Decodium PCM fan-out through a bounded relay/ingress. `src/sstv` creates no `QAudioSource`, independent CAT stack or external decoder process. |
| 4 | Analog RX from live, external/DecoPort and WAV audio | local-pass | Local sound-card, RTL-SDR, TCI and DecoPort paths publish source-labelled PCM to the same native ingress; WAV/replay and deterministic sources use the same runtime. WAV and source-adapter tests pass. An actual sound device, RTL-SDR/TCI/DecoPort session and RF reception have not been exercised. |
| 5 | Analog TX through loopback, WAV and real Decodium TX/PTT coordination | local-pass | Native loopback, atomic WAV export and the existing SoundOutput/CAT/PTT ownership path are connected and locally tested, including error and cancellation. No physical rig, audio output or RF transmission has been tested. |
| 6 | Every mandatory family in the canonical registry | pass | The registry contains 52 implemented analog rows spanning Martin, Scottie, Robot colour/monochrome, Wraase, Pasokon, PD, normal AVT and all required MP/MR/ML/narrow modes. FAX/HFFAX/WEFAX and AVT QRM variants remain separately classified and are not falsely advertised as implemented SSTV. |
| 7 | Correct timing, dimensions, VIS and colour order | local-pass | Fixed-point registry/family tests and pinned Handbook, libsstv, pySSTV and MMSSTV landmarks cover all implemented rows and preserve documented conflicts. Several rows still lack an independently captured compatible waveform or cross-application trial. |
| 8 | Deterministic encoder and decoder tests for required modes | pass | Per-family encoder, RX-session, runtime, coordinator, WAV and Studio coverage exists for every implemented analog row and is included in the current 83/83 SSTV-labelled pass. Robot B/W 8's canonical 66 ms and PySSTV-compatible 67 ms line periods additionally passed 40 repetitions and ASan. |
| 9 | Independent interoperability vectors for common modes | local-pass | Pinned PySSTV WAVs generated independently of Decodium pass native replay for Martin M2 (24.250 dB), Robot 36 (18.024 dB) and Robot B/W 8 (18.661 dB), all complete with coverage 1.0. Robot B/W 8 now explicitly accepts the independently observed 67 ms profile while native TX retains canonical 66 ms timing. Most rows and every external-decoder direction remain unverified. |
| 10 | Frequency-offset correction works | local-pass | Runtime tests cover automatic acquisition at -100 Hz and +100 Hz, manual/off modes and rejection of image-driven AFC. Live RF drift and tuning have not been measured. |
| 11 | Automatic and manual slant correction work | local-pass | Automatic and manual correction, retained-audio re-decode and -300/+300 ppm cases are executable and tested. No long live transmission or physical sample-clock mismatch has been measured. |
| 12 | Missing/damaged VIS timing fallback | local-pass | Registry-driven bounded timing fallback, ambiguity rejection, mode lock, retained early anchors and receive-without-VIS controls are implemented and tested. Damaged over-air captures remain untested. |
| 13 | Partial images are preserved safely | local-pass | Truncation, discontinuity, cancellation, storage and back-to-back tests preserve explicit partial state and valid lines. Live damaged transmissions remain manual evidence. |
| 14 | FSK ID RX and TX work | local-pass | Codec, streaming detector, post-image association and optional TX source pass deterministic tests. No independent complete FSK-ID waveform or on-air exchange has been executed. |
| 15 | Progressive rendering does not stall the GUI | local-pass | Worker-owned decoding, dirty/coalesced snapshots, asynchronous image providers, update throttling, offscreen QML tests and local performance evidence exist. Packaged-platform UI profiling remains open. |
| 16 | Atomic image save with metadata | pass | `QSaveFile`, bounded revalidation, sidecar/SQLite publication and fault paths have deterministic storage tests. |
| 17 | Responsive Gallery and thumbnails with a large collection | local-pass | Incremental `QAbstractListModel`, lazy thumbnail/cache and paged SQLite queries are tested with 5,000 synthetic records without a full model reset. Packaged UI profiling with a real collection remains open. |
| 18 | Database migrations are tested | pass | Storage and sharing use versioned transactional migrations. Schema-v3 sharing tests cover v1/v2 migration, rollback/restart and more than 10,000 closed inbox cycles with deterministic safe reclamation. |
| 19 | Opt-in, resumable remote upload and secure credentials | local-pass | REST, WebDAV and trusted pre-signed PUT adapters, durable pause/resume/retry, privacy-off defaults and direct secure-backend credential leases pass local tests. No production provider, real account or maintained-platform secure store has been exercised. |
| 20 | Validate incoming remote images before import | pass | Size, SHA-256, MIME/magic, dimensions, pixels, allocation, frame count, private metadata-free PNG normalisation and storage revalidation occur before explicit Gallery import and are tested. |
| 21 | Never disable production TLS validation | pass | Production transports require HTTPS, reject TLS errors and cross-origin credential redirects, and expose no normal UI bypass. Plaintext is available only behind the explicit test/development gate. |
| 22 | No secrets in logs or plain settings | local-pass | Provider values are held behind opaque leases, direct `SecureSettings` backend calls and central diagnostic/log redaction; focused tests scan ordinary settings. Backend-failure injection and real Keychain/DPAPI/Secret Service evidence are incomplete. |
| 23 | Versioned remote protocol documentation | pass | `REMOTE_SHARING_PROTOCOL.md` and `remote-sharing-openapi.yaml` define v1 capabilities, identity, validation, retry/idempotency, expiry, deletion and provider limits. |
| 24 | HAMDRM separated from analog SSTV | pass | HAMDRM has separate targets, profiles, controller, object/channel/PHY layers and `DECODIUM_ENABLE_HAMDRM`; analog-only builds do not discover or link OpenJPEG. |
| 25 | HAMDRM interoperability claims are backed by tests | local-pass | Native MOT/BSR/resume/OpenJPEG/channel/waveform/controller tests cover only the pinned subset described in the compatibility matrix, and no broader claim is made. Independent QSSTV/EasyPal waveform/object exchange is absent, so HAMDRM interoperability itself remains unverified. |
| 26 | Hostile WAV/image/metadata/network input is bounded and crash-safe | local-pass | Strict parsers, allocation/path/symlink controls, duplicate-key rejection and deterministic fuzz smoke cover the main boundaries. The real incoming-image staging boundary has a 259-case deterministic smoke test, and the complete current-tree ordinary CTest set passed under address/undefined sanitizers. Full coverage-guided libFuzzer corpus runs remain open: Apple Clang has no libFuzzer runtime locally and the configured Linux CI fuzz job was not executed. |
| 27 | Worker threads stop cleanly | local-pass | RX/replay, Studio, storage, sharing, TX and HAMDRM tests cover cancellation, stale generations and shutdown; the complete current-tree ordinary CTest set also passed under address/undefined sanitizers. Longer concurrency/shutdown stress and maintained-platform lifecycle tests remain open. |
| 28 | PTT releases after success/error/cancellation | local-pass | Coordinator tests cover success, timeout, audio loss/underrun, CAT disconnect, watchdog, destructor and cancellation during header, image and FSK ID, with exactly-once release. Real PTT feedback is unverified. |
| 29 | Negligible inactive overhead | local-pass | The post-fix local performance run measured 0.015 ms CPU over 755.027 ms wall time, no worker and zero chunks while inactive; 15 seconds of audio processed in 0.040 seconds (377.408 times real time). Cross-platform and long-duration application profiling remain open. |
| 30 | Complete mode/licence/security/user/developer documentation | local-pass | The documentation set includes architecture, mode/provenance, RX, TX, storage/gallery/QSO, sharing/OpenAPI, HAMDRM, security, performance, user/developer guides, release notes, this ledger and the final report. Italian translation validation reports 6,857 finished and 0 unfinished entries. The rebased implementation, build/CI and local hardening commits are above `upstream/main` `0bcd8b04a` (v1.0.584); the final local build/sanitizer evidence is recorded above, while external CI/package evidence remains open. |
| 31 | `MODE_MATRIX.md` exactly matches executed evidence | pass | `test_sstv_mode_docs` verifies all 64 registry/catalogue rows and generated cells; it passed within the current 83/83 SSTV-labelled run. External columns remain explicitly `unverified` where appropriate. |
| 32 | Release notes do not overstate support | pass | `RELEASE_NOTES.md` names the implemented subset and explicitly excludes unexecuted platforms, real radio/provider, E2EE and broad interoperability claims. It still requires final CI identifiers before publication. |
| 33 | No production stub, TODO-only feature or UI simulation | pass | Operator actions are wired to native controllers and unavailable capabilities are explicit. The final diagnostics fixes use stable ASCII source tokens, cap active TX refresh at 4 Hz plus terminal refresh, represent unavailable HAMDRM without fabricated zero metrics, persist the terminal test-tone result and guard/label the action as a real PTT/RF transmission. The current tree passed 83/83 SSTV-labelled tests, including Diagnostics/QML coverage. |

## Completion rule

This snapshot does **not** satisfy the complete Definition of Done. Completion
requires every row to be `pass`; `local-pass` records progress but never
substitutes for the platform, hardware, provider or independent evidence named
in the requirement. In particular, workflow YAML is not a platform build,
loopback is not radio/interoperability evidence, and a local provider is not a
deployed service. The final report records the current local test/build result;
actual final CI/package/hardware evidence is still required and must not be
promoted by inference. The visible workspace remains **BETA**; no GitHub
workflow, package upload, release or publication was performed for this
snapshot.
