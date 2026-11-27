# Native SSTV developer guide

This guide describes the maintained extension points of Decodium's native
SSTV subsystem.  It complements the evidence-oriented documents in this
directory; it does not replace the compatibility matrices or turn a compiled
path into an interoperability claim.

## Non-negotiable architecture rules

Native SSTV is part of the Decodium process and follows the application's
existing ownership model:

- the selected Decodium mono-PCM route is the only live RX source;
- SSTV must not create `QAudioSource`, PortAudio, ALSA, PulseAudio or another
  competing capture engine;
- the audio callback may validate, move a bounded PCM block and wake a worker,
  but it may not decode, allocate without a bound, read a file or use the
  network;
- QML consumes scalar state and immutable/coalesced image snapshots; it does
  not process audio or own decoder state;
- radio transmission uses Decodium's sound output and CAT/PTT ownership,
  including watchdog and unconditional release paths;
- WAV replay enters the same native RX runtime as live and DecoPort PCM;
- HAMDRM is a separate digital object/waveform subsystem and never masquerades
  as an analogue mode;
- remote sharing is IP transfer only and never keys PTT;
- credentials stay in the audited secure-settings backend and are never
  copied into ordinary settings, manifests or diagnostics.

Any change that violates one of these rules needs a new, reviewed architecture
decision rather than a local workaround.

## Build targets and feature gates

The top-level options are:

- `DECODIUM_ENABLE_SSTV`: builds the native analogue subsystem and its
  Decodium integration;
- `DECODIUM_ENABLE_HAMDRM`: builds the separate digital subsystem and requires
  native SSTV integration to be enabled;
- `DECODIUM_ENABLE_SSTV_FUZZING`: developer-only Clang/libFuzzer targets and
  requires tests plus the sanitizer runtimes;
- `DECODIUM_SSTV_EXTERNAL_VECTOR_DIR`: optional, local, absolute directory for
  the hash-pinned independent WAV pack.

The principal CMake aliases are:

- `Decodium::SstvCore` for protocol, DSP, image, RX/TX state and analogue mode
  implementations;
- `Decodium::SstvIntegration` for bounded audio ingress, runtime, replay, WAV,
  QSO and CAT/PTT/audio coordination;
- `Decodium::SstvStudio`, `Decodium::SstvStorage` and
  `Decodium::SstvGallery` for operator-facing image preparation and local
  persistence;
- `Decodium::SstvSharingCore`, `Decodium::SstvSharingQueue`,
  `Decodium::SstvSharingHttp` and `Decodium::SstvSharingUi` for the opt-in IP
  transfer boundary;
- `Decodium::SstvDiagnosticLogging` and `Decodium::SstvDiagnostics` for the
  bounded structured-event ring, scalar snapshot and atomic JSON exporter;
- the `Decodium::HamDrm*` targets under `src/sstv/digital/` for the digital
  feature gate.

With `BUILD_TESTING=ON`, `decodium_sstv_test_binaries` builds every ordinary
SSTV/HAMDRM test executable declared in `tests/sstv`.  Fuzzers remain explicit
because they need bounded run time and a corpus.

Do not add absolute developer paths or an implicit network download to the
normal configure/build.  Large independent vectors are optional, hash-pinned
and fail closed when their explicit directory is configured incorrectly.

## Component and thread model

The live analogue RX flow is:

```text
existing Decodium PCM producer
  -> DecodiumAudioSink / selected external relay
  -> SstvRxRuntime::enqueuePcm16* (bounded producer API)
  -> SstvAudioIngress / SstvPcm16Queue
  -> native RX worker
  -> resampler, preprocessor, frequency/tone observations
  -> leader/VIS or bounded timing fallback
  -> family RX session and sync/slant correction
  -> immutable progressive SstvImageSnapshot
  -> coalesced owner-thread notification
  -> DecodiumBridge scalar properties and QML image provider
```

`SstvWavReplayController` performs validated, bounded file reading on its own
worker and calls the same producer API.  It does not expose a shortcut to
family decoders.  A source switch creates a new generation; stale blocks and
late worker results must not be relabelled as the new source.

`SstvStorageWorker` owns its SQLite connection and image/sidecar filesystem
work on its dedicated `QThread`.  `SstvGalleryModel` remains an incremental
owner-thread model.  Sharing has a durable queue and provider/network
operations outside GUI/audio threads.  TX samples are pulled through
`SstvTxAudioDevice`; `SstvTxCoordinator` owns the transmit state machine and
the release/watchdog contract.

The operator-facing preparation, template, calibration, output diagnostic and
real PCM loopback contracts are documented in [STUDIO_TX.md](STUDIO_TX.md).
Calibration references enter the coordinator as prepared pull sources;
loopback instead owns a private replay `SstvRxRuntime` on an asynchronous
Studio job and never acquires SoundOutput or PTT.

Storage producers use the bounded, lifecycle-aware
`SstvStorageWorker::enqueueDatabaseOperation()` path. Direct `QMetaObject`
posting bypasses database queue-depth accounting and is not an acceptable new
producer pattern. Image-save timings and the redacted scalar snapshot are
defined in [PERFORMANCE_COUNTERS.md](PERFORMANCE_COUNTERS.md).

Every worker needs a tested, idempotent shutdown path.  External signal relays
must be disconnected before destroying the object they call directly.

## Canonical timing and signal representation

Protocol durations use integer picoseconds and are converted to samples with
`SstvTimingAccumulator`.  This carries fractional remainder across segments
and prevents a platform-dependent duration drift caused by repeatedly
rounding floating-point milliseconds.  Tone generation is phase-continuous.

Analogue image frequencies use the mode's documented black/white mapping.
RGB and full-range BT.601 YCbCr conversion is centralised in
`SstvColourConverter`; a family with different scaling must encode that fact
explicitly rather than silently changing the common converter.

VIS is LSB-first with parity and explicit leader/start/stop framing.  Standard,
extended and narrow/MMSSTV identities use their respective typed codecs and
detectors.  FSK ID is a separate bounded codec/detector/TX stream.  Do not
infer a mode merely from a display label or share private state between an
encoder and decoder test.

RX frequency correction has one authority.  An AFC update corrects the raw
frequency observations before the family session; a session must not apply
the same offset again.  Slant correction changes the time/sample mapping from
measured sync anchors or an explicit manual clock-error value.  It is not an
image shear performed after decoding.

## Adding or changing an analogue mode

Treat a catalogue identity, an implemented path and verified interoperability
as three different states.

1. Establish legal, pinned protocol sources and record conflicts in
   `UPSTREAM_PROVENANCE.md`.
2. Add one stable ID and unique long/short names to
   `SstvModeRegistry::canonical()`.
3. Populate classification, family, exact geometry, transmitted/displayed
   lines, timing, colour order, VIS identity, special ordering and evidence
   references.  A duplicate VIS needs an explicit documented shared-code
   group.
4. Leave capabilities unimplemented or blocked until executable protocol data
   and tests exist.  A note or a UI row is not capability evidence.
5. Put family protocol/mapping code under `analog/`.  Reuse common timing,
   image, VIS and tone primitives.  AVT keeps its sync-free countdown/session
   path; narrow modes keep their explicit extended identity handling.
6. Add TX source selection, RX session creation, runtime auto/manual selection,
   WAV/Studio support and cancellation/reset handling.  All dispatch switches
   must fail closed on an unknown enum.
7. Add deterministic protocol, encoder, decoder/session and runtime tests.
   Test exact duration/line count/order and hostile truncation as well as a
   clean path.
8. Add an independent fixture or source-derived oracle with licence, commit,
   SHA-256 and evidence classification.  Self-generated round trips remain
   useful but are not independent interoperability.
9. Update `MODE_CATALOG.md` and `MODE_MATRIX.md`.  The mode-documentation test
   requires exactly one catalogue row for every analogue/related-FAX registry
   ID and exactly one evidence row for every implemented mode.
10. Run strict warnings, repeat/stress, sanitizer and the relevant rendered
    QML/application builds before raising the claim.

The canonical registry must remain structurally valid.  Do not weaken its
validator or a documentation gate merely to accept a new row.

## Encoder and decoder extension rules

An encoder consumes a final, bounded image and emits a pull-based stream of
typed tone/sample events.  It must preserve phase, exact sample count, output
level and cancellation.  The Studio preview and WAV exporter use the same
source as live TX; a visually similar QML preview is not protocol evidence.

A decoder consumes streaming observations and publishes coherent progressive
snapshots.  It must preserve a safe partial frame on EOF, discontinuity,
abort or severe corruption.  Acquisition is bounded: invalid VIS, ambiguous
timing candidates and sync-like image content must not create an infinite
receive state.  Correction controls reprocess bounded retained audio through
the production path; they do not patch private pixels.

Channel tests should cover at least clean audio, positive/negative frequency
offset, sample-clock error, noise, clipping/DC, hum, impulse interference,
drop-out, missing/false sync and truncation.  Record the actually measured
limit per mode instead of copying a target into the compatibility matrix.

## Storage schema and Gallery changes

`SstvImageStorage` validates records, filenames, paths, hashes, dimensions,
metadata bounds and atomic PNG/sidecar operations.  `SstvStorageWorker` owns
schema migration and SQL.  When changing persisted data:

1. increment the typed current schema version;
2. add a transactional, restart-safe migration;
3. update exact schema validation;
4. bind values rather than composing untrusted SQL;
5. test new, prior, interrupted/invalid and idempotent-open cases;
6. keep full paths out of network manifests, ADIF and ordinary diagnostics;
7. update the Gallery role only if QML needs the field, and keep model updates
   incremental.

Deletion of owned files requires explicit UI confirmation and the private
staging/journal path.  Index-only removal must continue to preserve files.
Favourites, QSO associations, sharing state and retained raw audio participate
in retention rules and must not be bypassed by a new action.

## Adding a remote-sharing provider

Implement `SstvShareProvider` as a capability-driven asynchronous transport.
The durable queue remains the source of transfer truth; do not create a
provider-specific in-memory queue in QML.

A provider must:

- expose only verified upload/download/resume/revoke/delete/inbox/block
  capabilities and fail closed for unsupported operations;
- accept credentials through a short-lived secure lease, never an ordinary
  setting or loggable URL;
- enforce HTTPS in production, bounded redirects and no credential forwarding
  across origins;
- bound response bytes, JSON depth/nodes/strings, identifiers, chunk ranges,
  retry-after values and transfer count;
- preserve idempotency keys and durable resume offsets;
- validate hash, MIME, magic, dimensions and allocation before Gallery import;
- distinguish local blocking/deletion from a provider-side operation;
- redact secrets, signed URLs, local paths and private messages from metrics.

Generic REST, WebDAV and trusted pre-signed targets are transport profiles, not
a public Decodium service.  A pre-signed PUT profile still needs a trusted
broker/configuration that supplies a validated signed target.  Do not invent
one in the client.  Update the versioned protocol, OpenAPI description,
threat model and deterministic local HTTP tests with every contract change.

`SstvLocalIntegrationShareProvider` is the deterministic no-backend test
adapter. It may be supplied only by explicit developer/test injection, never
added to the production selector. Keep its one resident payload budget and
pending count/byte relationship intact. Production HTTP providers likewise
must reserve concurrent request+response bytes, cap active sessions and move a
terminal session to a minimal bounded idempotency record. Completion,
cancellation, revoke/delete, expiry and destruction must release bearer leases
and full session state. A capacity failure returns before acquiring another
credential or signed target.

## Diagnostics and structured logging

Use only the exact categories declared in `SstvDiagnosticLogging`: `sstv.core`,
`sstv.rx`, `sstv.tx`, `sstv.vis`, `sstv.sync`, `sstv.storage`, `sstv.share`,
`sstv.hamdrm` and `sstv.security`. Emit lifecycle transitions and terminal
errors at the controller/runtime boundary, never per sample, pixel, parser byte
or UI refresh. Every event name and scalar key must be on the central allowlist;
do not pass free-form exception detail and then rely on substring redaction.

The ring is bounded and thread-safe, and its sequence stays monotonic across a
clear. `SstvDiagnosticsController` accepts scalar maps only from known
RX/TX/storage/share/HAMDRM producers, whitelists SSTV settings, and writes a
versioned JSON snapshot asynchronously through `QSaveFile`. Tests must inject
hostile URLs, paths, secrets, person metadata and unknown keys and prove that
the export fails closed. Images, thumbnails, audio, local/device identifiers,
callsigns, grids, messages, envelopes and credentials are never diagnostic
fields.

## HAMDRM changes

Digital work stays under `src/sstv/digital/` and its own targets, registry,
controller and QML page.  Profiles describe the implemented subset; they do
not imply full ETSI DRM broadcast compatibility or EasyPal/QSSTV
interoperability.  OpenJPEG input is bounded before decode and its runtime
library and notice must be packaged whenever HAMDRM is enabled.

Known-good self vectors prove internal consistency only.  BSR, resume,
segment/CRC failure and waveform impairment tests must be reconciled with
`HAMDRM_COMPATIBILITY_MATRIX.md`, and external interoperability remains
unverified until another implementation is actually exercised.

## QML, settings and localisation

All operator functions belong in the navigable SSTV workspace and call typed
backend operations.  A button must have a real success/error/cancellation path
and must not simulate a transfer, decoder or transmitter in JavaScript.

Settings need a bounded native default, validation, persistence and a backend
consumer.  Secrets use secure settings only.  New user-visible strings use
`qsTr`, are added to `translations/decodium_it.ts`, and the `.qm` catalogue is
rebuilt with no unfinished or placeholder mismatch.  Render tests use the
offscreen/software backend, but an offscreen pass is not a human visual check.

## Required evidence before merging

At minimum run:

- a clean configure and `decodium_sstv_test_binaries` build;
- complete CTest label `sstv`, followed by focused repetition for stateful,
  threaded and protocol paths;
- analogue-only and analogue-plus-HAMDRM configurations;
- strict compiler warnings on changed C++ targets;
- ASan/UBSan and bounded real libFuzzer corpora where supported;
- the performance executable and rendered QML tests;
- application link plus feature-disabled build;
- package/runtime dependency checks;
- the maintained platform CI workflow.

Use `tests/sstv/fixtures/EXTERNAL_VECTORS.md` for the optional independent WAV
pack.  Report local mocks, self-round-trips, source-derived landmarks,
independent waveform decode, cross-application decode, live audio/radio and
packaged-platform runs as different evidence classes.

The living completion status is `DEFINITION_OF_DONE.md`.  A pending external
or platform gate must remain pending; it is not resolved by wording such as
"framework ready".
