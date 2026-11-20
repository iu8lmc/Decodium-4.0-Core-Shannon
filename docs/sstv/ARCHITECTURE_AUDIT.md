# Decodium native SSTV architecture audit

Audit date: 2026-08-23/24. Starting branch `main`, starting tag
`v1.0.583`, starting commit
`119947690e2d8a1df99a75f98b915f2115df99e7`. The implementation branch was
created as `feature/native-sstv` from that exact commit after fetching both
configured remotes and confirming `HEAD`, `origin/main` and `upstream/main`
were equal. The worktree was clean.

No file, symbol, QML component, CMake target or test containing SSTV, slow-scan,
HAMDRM, VIS or FSK-ID functionality existed in the starting tree. The feature
must therefore be integrated into Decodium4 rather than wrapping pre-existing
SSTV code.

## Baseline build and tests

Environment actually used:

- macOS 26.5.2 ARM64, 10 logical CPUs, 16 GiB;
- CMake 4.3.1, Ninja 1.13.2 and AppleClang 21;
- Qt 6.11.0, Boost 1.90.0_1, FFTW 3.3.11, Hamlib 4.7.0, libomp and
  librtlsdr 2.0.2 from Homebrew;
- Release build, `BUILD_TESTING=ON`, macOS deployment target 13.0.

Commands executed before production changes:

```zsh
cmake -S . -B build
cmake --build build --parallel "$(sysctl -n hw.ncpu)"
ctest --test-dir build --output-on-failure -j "$(sysctl -n hw.ncpu)"
```

Configuration succeeded. The existing Ninja dependency database reported a
premature EOF and recovered, after which all 734 build actions completed. Both
`build/Decodium4.app/Contents/MacOS/Decodium4` and the current QML frontend
`build/decodium` were linked. CTest passed 37/37 tests in 96.64 seconds.

The green total has known coverage limits: `test_qt_helpers` passed 154
subtests and skipped 12 FST4/MSK144/FT8/FT2 cases whose fixtures/comparators
were absent; one FT8 accepted-limit test permits a miss. The RTL-SDR hardware
test is built but intentionally not registered without hardware opt-in. There
were no SSTV tests, independent SSTV vectors, rendered SSTV UI checks or radio
SSTV tests.

Existing configure/link warnings include old CMake policies, Qt `GuiPrivate`,
missing Vulkan headers and Homebrew libraries built for a newer macOS target.
They did not fail the baseline but remain visible debt.

## Build architecture

The root [CMakeLists.txt](../../CMakeLists.txt) explicitly lists application
sources. `DECODIUM_SOURCE_DIRS` covers `src/app`, `bridge`, `core`, `models`,
`net`, `radio`, `security`, `services` and `ui`; SSTV must be added as a
target-scoped module rather than extending global include paths indiscriminately.

Two desktop executables are built:

- `wsjtx`, the legacy widgets application, includes `DecodiumBridge`, audio,
  CAT, RTL-SDR and current services;
- `decodium_qml`, output name `decodium`, is the active QML frontend and also
  compiles much of the legacy/backend source set.

The QML tree is not a compiled QML module. `sync_decodium_qml` removes/copies
the complete source `qml` directory into the build output, and installation
copies that directory beside the executable. New SSTV QML files will therefore
be packaged automatically only if the existing sync/install steps remain
intact; the C++ controller/model sources still need explicit target wiring.

The application compiles as C++17. `tests/CMakeLists.txt` sets a C++11 directory
default, so SSTV tests must request C++17 per target. The global language level
will not be raised.

### Hamlib feature-probe defect

`CMakeLists.txt` probes `rig_set_cache_timeout_ms` and `rig_get_conf2` before
putting the discovered Hamlib include path in `CMAKE_REQUIRED_INCLUDES`.
Configure therefore reports `hamlib/rig.h` missing and leaves
`HAVE_HAMLIB_CACHING`/`HAVE_HAMLIB_GET_CONF2` unset even though installed
Hamlib 4.7.0 exposes both symbols. `HAVE_HAMLIB_SEND_RAW` succeeds because its
later probe has the correct prerequisites. This does not break the baseline
build, but it wrongly disables CAT capabilities relevant to robust SSTV TX. It
will be corrected in a small isolated commit and covered by configure evidence.

## RX audio architecture

### Local sound-card input

`Audio/soundin.{h,cpp}` owns the existing Qt `QAudioSource`; macOS can instead
use its existing AudioQueue/pull implementation. It writes into an
`AudioDevice` supplied by the bridge. SSTV must not instantiate either class or
open another device.

`src/bridge/DecodiumAudioSink.h` is the principal local-audio convergence point:

- it converts configured channels through `AudioDevice::store`;
- its normal 48 kHz path uses the existing 49-tap `fil4` decimator to 12 kHz;
- it appends to the bridge's mutex-protected PCM buffer;
- it emits level/health metrics and `audioSamplesReady(QVector<short>)` after
  releasing the buffer mutex;
- `setDiscardSamples` prevents RX audio leaking into decoders during TX;
- `injectExternalSamples` feeds already-12-kHz DecoPort samples through the
  same buffer, metrics and signals.

The audio callback already does conversion, a bounded-size block copy and
metrics. SSTV may only enqueue/move the emitted block and atomically update
counters there. Resampling, tone detection, images and I/O belong to an SSTV
worker. The adapter needs an explicit queue capacity/drop metric; connecting a
long-running receiver directly to the GUI thread would repeat a known FT2-Link
CPU problem documented in the current bridge.

### External input paths

The sources do not all currently traverse one signal:

- DecoPort remote audio calls `DecodiumAudioSink::injectExternalSamples`, after
  local capture has been excluded. It is already at the 12 kHz decoder rate.
- RTL-SDR weak-signal PCM is delivered by `RtlSdrInput::pcmSamplesReady` to
  `DecodiumBridge::onRtlSdrPcmSamplesReady`, which appends directly to the same
  decoder buffer and async ring.
- TCI/WebSDR PCM arrives by a queued connection from
  `DecodiumTransceiverManager::tciPcmSamplesReady` and likewise appends directly
  in `onTciPcmSamplesReady`.
- RTL-SDR listening audio has a separate `audioSamplesReady(samples,
  sampleRate)` path and must not be confused with weak-signal decoder PCM.
- current WAV/recording support feeds existing decode paths but is not a
  general SSTV streaming source yet.

Accordingly, subscribing only to `DecodiumAudioSink::audioSamplesReady` would
miss valid RTL-SDR and TCI sources. Integration will add one lightweight
source-labelled RX PCM fan-out after each existing source has selected its
decoder stream, with sample-rate metadata. That fan-out feeds the bounded SSTV
adapter; it does not capture audio or mix simultaneous sources. Local card,
DecoPort, RTL-SDR, TCI, imported WAV, replay and deterministic tests share the
same SSTV ingestion contract.

The current decoder rate is normally 12 kHz, but the SSTV contract will accept
the required rate set and use a stateful anti-alias resampler. Source rate and
type are stored in reception metrics rather than inferred from block size.

## TX audio, CAT and PTT architecture

Decodium already owns output-device selection, `QAudioSink` lifetime,
resampling/format negotiation, TCI TX audio and platform workarounds inside
`DecodiumBridge`. SSTV must stream through an extension of that output path;
creating a second `SoundOutput`, serial PTT stack or direct radio manager would
violate ownership and platform safety.

Current TX state is centralized in `DecodiumBridge` (`m_txRequested`,
`m_transmitting`, tuning and RX-suspension state). It exposes pending and
confirmed PTT state and implements:

- active CAT selection across native, Hamlib, OmniRig, CAT4OM, legacy and
  DecoPort paths;
- `PttTransitionPolicy` confirmation modes and timeout;
- preflight before PTT, split/frequency synchronization and asynchronous PTT;
- delayed PCM start only after accepted confirmation policy;
- one-shot PTT-off, abort, RX restoration and audio restart paths;
- output progress, hard deadlines, playback completion and TX watchdogs.

`DecodiumTransceiverManager` already serializes rig state and implements CAT,
DTR, RTS and VOX policy; `setRigPtt` is not an API SSTV should call directly.
The SSTV coordinator will request exclusive ownership through a bridge-facing
native TX interface, provide a bounded PCM producer, and receive lifecycle
callbacks. Its RAII/fail-safe lease must funnel all exit paths back through the
existing one-shot PTT release and RX restoration. WAV export and internal
loopback bypass this lease completely.

The current arbitrary-waveform and DecoPort paths materialize a complete
`QVector<float>` before playback/12 kHz framing. That is O(transmission
duration) and is not acceptable for long SSTV images. The existing fallback
that waits for PCM preparation is capped around 25 seconds and does not re-arm;
it is not a long-stream watchdog. The native seam therefore needs pull/chunked
PCM with bounded buffering while retaining the existing `stopTx()` teardown.
DecoPort's remote TX watchdog also releases PTT after roughly three seconds
without audio, so the chunk pacer cannot leave unbounded gaps.

The existing `test_ptt_transition_policy`, `test_audio_sink_tx_gate`,
`test_tx_pipeline`, amplifier/QMX telemetry tests and legacy PTT flow are
regression surfaces. They do not yet test an arbitrary long SSTV stream.

## QML and navigation

`src/app/main_qml.cpp` constructs one `DecodiumBridge`, publishes it as both
`bridge` and `appEngine`, registers native visual types, and loads
`qml/decodium/BootLoader.qml`/`Main.qml` from the copied runtime tree. Existing
components use `qsTr`, the shared theme manager, keyboard/focus patterns and
context properties.

Application modes are currently exposed by
`DecodiumBridge::availableModes()` and consumed by `TxPanel.qml`. SSTV needs a
normal navigable workspace in Decodium4, not a hidden endpoint or detached
external GUI. The initial UI seam will register a native `SstvController` and
models in `main_qml.cpp`, add an `SSTV` application-mode/workspace route, and
load pages from `qml/decodium/sstv`. Analog, related FAX, remote sharing and
digital HAMDRM remain visibly distinct categories.

Adding only the string is unsafe: the bridge's canonical-mode validation does
not accept SSTV, and unknown modes fall through existing period helpers to a
15-second FT8-like default. SSTV monitoring/TX lifetime must therefore use its
own state/timing boundary and be excluded from weak-signal slot scheduling.
`SettingsDialog.qml` also hard-codes fourteen lazy tabs and an index clamp; a
new SSTV settings surface must update model, clamp and Loader together or live
inside the dedicated workspace.

The controller publishes compact state/progress/dirty-rectangle signals. QML
will not receive a full image `QVariant` per line, manipulate pixels in
JavaScript, enumerate the gallery synchronously or own DSP state. Every new
user string uses `qsTr` and the existing `translations/decodium_*.ts` workflow.

## Settings and secrets

Normal configuration uses the existing Decodium `QSettings` profile and bridge
get/set conventions. SSTV keys will be namespaced and migrated without changing
unrelated defaults. Monitoring/passive detection, raw audio, upload, public
sharing, incoming download, FSK ID and HAMDRM default to the safe values in the
mission.

`src/security/SecureSettings.{hpp,cpp}` abstracts macOS Keychain, Windows
Credential Manager and Linux Secret Service/`secret-tool`, including migration
helpers and tested fallback behaviour. Provider passwords, bearer/refresh
tokens, API keys and private keys must use this service. They must not be
exposed as persistent QML properties, stored in SSTV SQLite/QSettings or
included in diagnostics. A platform without secure storage must display the
capability accurately and must not silently enable remote sharing.

The existing generic helper deliberately falls back to returning/writing the
secret in plaintext when the secure backend is unavailable. That behaviour is
acceptable for compatibility paths already covered by its tests but does not
satisfy the SSTV remote-secret requirement. The SSTV credential adapter must
fail closed (or require an explicit non-secret provider mode) instead of using
the plaintext fallback.

## SQLite, files and gallery

The repository demonstrates the correct Qt SQL thread rule in
`lib/persistence/DecodeHistoryWorker`: a worker owns a named SQLite connection
separate from the GUI/default connection. Map/callsign services also use named
connections, schema checks and incremental models. SSTV will follow this model
with a dedicated worker-owned database/schema and transactional version table;
full-resolution images remain files, not SQLite BLOBs.

The current QML bootstrap opens `AppLocalDataLocation/db.sqlite`, enables WAL
and creates `sessions`/`decodes`. It has no `PRAGMA user_version`, schema-version
table or reusable migration ledger, and is created before profile selection so
it is shared across profiles. `DecodeHistoryWorker` is deliberately specific
to decodes. SSTV will not append ad-hoc columns to it: a separately versioned
SSTV schema/connection avoids coupling image migrations to decode history while
stable QSO identifiers provide the required association.

Paths use `QStandardPaths`; atomic writers already use `QSaveFile` elsewhere in
the project. SSTV stores received, transmitted, digital, remote, thumbnail,
raw-audio, WAV and transfer-temp data in separate application/user-selected
locations. The gallery is a `QAbstractListModel` updated per row/page, not a
recreated QML array. QSO association stores stable local identifiers and does
not invent invalid ADIF attachment fields.

## Networking and remote services

The repository has Qt Network services, a shared `NetworkAccessManager`,
tested bounded/redirect-aware `FileDownload`, WebSocket-based services and
DecoPort authenticated remote-radio transport. None is an audited production
SSTV recipient/inbox backend. DecoPort cannot be repurposed merely because it
already carries audio: its identity, authorization, persistence and object
semantics must first satisfy the versioned SSTV protocol.

Several existing services own independent `QNetworkAccessManager` instances;
there is no single global manager to reuse blindly. The SSTV provider manager
must own its QNAM in the correct QObject thread and explicitly react to
Decodium's offline state. The current shared subclass aborts SSL-error replies,
and no active production call to `ignoreSslErrors` was found.

Remote sharing will therefore start with a provider interface and deterministic
local server. Generic HTTPS REST, WebDAV and trusted pre-signed PUT providers
will use Qt Network with strict TLS, bounded responses, redirect-origin checks,
redacted diagnostics and persistent queue state. No cloud hostname or
unauthenticated relay is hard-coded. Local analog SSTV remains independent of
all providers.

## Logging and diagnostics

`src/bridge/DecodiumLogging` establishes application logging and
`DecodiumDiagnostics`/bridge status paths expose operational data. SSTV adds
`QLoggingCategory` names `sstv.core`, `sstv.rx`, `sstv.tx`, `sstv.vis`,
`sstv.sync`, `sstv.storage`, `sstv.share`, `sstv.hamdrm` and `sstv.security`,
then routes user-visible summaries through the existing diagnostics surface.
The export is redacted and excludes image/audio unless the operator explicitly
selects them.

The existing diagnostic API declares audio/rig/recent-log setters but the audit
found no normal producers for those setters beyond QML warning collection.
SSTV cannot assume automatic ingestion or redaction; it must connect its own
bounded, already-redacted summaries and test that no tokens, signed URLs,
private metadata or payload contents reach either log path.

## CI and packaging

The existing test workflow covers macOS Intel, Ubuntu and Windows but builds a
small selected test set; its path filters do not include future SSTV/QML paths.
Windows PR packaging builds the application with tests off. Release macOS,
Windows and Linux packaging do not constitute a complete CTest gate, and there
is no current `qmllint` gate.

SSTV work must add path triggers and core tests across platforms, full
application builds, analog-only and HAMDRM-enabled configurations, QML lint,
sanitizer/fuzz jobs and bundle inspection. OpenJPEG, if enabled, must be found
and packaged target-locally. New QML/assets use the existing copy/install flow;
image-format plugins and optional dylib/DLL/shared-object presence must be
verified in actual packages.

## Architectural decisions from the audit

1. Build a native C++17 `decodium_sstv_core` plus Qt integration modules; no
   monolithic bridge implementation and no external process.
2. Add a source-labelled PCM fan-out at existing Decodium convergence seams,
   not another capture stack.
3. Use a bounded worker queue/replay buffer and emit throttled incremental UI
   updates.
4. Extend the existing TX/PTT ownership boundary with a generic PCM producer;
   never call CAT/PTT independently from SSTV.
5. Use dedicated worker-owned SSTV SQLite storage and existing secure settings,
   path, theme, localisation, diagnostics and packaging conventions.
6. Keep analog, FSK ID, remote IP sharing and HAMDRM separate in types, state
   machines and UI.
7. Treat upstream mode tables as provenance-bearing evidence, resolve conflicts
   independently and exclude restricted HAMDRM source.
8. Expose SSTV through normal Decodium4 navigation so the finished feature is
   usable in the single installed application.
