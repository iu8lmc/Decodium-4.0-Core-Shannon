# Native SSTV implementation plan

Status: active design contract and progress ledger, 2026-08-24.

This plan implements SSTV inside the existing Decodium4 process. It does not
define a companion program, a second audio capture path, or a runtime bridge to
QSSTV, Python, Java, or Android code. The baseline is tag `v1.0.583`, commit
`119947690e2d8a1df99a75f98b915f2115df99e7`, on branch
`feature/native-sstv`.

## Invariants

- The production application remains `decodium_qml` (output name `decodium`)
  with its QML tree copied by the existing `sync_decodium_qml` target.
- RX samples enter SSTV only from the existing Decodium audio fan-out. Local
  sound-card, RTL-SDR, TCI and DecoPort sources must converge before the SSTV
  adapter; SSTV never opens a `QAudioSource`.
- TX ownership is granted by the existing Decodium TX/PTT policy. SSTV never
  opens a serial port or keys a rig directly.
- Core codecs do not depend on QML, a sound device, the gallery, networking, or
  mutable global mode state.
- Heavy DSP, image encoding, SQLite and networking never run on the GUI or
  audio callback thread.
- The project stays at C++17. Tests currently set a directory default of C++11;
  each SSTV test target will explicitly request C++17 rather than changing
  unrelated test targets.
- Analog SSTV remains usable when HAMDRM, remote sharing, or both are disabled.
- A mode is marked `implemented` only when timing, colour order, VIS handling
  and deterministic executable tests are complete. It is not marked
  `verified` until independent evidence and the exact interoperability scope
  are recorded in the mode matrix.

## Target structure

The implementation will use target-scoped CMake integration and these native
modules:

```text
src/sstv/
  core/         value types, registry, timing, VIS, FSK ID, colour conversion
  dsp/          resampling, tone/FM detection, sync, AFC, slant and metrics
  rx/           bounded audio ingress, replay buffer and analog RX state machine
  tx/           image preparation, DDS encoder, WAV stream and TX state machine
  integration/  Decodium audio, settings, controller and fail-safe TX ownership
  storage/      atomic files, SQLite worker, thumbnails and gallery model
  sharing/      providers, persistent queues, manifests and incoming inbox
  digital/      separately gated HAMDRM codecs, profiles, objects and BSR
qml/decodium/components/sstv/
tests/sstv/
docs/sstv/
```

`decodium_sstv_core` will be a C++17 static library linked by focused tests and
the Decodium application target. Qt-facing integration will be separate from
the codec library so command-line vector tests cannot accidentally depend on
QML or hardware. Optional libraries will be attached to the smallest target
that needs them.

## Delivery sequence and gates

### M0: audit and baseline

Deliverables:

- starting SHA/branch and clean-worktree evidence;
- actual audio, TX/PTT, QML, storage, security, network and packaging audit;
- upstream commit/licence inventory;
- the eight mandatory design documents;
- a clean baseline build and complete current CTest run.

Gate: the documents contain repository paths and observed limitations, the
baseline commands and results are recorded, and no production SSTV code has
been added before the audit.

### M1: reusable protocol core and encoder foundation

Implement in this order:

1. strongly typed `SstvModeId`, classification/family, capabilities and
   rational microsecond timing fields;
2. immutable canonical registry with uniqueness and consistency validation;
3. fractional sample accumulator that preserves long-run duration at every
   supported sample rate;
4. standard and extended VIS parser/generator with raw bits and confidence;
5. FSK ID framing, sanitisation, deterministic RX/TX symbol codecs;
6. audited RGB/GBR/Y-RY-BY/monochrome conversions;
7. phase-continuous DDS tone stream and bounded PCM sink interface;
8. deterministic analog encoder and streaming RIFF/WAV writer.

Gate: registry/timing/VIS/FSK/colour tests pass; common TX modes match an
independent timing oracle; phase continuity and exact segment counts are
measured. A clean self-round-trip alone is insufficient.

### M2: streaming analog receiver

Implement a bounded SPSC-style ingress queue fed by the current PCM signal and
a worker-owned pipeline:

1. source-rate metadata and stateful anti-alias resampler;
2. DC blocker, level/clipping metrics and conservative optional preprocessing;
3. leader/break detector and frequency-offset estimate;
4. VIS state machine and manual/no-VIS entry;
5. streaming FM estimator, sync tracker and line-period classifier;
6. per-line timing/slant correction and missing-sync prediction;
7. progressive dirty-rectangle events throttled before QML;
8. bounded replay buffer, partial-image completion and immediate return to
   leader search.

Gate: common modes decode independent fixtures; the state machine terminates
cleanly under truncation/corruption; ±100 Hz acquisition and ±300 ppm clock
error targets are measured; queue drops and DSP time are exported.

### M3: complete analog catalogue

Move one family at a time from `catalogued` to `verified`: Martin, Scottie,
Robot colour, Robot monochrome, Wraase, Pasokon, PD, AVT, MP/MR/ML, MMSSTV
narrow modes, then separately classified FAX/HFFAX/WEFAX variants. Each family
gets mode-specific colour/line tests and at least one independent oracle or
legally redistributable vector. Conflicting specifications remain blocked in
the matrix until resolved; they are never silently guessed.

Gate: every mandatory mode has an explicit RX/TX/auto-detect status and proof
cell. No user or release text says "all modes" unless every required row is
verified.

Current Wraase/Pasokon tranche: SC2-60/120/180 and P3/P5/P7 have one original
table-driven bounded RGB mapper, streaming TX encoder and one-scanline RX
decoder, automatic standard VIS selection in the existing RX runtime, shared
TX coordinator/WAV source construction and Studio descriptors. Timing and
geometry conflicts are represented as explicit compatibility profiles in the
registry and provenance audit. Executed pinned pySSTV landmarks cover
SC2-120/180 and P3/P5/P7; SC2-60 has deterministic self-generated evidence
only. All six therefore remain implemented, not verified, pending live-radio
and cross-application evidence; SC2-60 additionally needs a second executable
lineage or independent waveform. These RX sessions use the VIS-derived image
anchor and cumulative protocol clock; line syncs are observed for diagnostics,
but missing-sync reacquisition, measured slant correction and on-air clock/AFC
performance have not been demonstrated for this family. Pasokon transmits all
496 prepared RGB rows: the Handbook reserves the upper 16 for calibration but
does not define a unique grayscale pixel pattern, so the native Studio does not
synthesize one implicitly.

Current PD tranche: PD50/90/120/160/180/240/290 share one table-driven
cumulative mapper, phase-continuous bounded TX encoder and four-scan-bounded RX
decoder/session. The existing in-process RX runtime maps standard VIS
93/99/95/98/96/97/94 to these sessions; the shared TX coordinator, atomic WAV
export and Studio mode model use the same descriptors and exact geometry. A
radio scan is always 20 ms sync, 2.08 ms porch, Y-even, floor-average Cr,
floor-average Cb and Y-odd, and the mapper terminates at exactly `height/2`
pairs. Pinned pySSTV landmarks independently cover PD90-PD290; a separately
executed pinned libsstv path covers all seven only through the canonical image
boundary. Its verified extra-pair/out-of-bounds suffix is negative evidence and
is never reproduced. All seven remain implemented, not verified: no live radio
or cross-application exchange has measured long-duration AFC, slant or terminal
compatibility, and PD50 still lacks a second independent executable lineage.

Current AVT tranche: normal AVT24/90/94 share one cumulative, sync-free RGB
mapper plus a fixed-memory protected-countdown detector. The existing runtime
requires a mapped normal standard VIS and a valid inverse-protected 32x17
countdown before starting a progressive image session; it does not invent
per-line anchors after a discontinuity. TX, the SoundOutput/PTT coordinator,
atomic WAV export and Studio use the same native pull source. AVT90 preserves
the Handbook's 256x240 effective resolution separately from the audited
320x240 prepared/wire raster and uses prefix `101`, not the pinned MMSSTV
`010` defect. The complete AVT24 PCM runtime loopback and pinned source
landmarks are deterministic developer evidence only, so all three normal modes
remain implemented rather than verified. Narrow/QRM/Narrow-QRM VIS identities
are catalogued but deliberately have no RX, TX, autodetect, WAV or Studio
capability until complete picture semantics and independent evidence exist.

### M4: storage, gallery and native QML workspace

Add a controller and C++ models registered by `main_qml.cpp`, then a navigable
SSTV workspace under the existing QML application. Implement receive,
transmit, gallery, sharing, settings and diagnostics pages with existing theme
and `qsTr` localisation conventions.

Storage uses `QStandardPaths`, `QSaveFile`, content-validated `QImageReader`, a
named worker-thread SQLite connection, versioned transactional migrations and
path-only image records. Thumbnails are lazy and gallery changes are
incremental.

Gate: QML lint and rendered smoke checks pass; a large synthetic gallery stays
incremental; atomic-save, migration, quota-preview and hostile-image limits are
tested. The feature is reachable from normal Decodium navigation.

Current Gallery integration is native and incremental. Per-record actions can
atomically export a re-verified PNG on the storage worker, open the indexed
image in the existing Transmit Studio without keying the radio, start the
existing bounded WAV replay path when retained raw audio exists, or preselect
the image on the opt-in Remote Sharing page without queueing it. The export
refuses non-local/linked/non-PNG destinations, existing files unless explicit
replacement is requested, and the indexed source itself; it streams and hashes
the copy before atomic commit. Index-only removal remains deliberately labelled
and preserves PNG/sidecar files. A separate strongly confirmed destructive
action verifies every selected record, refuses unsafe/shared mandatory paths,
privately stages the owned PNG, sidecar, thumbnail and exclusive retained WAV,
then deletes the SQLite rows transactionally and removes the staged files.
In-process failures restore staged files; a bounded private journal lets startup
restore a pre-commit interruption or finish post-commit cleanup; shared optional
files are retained. Gallery favourites and versioned retention settings are now
first-class in SQLite/sidecars/model/QML. Separate image/thumbnail/raw-audio
quota inventory feeds a non-destructive deterministic preview; manual apply
requires an exact phrase and automatic apply is persisted opt-in, off by
default, with favourite/QSO/shared/unowned protections. Both reuse the same
bounded deletion journal. See `GALLERY_RETENTION_POLICY.md`. QSO logging and
retained-audio re-decode are integrated native workflows: Gallery opens the
bounded log/associate-QSO dialog or a retained-WAV replay, while Receive
re-decodes retained audio with the selected AFC, slant and mode controls.

### M5: Decodium TX/CAT/PTT integration

The SSTV coordinator validates prepared content, obtains exclusive TX
ownership, waits for policy-approved PTT, observes lead/tail delays, streams
PCM through the existing output selection, watches progress/underruns, and
releases ownership on every success, cancellation, error, disconnect,
shutdown and destructor path. A fail-safe guard is mandatory.

The current native VOX path no longer treats a logical VOX selection as proof
that the radio was already keyed. For live SoundOutput only, the coordinator
streams one bounded envelope consisting of a configurable 1900 Hz pre-key
tone, the unchanged SSTV/FSK payload, and a configurable hang tone. Protocol
landmarks are shifted explicitly, UI progress excludes the envelope, and
cancellation reaches all three phases through the same audio lease. CAT/PTT
continues to use confirmed feedback plus silent lead/tail barriers. WAV export
remains protocol-only. Timing controls are exposed on the SSTV Settings page,
are persisted through Decodium settings, apply only while TX is inactive, and
are locked for the duration of a session.

Local verification built and linked `decodium_qml`, rendered the Settings page
offscreen without QML warnings, and ran the coordinator suite twenty times.
The tests compare the emitted VOX tone samples with the native DDS, verify
absolute pre-key/header/image/payload/hang boundaries, progress semantics,
runtime timing updates, cancellation, lease retention and the absence of a
CAT PTT-off request for VOX. This is not an on-air VOX threshold or radio
hardware validation.

Gate: tests cover PTT success/timeout, cancellation at header/image/FSK,
device loss, watchdog and concurrent weak-signal TX rejection. WAV export and
internal loopback never key the radio. Real-radio validation remains a separate
manual result, not inferred from mocks.

### M6: secure remote sharing

Current native implementation:

- strict canonical manifest v1, bounded JSON/security helpers, provider
  abstraction, redacted failure classes and persistent transfer state;
- generic HTTPS REST outbound create/sequential-chunk/status/complete/cancel
  plus fail-closed capability/recipient/inbox/range-download/acknowledge/reject/
  incoming-delete/sender-block/revoke/remote-delete, WebDAV HTTPS
  collection/upload/status/delete/direct bounded GET and
  trusted-lease pre-signed PUT providers;
- SQLite schema v1 upload/download/inbox queue with bounded concurrency,
  deterministic retry, restart recovery, durable operator pause/resume for
  uploads and downloads, private staging, full SHA-256, explicit accept,
  acknowledge, reject and cancellation;
- deterministic tests for the core, HTTP providers and queue, including
  plaintext production gating, cross-origin redirect rejection, idempotency,
  restart boundaries, hash mismatch and inbox decisions;
- a dedicated sharing worker/controller exposed through
  `DecodiumBridge::sstvShare`, with privacy-off startup, user-configured HTTPS
  REST/WebDAV, capability-driven queue/history/inbox models and a rendered QML
  page;
- bounded outgoing image decode and dimension checks followed by an atomic,
  owner-only metadata-free PNG copy under native SSTV sharing storage; the
  operator's original gallery/storage image is never the queued mutable source;
- bounded incoming byte/hash/MIME/magic/dimension/pixel/allocation and
  single-frame checks, followed by an atomic private metadata-free PNG and a
  versioned handoff;
- an owner-thread Gallery storage consumer for that exact schema-v1 handoff,
  with repeated canonical/private path, no-symlink, byte/hash, PNG and bounded
  full-decode checks, exact-byte atomic publication in the existing imported
  layout, transactional SQLite insertion, post-commit staging cleanup and
  UUID/hash idempotency;
- a direct fail-closed `secure_settings::Backend` credential path for current
  Bearer/Basic providers; a focused test covers worker-thread store/remove and
  absence of the submitted secret from its ordinary settings file;
- a server-neutral reference contract in
  `docs/sstv/remote-sharing-openapi.yaml`, with no production endpoint.

Local verification on 2026-08-24 built the sharing UI and linked
`decodium_qml`; focused sharing CTest targets and the 1040x700 offscreen
Sharing-page render passed. No live server, real platform keychain/secret
store, packaged artifact or Internet interoperability was exercised.

This is a native bidirectional generic REST client tranche, not a completed or
deployed milestone. Provider incoming-delete, completed-object delete/revoke
and sender-block flows are wired through the queue, controller and Sharing page;
each is capability-gated and remains unavailable until an authenticated provider
advertises the corresponding executable operation. Native accepted-image
save/import and strongly confirmed local Gallery deletion are present. The
native storage import API, controller-to-storage integration test and queued Bridge lifecycle
connection exist; the controller's secure-store and lifecycle tests still do
not cover every failure or maintained release platform.

Qt 6 Sql and the runtime `QSQLITE` driver are mandatory for the persistent
queue. Linux build/package paths already mention the SQLite plugin, but every
produced Windows, macOS and Linux artifact must be verified. Missing `QSQLITE`
is a fail-closed initialization error, not an in-memory fallback.

Peer/relay remains design-only behind `SstvShareProvider`. Audit the existing
DecoPort/WebSocket control channels before any reuse decision. A relay would be
a separately deployed, explicitly configured service with independently
verified identity, authentication, limits, retention, privacy and operations;
no fictional production service or URL is embedded. E2EE remains unavailable
until an audited crypto dependency, key lifecycle, envelope vectors and
maintained-platform packaging exist.

Remaining gate: exercise provider delete/block/revoke and provider-event handling
against a deployed endpoint; prove provider events cannot reach TX/PTT; extend
expiry, quota/flood, TLS
certificate/hostname, worker lifecycle and credential-backend failure coverage;
scan every persistence/diagnostic surface for secrets; verify QSQLITE and the
platform secure store in maintained-platform artifacts; and run conformance
against any provider offered to users. Remote sharing stays opt-in and local
SSTV must work with no credentials or service.

### M7: separate HAMDRM subsystem

After the analog path is stable, implement a separate profile registry,
waveform RX/TX, object segmentation/integrity, partial persistence, BSR and
retransmission. OpenJPEG is optional, audited and target-scoped. QSSTV is an
interoperability reference; code with incompatible or non-commercial/research
licence terms is excluded and replaced clean-room.

Gate: enabled and disabled builds pass; known-good independent vectors cover
each advertised profile; malformed objects and carrier loss fail safely. The
UI never equates HAMDRM with analog SSTV or KG-STV.

### M8: hardening and release readiness

Add fuzz targets for VIS, FSK, WAV, manifests, chunk ranges, image limits and
HAMDRM objects; sanitizer CI; performance counters/benchmarks; full docs and
notices; and platform build/package jobs. Run complete regressions and verify
actual produced bundles contain required Qt image plugins and optional native
libraries.

The storage slice now has fixed-size, saturating counters for bounded database
queue depth and image-save duration/success/failure, with lifecycle,
concurrency, privacy and real-save tests. Other M8 counters, maintained-platform
benchmarks and package jobs remain separate work.

Gate: maintained Windows, macOS and Linux jobs build the application and SSTV
tests. Hardware/radio/platform claims are limited to what was actually run.

## Commit discipline

Commits are grouped by audit/docs, core types/registry, VIS/FSK/timing, encoder,
receiver DSP, individual mode families, storage, QML, TX coordination, sharing,
HAMDRM, hardening and final documentation. Unrelated formatting and cleanup are
excluded. Each commit must compile its affected targets and update the matrix
when capability evidence changes.

## Known prerequisites and early corrections

- The current CMake Hamlib symbol checks run two probes before setting required
  include paths, incorrectly hiding installed caching/get-conf2 capabilities.
  Correct this in a small baseline commit and test the cache result before SSTV
  TX integration relies on it.
- Existing CI exercises only selected tests; SSTV paths and a full application
  build need explicit triggers/gates.
- No native SSTV code or vectors exist in the starting tree. All compatibility
  rows begin as unimplemented until code and evidence change them.
- Large third-party fixture packs must not be committed blindly. Prefer
  deterministic generators or pinned optional packs with redistribution and
  SHA-256 metadata.
