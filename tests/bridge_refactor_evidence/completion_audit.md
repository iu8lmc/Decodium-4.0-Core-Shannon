# Bridge refactor completion audit

Snapshot: 2026-11-10. This is an evolving review, not a claim of completion.
The objective is to split the Bridge while preserving behavior, with A/B and
functional evidence for every touched function. Cross-platform test execution
was removed from the goal by the user; portable implementation is still
required. CAT, physical PTT and RF are not necessary to claim the virtual-audio
results below.

## Function-level evidence added after the snapshot

| Functions | Comparison and functional evidence | Boundary |
| --- | --- | --- |
| `queueFt4DecodeRequest`, `maybeDispatchFt4EarlyDecode` | `bridge_ft4early_ab.py` compares the moved methods; `ft4-early-audio-ab.txt` records both placements submitting a generated FT4 CQ to the actual worker and displaying the result. | The app test injects a PCM buffer and forces native publication; it does not exercise RF. |
| `tryAutoCallFromRows` | The existing `bridge_autocallselect_ab.py` compares the source and controlled behavior; `autocall-wav-ab.txt` records both placements selecting K1ABC after an actual WAV decode. | CAT and rig control were disabled and immediate TX dispatch suppressed. |
| `appendAdifRecord` | `adif-folder-target-ab.txt` retains the baseline/current harness results for writing and appending; `adifappend-runtime.txt` records a synthetic QSO written through the application. | The functional app result is recorded separately from the controlled-serializer A/B harness. |
| `openActiveLogbookFolder` | `adif-folder-target-ab.txt` compares baseline/current output URLs, and the current app opened its isolated profile folder in Finder, as recorded in the main report. | Actual Finder observation covers the current placement only. |
| `startTx` | `start_tx/before.json` and `start_tx/after.json` record the method reached in both placements, nonzero virtual audio and CQ decoded by the other isolated session. | One synthetic FT8 CQ; CAT disabled, no physical PTT/RF. |
| `onFt8DecodeReady` | `ft8-ready-helper-repair-remaining-ab.txt` records successful before/after FT8, FT2, JT4 and other fixture-driven decode/publication probes; the main report describes the saved pre-extraction fixture and guard tests. | Forced native publication is used on the macOS legacy setup; it does not establish every callback branch or RF. |
| `checkSwrAllowsTransmission`, `enforceSwrTransmissionLimit`, `enforceWsprPowerTransmissionLimit` | `bridge_swr_admission_app_ab.py` compares the relevant v1.0.737 and current method bodies and runs them in isolated applications; `swr-wspr-stop-app-ab.txt` records matching threshold decisions and simulated stop results. | Telemetry and active TX/CW state are synthetic; CAT and embedded legacy rig control are disabled. |
| `ft2LinkAccessPasswordConfigured`, `verifyFt2LinkAccessPassword` | The full A/B output covers configured, missing, weak and malformed synthetic credentials. `ft2-access-success-ab.txt` additionally records wrong-password rejection, successful unlock and relock in both application placements. | Synthetic credentials and local application only; no external service or user credential was exercised. |
| `mamPruneSlots` | The existing focused A/B harness checks pruning decisions; `mamprune/before.json` and `after.json` show both app placements removing one logged synthetic slot while retaining another. | This app observation does not establish RF or every timeout case. |
| `replayPskReporterRecentEntries` | The existing A/B harness compares selection; `psk-replay-app-ab.txt` shows both app placements queueing the same recent complete call. | External reporting was disabled. |
| `publishRemoteActivityEntry` | The existing A/B harness compares guards and cache behavior; `remote-activity-websocket-ab.txt` records matching real localhost WebSocket publication and reset in both placements. | Credentials and activity are synthetic; external networking was not tested. |
| `ensureTxAudioPrepared` | The existing A/B harness compares the implementation and `txprepare/cache-{before,after}.json` records two starts and a cache hit in both placements; the main report documents additional virtual-audio modes. | Cache results alone do not establish RF or every error path. |
| `addStationFrequencyRow`, `updateStationFrequencyRow` | `stationfreq-real-ab.txt` records A/B add, replace, update and failure cases against the real station/band models and a temporary settings file. | Bridge and CAT collaborators are simulated. |
| `rememberRecentAutoCqAbandoned`, `rememberRecentAutoCqWorked` | `autocqhistory/cache-ab.txt` and `autocqhistory/runtime-ab.txt` record matching A/B cache handling and app worked/abandoned, cooldown and clearing behavior. | Synthetic contacts; no RF. |
| `maybeStartCourtesySignoff` | The existing fixture comparison and `courtesy-success-runtime-ab.txt` cover both placements starting and releasing the synthetic signoff state. | CAT off; the listed result does not prove on-air signoff. |
| `drainLegacyDecodeSecondaryWork`, `drainNativeDecodeSecondaryWork` | `secondary-harness-ab.txt` compares ordering, deduplication and routing with substitute collaborators; `secondary-isolated-app-ab.txt` records a decoded FT8 caller in both app active-stations models. | The app result covers native decoding; legacy external side effects remain simulated. |
| `pendingLogQsoPreview` | `logpreview-full-ab.txt` and `logpreview-runtime.txt` record the controlled A/B and isolated app preview scenarios for both placements. | The runtime test uses a synthetic QSO and does not commit it. |
| `readDecodeUserFilterConfig` | `filtersettings-ab.txt` records the controlled A/B scenarios for the pre-extraction and moved bodies; the decode-filter review below describes the separate application evidence. | The earlier app blacklist probe was not fully isolated from shared legacy test settings. |
| `haltWithReason`, `finishModulatorIdlePlayback` | `halt/direct-fix-cw-ab.txt` and `cw-natural-modulator-fix-ab.txt` record deliberate before/fix differences and current successful CW state cleanup; related virtual-audio evidence is described below. | These are intentional corrections, not identical-behavior claims; no physical relay was measured. |
| `saveSettingsInternal` | `save-settings-restart-callsign-ab.txt` records matching synchronous/asynchronous settings scenarios across isolated application restarts for both placements; the saved fixture comparison is described below. | The fixture precedes extraction but is not identical to v1.0.737; not all settings were exercised. |
| `saveWindowStatesAsync` | The existing A/B suite covers the window-state methods, while `window-state-current-runtime.txt` records synchronous and asynchronous save/read in the current application. | Native visual window placement was not established. |
| `resolveTxOutputDevice` | The A/B harness covers resolution/fallback, and `tx_output_device/{before,after}.json` records the instrumented method reached during FT8 virtual audio in both placements. | No physical output device or RF. |
| `replayWorldMapEntry`, `queueWorldMapEntryForReplay`, `worldMapEntryFreshEnough` | Existing targeted A/B harnesses compare replay and freshness rules; `worldmapreplay-wav-isolated-ab.txt` records decoded-contact replay in release and current apps. | The recorded WAV scenario does not prove every map entry or rendered pixels. |
| `initialiseSstvStorage` | `bridge_sstv_storage_startup_share_ab.py` compares the pre-extraction fixture and moved body and runs both app placements; `sstv_storage_startup_share/{before,after}.json` records two verified signed encrypted images and gallery import. | Local test sessions and synthetic data; no deployed server or RF. |

These entries describe the named paths, not every branch or all Bridge methods.

SSTV TX evidence provenance: the main report records dedicated before/after
FAX480, Radiofax, HAMDRM and calibration loopbacks, but their individual
console transcripts were not located in the evidence directory during this
review. Retained Radiofax and HAMDRM producer-tap comparisons separately show
successful virtual transfers, but compare tap placement rather than TX-method
placement. They do not replace the missing dedicated comparison transcripts.
No loopback was repeated in this review.

The calibration capture was subsequently rerun after repairing its moved
output-device helper declaration in the test harness. Both placements emitted
the expected 1200 Hz tone (measured FFT bin 1195.31 Hz, resolution 11.72 Hz),
completed, and released TX ownership. CAT and embedded legacy rig control were
disabled. The retained result is `sstv-calibration-capture-ab.txt`; it covers
`startSstvCalibrationTone`, not RF or a physical PTT.

The dedicated `startHamDrmPreparedAudio` two-app A/B was subsequently rerun
with embedded rig control disabled after fixing its moved-helper declaration.
Both TX placements transferred the same 225 bytes exactly over BlackHole;
`sstv-hamdrm-tx-loopback-ab.txt` retains the results. This is Decodium-to-
Decodium, not independent EasyPal interoperability or an RF test.

The Target Call WAV lab reconstructs the previous placement from the moved
body; that body was checked against v1.0.737, normalizing the shared logger
name and one additional blank line. The Auto Call WAV lab reconstructs its
previous placement from the moved body; that body matches the saved
pre-extraction fixture. Both scripts now enforce these source checks before
running. The existing WAV results are placement comparisons, not separate
release-binary tests; no WAV probe was repeated for this source check.

FST4-15 recorded failure review: the clean PCM was identical in both Bridge
placements. At 1480–1530 Hz neither produced the expected CQ; the official
WSJT-X decoder also failed at those matched bounds. At 1480–1550 Hz both
Bridge placements decoded and displayed the expected CQ using the same PCM.
The earlier apparent reference difference used different bounds. The FST4
worker source files have no diff against v1.0.737. This is evidence of a
range-sensitive test condition in both placements, not proof of every FST4
case or normal legacy/RF operation. See `fst4-clean-matched-range-ab.txt` and
`fst4-clean-wide-ab.txt`; no existing test was repeated for this review.

JT65 decoder correction review: the FFT direction change in
`Detector/LegacyDspIoHelpers.cpp` is an intentional decoder fix, not merely a
Bridge extraction. `jt65-fft-direction-ab.txt` records the same reference WAV
failing with the previous direction and passing with the corrected direction in
both Bridge placements. An official WSJT-X binary independently decoded the
fixture; all nine message texts were compared with corrected Bridge output.
The internal stage comparator used a shared percentile implementation and had
an unresolved numerical tolerance difference, so it is not a fully isolated
proof of internal equivalence. These tests forced native snapshot/publication
on the Mac and do not establish normal legacy-mirror or RF behaviour. Existing
evidence was reviewed without repeating the tests.

Startup changes review: delaying legacy initialization until QML readiness and
preventing profile-copy growth are intentional changes alongside the refactor.
The retained `startup-order-ab.txt` covers readiness, fallback and shutdown
with controlled backend collaborators; the app ordering test includes a QML
stall. Two-app legacy BlackHole audio recovery and mode switching were also
reported as passing. An attempted legacy FT8 decode comparison was inconsistent
between runs and the WAV decode-model condition failed in both versions; those
attempts are explicitly retained as unresolved rather than passing evidence.
The 28.503 s versus 1.900 s constructor measurements used sanitized temporary
copies with different profile sections, not matched live-operator measurements.
No existing test was repeated for this review.
Review of the earlier baseline legacy FT8 log found `nout=0` in the recorded
decoder metrics and no logged blacklist rejection. The shared test blacklist
found later is therefore not established as the cause of that timeout. A
separate later two-app test did decode with the legacy backend in both decode
model-initializer placements, but those placements did not compare the old and
new startup ordering. Later timer-placement comparisons are documented below;
the earlier release-binary timeout itself remains unexplained.
The follow-up `bridge_startup_legacy_decode_order_ab.py` uses the same current
binary/source except for the previous/current legacy startup timer and disables
embedded rig control in both. Two isolated sessions per variant communicate via
BlackHole; both variants report an active legacy backend and display the FT8 CQ
(`startup-legacy-order-decode-ab.txt`). This matched test does not reproduce the
earlier release-binary timeout and does not force a delayed QML boot, so it
does not identify that timeout's cause or prove the delayed-order case under RF.

On 2026-11-10 `python3 tests/run_bridge_refactor_ab.py --base v1.0.737`
completed with exit code 0. The insertion-anchor inventory reported zero
potentially stale literal anchors and `git diff --check` passed. These checks
do not establish complete functional coverage of every moved method.

`source-method-diff.json`, generated by `bridge_source_method_diff.py`, is a
conservative navigation aid for comparing current method bodies with the two
Bridge source files in v1.0.737. It identifies exact textual body matches and
also classifies matches after replacing simple logging forwarders, four
explicitly checked ADIF/band/UTC helpers and simple return-only forwarding
functions or void forwarding calls with unchanged argument order; it groups the rest for review. The
normalisation is heuristic and does not resolve C++ overloads or name lookup.
These groups are not counts of regressions or
untested functions: source movement, forwarding helpers, overloads, comments,
macros and methods introduced after the release can all affect the result.
Textual equality does not establish runtime behavior or A/B execution. The
scanner's handling of ordinary quoted strings and comments was sanity-checked
with synthetic braces; it is not a complete C++ parser. An earlier index
incorrectly classified many SSTV methods as absent because C++ numeric
separators such as `1'000` were mistaken for character literals. The scanner
was corrected and the index regenerated; a synthetic separator check and four
known SSTV baseline definitions were checked afterwards. Earlier index counts
are superseded.

The same index now lists matching `*_before.inc` fixture candidates separately
from comparisons with the release. This helps locate worktree changes that
predate their individual extraction. For example, the constructor, `logQsoNow`
and `stopTx` match named saved fixtures, and their existing app A/B probes are
documented in the main report. A matching fixture is not, by itself, proof of
its provenance or that a test ran; the corresponding test and retained result
must still be inspected. In particular the stop-TX fixture includes work done
after v1.0.737 and should not be described as that release's implementation.

The index records a per-module overload ordinal and number of baseline name
candidates so identically named methods are not presented as one entry. For
example, the two `shouldDisplayEntryForBandActivity` overloads are separate:
one body is unchanged and the other matches after a telemetry-classifier
forwarder. The controlled decode-visibility A/B uses the real worked-filter
policy with substitute telemetry and ghost predicates; see the corresponding
extraction entry. The
ordinal is not a parsed C++ signature, and forwarding normalisation is not
proof of overload resolution or functional coverage.

Settings review: `bridge_load_settings_ab.py` compares the complete extracted
`loadSettings` body with its saved pre-extraction fixture, allowing nine named
forwarders and checking selected shared defaults. The retained application
results in `load_settings/load-{before,final}-{a,b}.txt` show matching callsign,
RX/TX frequencies, Hold TX and TX-off state for two synthetic profiles.
`bridge_save_settings_ab.py` compares the synchronous and asynchronous settings
methods with their pre-extraction fixture; `save-settings-restart-callsign-ab.txt`
records matching values across isolated app restarts, including the persisted
callsign. These are extraction comparisons against saved fixtures, not a full
comparison against the released version: the source index flags both methods as
different or ambiguous against v1.0.737. The retained runtime probes do not
exercise every setting, credential, connected CAT or external service. No test
was rerun solely for this evidence review.

`bridge_load_settings_audio_bounds_ab.py` supplements the existing startup
settings results with previously untested audio boundary values: negative and
above-maximum RX/TX levels and input/output channel indices. Both placements
loaded the expected clamped values in isolated application sessions, with CAT,
monitor and TX disabled; see `load-settings-audio-bounds-ab.txt`. These tests
do not exercise audio hardware or every setting.

SSTV RX worker-start failure: `bridge_sstv_rx_start_worker_failure_ab.py`
temporarily forces the worker-start failure branch in both the pre-extraction
fixture and extracted method. Both isolated app placements report the expected
error, clear request/ownership flags, leave the runtime inactive, and keep CAT
and TX off; results are in `sstv-rx-worker-start-failure-ab.txt`. The test
restores sources and rebuilds the app. This covers the injected worker failure
with monitoring initially off; it does not simulate a real audio-device failure
or a legacy-monitor restoration after failure.

The subsequent `bridge_sstv_rx_start_legacy_worker_failure_ab.py` repeats the
injected worker failure with the legacy monitor already active on BlackHole.
Both placements restore the legacy monitor and clear the SSTV ownership flags;
see `sstv-rx-legacy-worker-failure-ab.txt`. CAT and TX remain disabled; the
worker failure is injected, not caused by physical hardware.

CPU pressure and thread settings review: the source index flags
`effectiveFtThreadLimit`, `setFtThreads` and `cycleFtThreads` as ambiguous
against the release. The existing source comparison accounts for extracted
helpers, and the focused harness checks pressure and cancellation, FT4 guards,
low-CPU limits and stall recovery (`cpupressure-harness-ab.txt`). Retained
application A/B results report normal/low limits of 6/2 and TX off in both
placements (`cpupressure-runtime-ab.txt`), while `cpuprofile-runtime-ab.txt`
records matching automatic/manual and limit behavior. The app probes cover
selected state transitions rather than actual CPU-pressure load or Windows GPU
recovery. This review uses the existing results without repeating them.

Courtesy signoff positive app A/B: `bridge_courtesy_success_runtime_ab.py`
seeds a synthetic recently worked partner and verifies the actual bridge
prepares TX5 without re-logging, then releases and clears the courtesy state.
Both the fixture and extracted method agree (`courtesy-success-runtime-ab.txt`).
Periodic dispatch is suppressed and CAT/TX remain off; this does not test an
on-air exchange. The first attempted message ended in 73 and was correctly
rejected by the existing signoff rule; the positive case uses a directed RR73.
For the pre-extraction fixture, the window constant uses its existing extracted
forwarder. Source and executable are restored after the test.

The earlier `bridge_courtesy_runtime_lab.py` used the now-extracted window
constant directly when rebuilding its saved fixture and no longer compiled.
It now uses the existing forwarding function; the repaired guard A/B passed
in both isolated app placements (`courtesy-guard-helper-repair-ab.txt`).
The insertion-anchor inventory now also scans Python tools ending in
`_runtime_lab.py` and flags missing courtesy-window forwarding in scripts that
reinsert that fixture. Synthetic stale, repaired and unrelated examples passed;
the current scan found no such missing forwarders. This remains a heuristic,
not a general compilation or behavioural test of the scripts.

TX precompute content A/B: `bridge_txprecompute_wave_hash_ab.py` extends the
existing FT2 precompute lab to hash the complete float waveform. Both saved
pre-extraction and current code generate identical SHA-256 values for all
120,960 samples and identical cache-hit results for `CQ 9H1SR JM75`; see
`txprecompute-wave-hash-ab.txt`. CAT and transmission are off. This proves the
tested FT2 waveform on this host, not every message, platform or RF output.

World-map evidence review: the retained A/B harness outputs for queue, reset,
replay and entry cover duplicates, filtering, bounded queues, TX deferral,
freshness, replay interruption and synthetic CQ/directed cases. Their scripts
compare release and extracted method bodies with explicit forwarder
normalisation. The four corresponding `worldmap*-runtime.txt` files each
contain one unlabeled app result, even though the historical report describes
both placements. The replay probe source requires two K1ABC contact signals
after disconnect/reconnect before producing that result. The saved runtime
files therefore prove one successful app observation each, but do not by
themselves independently establish two separate before/after app runs. Do not
count those four files alone as complete app-level A/B evidence; the focused
two-version harness comparisons remain separate evidence.
On 2026-11-10 `bridge_worldmapreplay_app_ab.py` attempted a new labelled
comparison using the release body in the current module. The release-side app
decoded the WAV but the expected map replay signal did not arrive before the
probe timed out; the current side was not run. The attempt is inconclusive
and retained as `worldmapreplay-release-attempt-unresolved.txt`. The test
restored the original module and rebuilt the app. This does not contradict the
focused A/B harness result, but does not establish application replay parity.
The subsequent `bridge_worldmapreplay_synthetic_app_ab.py` isolates replay from
WAV decoding by injecting a fresh synthetic `CQ K1ABC FN42` row into the real
app and waiting for its map-contact signal. Both release and current method
bodies passed (`worldmapreplay-synthetic-app-ab.txt`), with CAT and TX off.
The inherited result labels `band`, `before` and `rx` are fixed success markers,
not independent list assertions. The original audio-to-map timeout is analysed below.
The follow-up WAV diagnostic recorded zero map contacts and empty decode and
RX lists at timeout (`worldmapreplay-wav-diagnostic-unresolved.txt`), although
the FT8 worker reported `nout=1`. That narrows the observed failure to before
an entry appeared in those lists; the metric alone does not establish which
message was decoded or why it was not published. It is not evidence that the
map replay itself discarded a populated entry. The release-side attempt did
not reach the current-side run; no A/B conclusion follows from it.
Further callback instrumentation identified the FT8 row as `CQ K1ABC FN42`
and counted one rejection by the user-filter path, with no parse, semantic or
duplicate rejection (`worldmapreplay-wav-filter-diagnostic.txt`). The precise
filter rule was not identified. With filters bypassed only in the isolated lab
profile, `bridge_worldmapreplay_wav_unfiltered_ab.py` decoded the WAV, observed
the contact, disconnected/reconnected the consumer and observed the contact a
second time in both labelled release/current method builds; see
`worldmapreplay-wav-unfiltered-ab.txt`. The inherited output's `band`, `before`
and `rx` fields are fixed success markers; the two contact events are the
actual probe condition. CAT and TX were off. This does not establish which
filter setting caused the earlier rejection or visual pixel rendering.
Subsequent temporary callback instrumentation identified the rejection reason
as `blacklist` in this lab run (`worldmapreplay-wav-blacklist-diagnostic.txt`).
It did not identify the matching term or settings source, and does not imply
that the user's normal profile has that setting. The test restored the source
and rebuilt the normal executable.
The next diagnostic established that `legacyIniPath()` in Qt test mode uses
the shared `~/.qttest/Library/Preferences/decodium4.ini`, outside the test's
temporary `DECODIUM_LAB_SETTINGS_DIR` (`worldmapreplay-wav-filter-location.txt`).
`bridge_worldmapreplay_wav_isolated_ab.py` temporarily redirects this Bridge
lookup to the test directory and restores the source afterwards. Both release
and current method bodies then pass the WAV-to-replay probe with normal filters
enabled (`worldmapreplay-wav-isolated-ab.txt`). This isolates this particular
lab only: other tests may still share Qt's legacy test INI, and this result does
not establish the matching blacklist term or the backend's separate paths.
Read-only inspection of the shared Qt test INI found blacklist enabled and the
synthetic `K1ABC` test call listed in the General, Configuration and Default
groups. `bridge_filtersettings_runtime_lab.py` sets those same synthetic values
and `setSetting` synchronises legacy keys, so the earlier filter lab is a
plausible source of cross-test contamination; its provenance is not proven by
the file alone. On this Mac, changing `HOME` in the subprocess environment did
not redirect Qt's ConfigLocation in a standalone Qt probe, so it is not a safe
general isolation fix. The shared file was not edited or deleted.

Decode filter evidence: `bridge_filtersettings_ab.py` compares the saved
pre-extraction filter snapshot with the moved implementation using temporary
settings and exercises legacy/profile precedence, aliases and bypass. The
separate `bridge_userfilters_ab.py` compares release/current evaluation with
production token and callsign rules. The existing app test verifies blacklist
and bypass using a synthetic WAV, but its use of shared legacy test settings
means its recorded environment is not fully isolated. The later world-map WAV
comparison isolates the Bridge legacy lookup and demonstrates a normal-filter
case. Neither result alone proves all user-filter combinations in the app.

Decode row parser review: the source index lists `parseFt8Row` and
`parseWsprRow` as ambiguous against the release. The full A/B runner extracts
the release parser section, normalises the UTC helper forwarder in the current
section, compiles each with `bridge_refactor_decode_rows_harness.cpp` and
compares output. The retained `legacymirror-full-ab.txt` lists valid and
invalid FT8/WSPR/JT65 cases, WSPR invalid power/callsign and frequency
fallback. Separately, `decoderows-ft8-refactor.txt` records a decoded FT8 CQ
from two isolated apps over virtual audio; the WAV-to-map replay A/B above
also exercised FT8 parsing in both app builds. The forced-native WSPR and JT65
recorded-WAV comparisons are documented separately; none proves RF or every
format variant. Existing evidence was reviewed without rerunning the suite.

Automatic RX gain review: `updateAutoRxInputLevel` is ambiguous in the generic
index because shared gain/logging helpers moved. The full runner compares its
release and current bodies after explicit helper normalisation and checks the
gain conversion and limit; `bridge_autorx_ab.py` executes both with deterministic
levels/times and a substitute setter. `autorx-runtime.txt` records two-app
BlackHole observations for both original and refactored versions, including
active virtual sender, enabled automatic gain and reduced receiver level; it is
a retained summary rather than raw per-session logs. These checks cover the
described virtual path, not physical input gain or every adaptive condition.

Audio device cache review: the generic source index flags
`refreshAudioDeviceCache` because its device, Pulse and logging helpers moved.
`bridge_audiocacherefresh_ab.py` explicitly normalises those forwards, checks
the release and current bodies, and executes both in normal and simulated-Linux
harness cases. Retained `audiocacherefresh-ab.txt` covers first enumeration,
unchanged devices, changed devices with signals suppressed and cache reuse.
The independent `bridge_audio_device_init_ab.py` app probe covers synthetic
device notifications and refresh scheduling; the recorded legacy-RX BlackHole
pair test exercises an integrated virtual audio path. Neither proves real
hardware hot-plug or a Linux system integration run. Existing evidence was
reviewed without repeating the tests.

Automatic log snapshot and preview review: `bridge_autologsnapshot_ab.py`
extracts the release methods including `armLateAutoLogSnapshot`, checks the
current methods after normalising the partner, grace-window and logging
forwarders, and executes both against substitute collaborators. The retained
`autologsnapshot-app-followup.txt` summarises original/refactored virtual TX5
prompt and rejection, including preservation of the captured call after the
current call changes. `bridge_logpreview_snapshot_runtime_ab.py` separately
checks snapshot, pending and live preview branches in isolated applications,
as reported in the main evidence document. These checks do not demonstrate a
completed two-way RF QSO or live prompt acceptance. The runtime follow-up is a
summary rather than raw per-session logs; no existing test was repeated.

Secondary decode queues: `bridge_secondary_ab.py` compares the complete
release and extracted legacy/native queue sections after normalising three
forwarders, and executes both with real Qt event processing and substituted
side-effect collaborators. Retained output covers ordering, duplicate
suppression, mode filtering, deferred drain and routing. The recorded app probe
also decoded an FT8 WAV and observed K1ABC in the actual active-stations model
in both sessions; it used the native path with CAT and monitoring off. The
legacy queue's external side effects remain harness-simulated. This review
uses the existing results and does not repeat the probe.
The runtime probe was subsequently repaired to use a unique removable test
profile, a loopback-only network sandbox and explicit filter bypass, addressing
the discovered shared test blacklist interference. The new labelled
`bridge_secondary_runtime_ab.py` comparison passed with the release/current
queue bodies: both decoded the WAV and populated the actual station model
(`secondary-isolated-app-ab.txt`). This covers the native queue path, not the
legacy queue's external-service side effects.

AutoCQ history and MAM pruning review: focused A/B harnesses compare the
release/current history and pre-extraction/current prune methods, including
cooldown, abandoned/worked state and slot expiry. The isolated app probes
exercise worked/abandoned cache operations and, separately, retry-exhausted,
expired and live MAM slots plus recovery/cooldown signals in both placements.
These are synthetic state transitions with CAT and TX off, not live QSOs.
See the AutoCQ history, MAM pruning and MAM expiry sections of the main report;
no existing tests were rerun for this review.

TX start and completion review: the main report records reconstruction of the
ordered preflight and selected-message guards, twenty preflight scenarios and
FT8 virtual-audio A/B between two isolated apps. Completion evidence covers
FT8 and FT2-Link virtual audio, pending PCM and the intentional CW completion
fix; the latter is a behavior correction rather than an identical source move.
HALT comparisons and captured silence for CW, FT2-Link and telemetry are
listed above. These observations cover the described modes and conditions,
not all `startTx` branches or physical PTT/RF. No old test was repeated for
this review.

Station frequency review: `bridge_stationfreq_restart_ab.py` compares the
v1.0.737 and current station methods in isolated applications across add,
restart, edit, restart, delete and final restart. The main evidence report
records both versions passing and notes the redirected temporary legacy INI.
`stationfreq-real-ab.txt` separately retains the real Qt model/QSettings
harness comparison. No separate raw log for the five app launches was located
in the evidence directory during this review, so the historical report rather
than a per-run log is the available app result. CAT tuning and physical radio
are not covered. Existing tests were not repeated.

Window and UI preferences review: the full A/B runner executes synchronous
and asynchronous window-state save/load/reset against temporary QSettings.
The UI preference app results include isolated property changes and restart
checks for quality, auto-detach, frameless windows and FPS. Historical style
experiments used native macOS preferences that were not fully isolated; their
side effect and unknown prior value are documented in the main report and are
not treated as proof that the user's style was restored. These checks do not
establish actual multi-monitor placement or rendered appearance. Existing
tests were not repeated.

SSTV controls and transmit review: the existing full runner compares
`sstvRxControls` against the release, and app A/B probes cover notch, FAX480
and Radiofax control settings. Separate before/after virtual-audio tests in
the main report cover Dayton FAX480 complete-image transfer, centred Radiofax
IOC 576 at 240 LPM and a 225-byte HAMDRM transfer. Calibration has independent
BlackHole capture near 1200 Hz in both placements. The referenced probes use
isolated apps with CAT disabled. They establish the described configurations,
not every SSTV profile, physical RF, or complete EasyPal interoperability;
the latter was explicitly outside the later continuation of this goal. No
passing test was repeated for this review.

Remote activity publication: the three moved cache/publication methods had no
focused A/B reference located in the report. `bridge_remote_activity_ab.py`
now compares their v1.0.737 bodies with the current bodies after normalising
the time helper forwarder, then executes each with a socket-free server
substitute. It verifies absent/stopped server guards, RX/TX defaults, duplicate
suppression, mode keys, the 4096-entry cap and local/remote clearing; see
`remote-activity-harness-ab.txt`. The time normalizer and remote server are
test substitutes. A real localhost server/client publication comparison remains
to be checked; this harness does not prove networking.
The follow-up `bridge_remote_activity_ws_ab.py` runs both release and current
activity bodies in the app with the real remote server and an authenticated
localhost WebSocket client. Both emit one activity event after two identical
publishes and one reset event; results are in `remote-activity-websocket-ab.txt`.
Credentials and row are synthetic, network is sandboxed to localhost, and CAT
and TX are off. The test substitutes only the credential lookup and restores
the sources and executable; it does not establish external network behaviour.

Legacy decode review: the release/current A/B harnesses explicitly cover
`appendLegacyAllTxtDecodeLine`, `augmentLegacyMirrorWithAllTxt`,
`resolveFt8HashPlaceholdersFromRecentAllTxt` and
`syncLegacyBackendDialogState`, normalising their extracted helpers. The hash
bridge retained results include match, wrong-mode, no-match and ambiguity;
the augmentation app comparison primes and appends a temporary ALL.TXT and
rejects duplicate entries in both placements. The dialog harness uses a
recording substitute, while separate legacy BlackHole sessions exercise RX.
Full-app hashed-call recovery and every legacy dialog operation were not
established by these checks. The main report also retains an unresolved
pre-existing placeholder case; ordinary CQ success does not prove that case.
No passing test was rerun for this evidence review.

The subsequent `bridge_legacyhash_app_ab.py` invokes the release and current
hash resolver inside isolated applications using a synthetic ALL.TXT. Both
recover `K1ABC K2XYZ RR73` and leave the placeholder unchanged in FT4; results
are in `legacyhash-app-ab.txt`. Candidate-path discovery is temporarily
restricted to that one synthetic file to prevent reading the operator's logs;
source files and executable are restored. CAT and TX are disabled. This tests
the real resolver with a synthetic placeholder, not a live hashed RF decode or
every ambiguity branch.

External URL review: the full runner extracts `openExternalUrl` from the
v1.0.737 Bridge and the current module, checks the bodies for equality, then
compiles and executes each with the same desktop-service substitute. Its
retained `legacymirror-full-ab.txt` result covers HTTPS, mailto, local files,
blocked schemes, empty input and opener failure; output is compared between
placements. The separate `external-url-{baseline,refactor}.txt` application
results confirm rejection of empty and blocked URLs. The callsign initializer
routes its lookup signal to this method; its documented application probe
checks the rejection path. These complementary checks do not demonstrate an
actual browser launch or the Linux AppImage-specific helper. No existing test
was rerun for this review.

`extracted-initializer-index.md` maps the 28 methods that the corrected scanner
does not find as standalone definitions in the two baseline files to documented
A/B scripts and report entries. These include constructor blocks extracted into
initializers and TX preflight/selection helpers. Script presence and report
references were checked; the mapping does not itself prove execution of each
callback or replace the individual scenario and limitation review.

For example, the changed ADIF import, TCP forwarding and two UDP forwarding
bodies differ from v1.0.737 only by logging forwarder calls, whose definitions
call the original logger. Their previously recorded A/B and local app evidence
is described in the ADIF import and UDP/TCP forwarding sections of
`tests/BRIDGE_REFACTOR_AB.md`; this is not a claim about external loggers.
The logbook-profile methods similarly match after the checked ADIF helper
forwarders are normalised. Existing evidence includes a focused A/B harness and
isolated application comparisons with the checksum-verified v1.0.737 release,
covering profile creation, switching, backup, deletion and restart.

For legacy backend ownership and TX timing, source differences primarily
reflect extracted helper forwarders. Existing A/B harnesses exercise ownership
selection, timing decisions and Qt timers; separate before/after FT8 and FT2
BlackHole sessions provide virtual-audio evidence. One real behavioral change
must remain visible: `scheduleSyncTxAtNextValidSlot` uses `Qt::PreciseTimer`
where v1.0.737 used the default timer, to avoid a retry firing before its slot.
The recorded timer and retry A/B checks passed after this correction, with CAT
disabled; they do not establish physical RF timing. The method remains listed
as different in the source index rather than being normalised away.

For the separately raised FT8 duration question, `txwave/real-modes-ctest.txt`
records 606,720 samples and a matching before/after waveform hash; the wave
generator is called at 48,000 samples/s, giving 12.640 s of generated audio.
This is not a measurement of the radio relay, PTT lead/tail or actual RF
duration. The retained result was inspected without repeating the test.

The current HALT and stop paths also intentionally differ from v1.0.737 for
bridge-owned CW, FT2-Link and telemetry under legacy TX ownership. In retained
before/fix app comparisons, CW and FT2-Link audio was present before HALT and
silent afterwards, and the corrected bridge left TX inactive; see
`halt/direct-fix-{cw,ft2}-ab.txt` and `halt/direct-fix-{cw,ft2}-audio.txt`.
The telemetry path also has an original/corrected app-state comparison in
`halt/direct-fix-telemetry-ab.txt`: the original remained TX-active after HALT,
whereas the corrected method stopped. That telemetry check did not capture audio.
The corrected version was then measured separately over BlackHole: nonzero
telemetry audio preceded HALT and the measured later window was silent; see
`halt/direct-fix-telemetry-audio.txt`. This does not measure RF or physical PTT.
Separately, natural CW completion clears the CW-active flag; the retained
`cw-natural-modulator-fix-ab.txt` compares the former and corrected behavior.
These deliberate corrections are not ordinary source moves, and virtual audio
does not establish physical PTT or RF behavior.

The audio-output level and CW frequency-control bodies match the original after
their simple helper calls are normalised. Their existing A/B tests cover audio
level math and application controls, and CW frequency admission with simulated
CAT collaborators. The CW tuning app test keeps CAT disabled; it does not prove
an actual radio command.

The discovery inventory refreshed after the latest test additions identifies
1,281 module/method-name pairs across 410 modules and 25 pairs with no literal
mention in scanned test sources. These 25 are mapped to related evidence in
`method-inventory-review.md`. Neither
figure is a count of untested functions: property calls may invoke setters
indirectly, repeated names and overloads may be conflated, and a text mention
does not establish execution. Review must map the original and extracted
implementation, test scenario and recorded runtime result for each function.
For example, the inventory misses `setUiStyle` by name while the explicit
property/restart A/B evidence is documented in `tests/BRIDGE_REFACTOR_AB.md`.

| Requirement | Current evidence | Status / next work |
| --- | --- | --- |
| Smaller Bridge files | Main `DecodiumBridge.cpp`: 700 lines versus 62,908 lines in v1.0.737; 410 split `DecodiumBridge*.cpp` files. Both local app targets compiled the modules. | The original monolith has been split; size alone does not establish behavioral equivalence. |
| Preserve behavior | Full `tests/run_bridge_refactor_ab.py` passed on 2026-11-09; it includes source comparisons and focused harnesses. Many per-feature real-app comparisons are documented in `tests/BRIDGE_REFACTOR_AB.md`. | Not established for every touched function. Map each method to the exact comparison and execution evidence; mentions alone are insufficient. |
| Functional proof | Real-app scenarios include ADIF import/export, settings restart, local PSK XML, SSTV gallery and virtual audio, and FT8 two-app decoding. | Scope varies by feature. Review whether each moved function is reached; add app-level probes where only static comparison or an unrelated path exists. |
| Test repeatability | The literal-anchor check reports zero candidates after 14 scripts were repaired and rerun. Several SSTV scripts needed further helper declarations and were also repaired. All 944 Python test tools parsed; all 801 script names referenced by the evidence document existed when checked. | Static checks do not guarantee all scripts run; stale dependencies were found despite them. Continue targeted reruns based on feature and risk. |
| SSTV Robot 12 local loopback | Both placements passed a complete image with simultaneous capture; both previously ended with incomplete images without capture. A separate captured run had a high last-column error despite passing global image thresholds. A synthetic PCM diagnostic reproduces edge error with and without the test-only disabled band-pass; see `robot12-bandpass-comparison.txt`. | The no-capture failures and edge error remain unexplained. Avoid claiming equivalence for that condition solely from the passing captured run. |
| Portable implementation | Mac QML and legacy targets compiled after guarding an RTTY-only include; a focused disabled-RTTY syntax check passed. | Preserve platform guards and inspect changed code; full Windows/Linux execution tests are not part of this goal. |

The `startSstvTx` body was checked against the saved immediate pre-extraction
fixture and still matches after normalising its output-device helper forwarder.
Its two-session Robot C12 comparison has both a passing captured run and
incomplete-image runs, including a before-only failure in a later captured
comparison. The latter are retained in `sstv-profile-cleanup-ab.txt`; passing
one run does not establish reliability or explain those failures.

The retained full A/B results also cover CAT profile operations, keyboard audio,
window-state persistence and SSTV timing; `method-inventory-review.md` maps
their actual harness scope and limits. The CAT profile section was additionally
compared in isolated application sessions after the source-placement split.
These mappings reuse existing results and do not imply complete coverage of
the other modules.

Module-reference review: 211 of the 410 Bridge source filenames appear in the
general A/B runner; the remaining 199 require other evidence. A literal search
of the Python tools finds 197 of those names. The two remaining modules,
`DecodiumBridgeTxTimingMath.cpp` and `DecodiumBridgeTxPcm.cpp`, are compiled
directly into dedicated A/B CTest targets in `tests/CMakeLists.txt`. Retained
results cover generated waveform comparisons and 192 PCM conversion cases,
respectively; separate FT2 app/BlackHole evidence is documented in the main
report. Filename references and target declarations alone do not establish
that every method or branch in all 410 modules was exercised.

The directory-selection probe exercises cancellation and a Unicode path in
both application placements using Qt's non-native dialog. The current
`openDirectoryDialog` and `localDialogPath` bodies also match v1.0.737 exactly.
The native OS dialog was not exercised; that is a test limitation rather than
evidence of a changed implementation.
The corresponding file open/save methods now also have isolated app A/B
evidence for cancellation and selected paths, recorded in
`file-dialog-runtime-ab.txt`; the save dialog was tested with Qt's non-native
implementation.

LoTW cache migration has application A/B evidence using a synthetic legacy
cache and redirected temporary paths: both placements migrated the bytes and
returned the expected call lookups (`lotw-migration-runtime-ab.txt`). Local
cache loading, successful HTTP, empty response and network-error cases also
have app A/B tests; external HTTPS and actual user data are outside those
proofs. These examples are not an exhaustive list or a count of missing tests.
Do not infer that every unlisted method is covered.
Target-call synthetic decoded-row transition and a real 10-second timer now
have app A/B evidence; actual decoded audio and other timeout branches remain
outside those particular tests.

On 2026-11-09 the local QML and legacy build targets were rebuilt successfully
after the latest probes, and `git diff --check` reported no whitespace errors.
Approximately 9.1 GiB remained free before this build; Qt test application
support data occupied about 10 GiB. Existing test profiles were not deleted
during this audit because their provenance was not established.

Later test-space maintenance: after confirming no Decodium/test processes
were running, 1,035 duplicate `grid_data.bin` and 1,054 duplicate
`state_data.bin` files older than 24 hours were removed only from named Qt test
profiles. Each removed file was SHA-256 checked against its retained reference;
recent files, nonmatching hashes and the rest of each profile were preserved.
Free disk rose from 3.7 to 8.2 GiB and the IU8LMC Qt test application-support
tree fell from 11 to 6.1 GiB. These files are downloaded US-state data caches,
not the recorded test evidence in this repository. This cleanup does not
establish the provenance or disposability of other profile data.

The legacy startup virtual-audio probe with a four-second artificial QML stall
on 2026-11-10 was inconclusive as an ordering comparison. The previous-timer
variant reported a decoded model entry and reached the ordering assertion,
but the observed Main.qml marker preceded the legacy configuration marker,
contrary to the fixture's expectation. The assertion stopped the run before
its current-timer variant; no conclusion about the two orderings follows from
this run. The traceback is retained in
`startup-legacy-stall-decode-inconclusive.txt`. The source and build QML
fixtures were restored and the application rebuilt by the script's finally
block. Earlier successful A/B tests remain separate evidence.

On retry, the QML stall fixture was installed after each build and its start
and end markers were required in the RX log. Both variants reported a decoded
entry with legacy backend active and TX false; the previous-timer variant had
legacy configuration before Main.qml. The current-timer variant also showed
legacy configuration before Main.qml, so the test's ordering assertion failed
and this remains an inconclusive startup-order comparison. The result does not
establish a production regression: the lab starts monitoring during startup,
and the application has other paths that can initialise the legacy backend;
the initiating path was not determined. The full output is retained in
`startup-legacy-stall-decode-retry.txt`. The script restored its source and
QML fixtures and rebuilt the app on exit.

A focused current-timer retry moved the lab RX monitor start from 2.5 to 10
seconds while retaining the verified four-second QML stall. It observed the
Main.qml marker before legacy configuration and decoded the virtual-audio FT8
message with the legacy backend active and TX false; see
`startup-legacy-stall-current-late-monitor.txt`. This is consistent with
monitor startup triggering early legacy creation in the prior run: `startRx()`
can call `ensureLegacyBackendAvailable()` when legacy TX is requested. The
calling path was not instrumented, so causation is not proven. The successful
previous-timer run used a 2.5-second monitor delay, therefore these separate
results are not a matched A/B of startup ordering with live monitoring. Both
fixtures were restored and the application rebuilt after the focused retry.

The previous timer was subsequently exercised with the same four-second QML
stall, 10-second RX-monitor delay, synthetic message and virtual audio devices
as the focused current-timer run. Both decoded the message with legacy active
and TX false; legacy configuration preceded Main.qml with the previous timer,
whereas Main.qml preceded legacy configuration with the current timer. The
logs are `startup-legacy-stall-previous-late-monitor.txt` and
`startup-legacy-stall-current-late-monitor.txt`. This compares the release
startup timer and current timer inside the current application build, not the
entire release binary; it does not resolve the earlier release-binary decode
timeouts or prove operation with physical CAT/RF. Fixtures were restored and
the current app rebuilt after each test.

Related earlier evidence is retained in `legacydelta-pair-ab.txt`: the
legacy-delta report identifies an original-app virtual-audio attempt that
first timed out and then decoded on retry, alongside a successful refactored
app run. That retained result corroborates that reception was possible in both
app variants under that lab setup; its short output does not record the binary
paths or explain any timeout. It must not be used to claim that the two later
startup-specific release-binary timeouts were resolved or to attribute their
cause. The detailed failed logs report decoder `nout=0`, which alone does not
identify whether audio capture, timing, level, configuration or decoding was
responsible.

Constructor test maintenance: after the constructor itself moved to
`DecodiumBridgeConstructor.cpp`, 53 tests still selected the old Bridge file
while explicitly replacing or locating initializer calls. Their constructor
path was updated. The TX-audio and legacy-filter static ordering checks pass;
the core-timers app A/B passed both placements, timer firing and isolated
settings readback after adding two declarations needed when the old block is
compiled in the new translation unit (`core-timers-constructor-path-ab.txt`).
The other updated runtime scripts have not all been executed after this path
change and may require their own translation-unit includes or declarations;
syntax parsing is not sufficient evidence that they run. The previous literal
anchor inventory missed this issue.

`bridge_tx_audio_startup_runtime_ab.py` also used a direct literal constructor
anchor missed by the initial inventory. Its path and test-only includes were
repaired. Both app variants passed object creation, synthetic audio-error
propagation and an injected modulator-idle hold, with CAT and TX off; see
`tx-audio-startup-constructor-path-ab.txt`. The injection is not an observed
physical or virtual transmission callback. Two other scripts were found using
the same direct literal pattern against the former constructor file:
`bridge_tx_audio_startup_loopback_ab.py` and
`bridge_legacy_filter_connections_runtime_ab.py`. Their prior retained results
are separate; their current script entry points remain stale and are not
claimed runnable.

The TX audio startup loopback script was then repaired for the constructor
location and test-only headers, and rerun successfully in both placements.
Both isolated pairs decoded the synthetic FT8 CQ with matching peak and 127
active audio windows, and passed the timing check; CAT was disabled. New
results are in `tx_audio_startup_constructor/`, preserving the earlier files.
The modulator-idle callback was not observed in either loopback; the separately
injected callback result remains the evidence for that path. The legacy filter
runtime script identified above still requires its path repair.

The legacy-filter runtime script's constructor path was repaired and its
embedded legacy rig control was forcibly disabled during the test. Both app
placements passed synthetic CQ, CQ-level and MyCall filter notifications with
the legacy backend active and CAT/TX false. The source was restored and the
application rebuilt; output is `legacy-filter-constructor-path-ab.txt`. This
resolves the two specifically identified direct-literal stale paths; it is not
an assertion that every other lab script has been revalidated.

CAT recovery evidence review: `catrecovery/ab.txt` records the A/B guard and
reconnect simulation; `catrecovery/runtime.txt` records matching app results
for TX and Tune guards with CAT and TX false. The existing report separately
describes a successful local dummy-rigctld initializer A/B; its detailed
per-run output was not located in this evidence review, so the report alone
is not treated as a newly verified binary log. Neither establishes operation
with the connected physical radio. No test was rerun during this review.

Six other CAT connection A/B scripts insert the same pre-extraction fixture
into the constructor. Their before-variant setup now supplies the same module
dependencies as the successfully repaired dummy-rigctld script. Python syntax
checks pass; these six scripts were not rerun, so their individual compiled
instrumentation and runtime results are not newly verified. Prior recorded
results are described in `tests/BRIDGE_REFACTOR_AB.md`.

CAT consolidated evidence: `cat-rigctl-constructor-ab.txt` records matching
connection, frequency and disconnect results against a localhost dummy rig,
with PTT/TX inactive. `cat-frequency-constructor-ab.txt` records matching
frequency/band/routing results with a fake manager. `catrecovery/runtime.txt`
records matching TX and Tune guard results. Prior app scenarios for error,
mode, split and synthetic PTT feedback are described in the CAT connection
section of `tests/BRIDGE_REFACTOR_AB.md`; their five updated script entry
points were not individually rerun after the constructor-path repair. These
results are software/virtual tests and do not demonstrate physical rig CAT.

The five remaining modified CAT connection scripts (base, pending PTT
confirmation, connection, mode/split and PTT feedback) subsequently completed
both application placements successfully with isolated profiles, simulated
managers, CAT auto-connect disabled and TX off. Their retained outputs are in
`cat_connections_constructor/`. These tests demonstrate software routing;
they do not demonstrate physical CAT/PTT.

Audio-device initializer review: the current constructor contains the single
initializer call and both Qt headers directly needed by the saved pre-extraction
block; that saved block remains identical to the extracted method body. The
previously retained app/BlackHole evidence is described above. This is a
source/dependency review, not a new build or execution of the scripts after
their path edit.

File-size review: the three largest split Bridge files currently contain
`startTx`, `autoSequenceStep` and `onFt8DecodeReady` respectively, at 1,418,
1,339 and 1,262 lines including includes and helpers. The original Bridge
file was 62,908 lines. These are substantially smaller but each still contains
one large behavioral method; further subdivision would require changing those
method bodies and would need its own A/B validation. This observation is not
an assertion that the existing functional coverage is complete.

Largest-method evidence review: retained `start_tx/before.json` and
`start_tx/after.json` both show the start-TX path reached and the synthetic
FT8 CQ decoded over virtual audio, with matched peak and active-window count;
the report also describes guard A/B for 20 synthetic native/legacy cases.
The auto-sequencer report describes matching directed FT8/FT4/FT2 message
transitions, but only selected branches. The FT8-ready report describes PCM
fixture comparisons and stale-session/mode guards, not all callback branches.
These are scoped proofs, not a complete branch audit or a physical RF test;
no test was rerun in this review.

The historical full-run console output for 2026-11-10 was found at
`/tmp/bridge-full-ab-20261110.log` and preserved as `full-ab-20261110.log`.
It starts with initializer source-equivalence checks and reaches the final FT2
access cases, with no traceback or compiler error found in the retained text.
The text itself does not contain a shell exit-code record; the earlier audit
reported exit code 0. The log predates later test-script maintenance and is
not evidence that every individual later-edited lab is runnable. The full run
was not repeated.

Portability source review: a search of `DecodiumBridge*.cpp` and matching
headers found no hard-coded `/Users/salvo`, `/opt/homebrew` or
`/private/var/folders` paths. The HAMDRM lab directory override in
`DecodiumBridgeSstv.cpp` is guarded by Qt test mode and validates absolute,
non-symlink paths. This limited search does not prove Linux/Windows compilation
or correct behavior on other systems. No platform execution was rerun.

Constructor-lab dependency review: a static comparison found 16 initializer
modules whose direct include lists contain headers not explicitly included by
`DecodiumBridgeConstructor.cpp`. This is only a candidate list: headers can be
transitively available, and not every header is used in the inserted fixture.
For example, the repaired WAV initializer A/B passed despite such an include
list difference. The successfully repaired core-timer, decode-model, CAT and
TX-audio probes demonstrate that some moved fixtures do require explicit
additional dependencies. The remaining changed labs cannot be declared
runnable solely from this check. No further app tests were run in this review.

The constructor's current macOS compiler dependency list was inspected against
the 16 initializer include candidates. For five modules all extra include
names were already represented in that dependency list; eleven had at least
one include not observed there. This is a conservative name/path comparison:
conditional headers and actual fixture use were not resolved, Qt forwarding
headers may differ from their lowercase implementation header, and the other
translation units are not compiled on this basis. Neither figure counts failing
scripts or establishes Linux/Windows compilation. No test was rerun.

A non-mutating macOS `-fsyntax-only` compile of the satellite initializer's
before-extraction fixture inside a temporary constructor copy exposed missing
`SatelliteTrackingService.h` and `Q65DopplerTracker.h`. Both existing satellite
initializer A/B scripts now inject those headers in their test-only before
variant. The same temporary fixture compiles successfully with the headers;
Python syntax also passes. This did not launch the app or rerun the prior A/B
functional tests, and does not validate the other initializer scripts or other
platforms.

Constructor test-script follow-up: 25 A/B scripts using ten initializer
fixtures were updated to provide the corresponding extracted-module
dependencies when reconstructing the before variant. The actual generated
before-constructor variant from each script passed macOS compiler
`-fsyntax-only`; see `constructor-script-fixtures-syntax.txt`. The diagnostic
connections script was also run end to end after the repair, with matching
before/after results, CAT and TX disabled; see
`diagnostic-connections-constructor-ab.txt`. Compile-only results do not
establish that the other 24 scripts execute successfully or expand earlier
functional coverage. The saved full-suite result predates these script edits.

The intentional startup differences were checked directly against the
retained A/B outputs: `startup-profile-growth-ab.txt` records a sibling
profile copied by the baseline but not by the revised implementation, while
`startup-order-ab.txt` records earlier versus deferred legacy creation and
matching readiness/fallback cases. These changes are not described as pure
code movement. Neither output establishes matched real-user startup timing
or all profile layouts; no existing test was rerun.

The separate `Main.qml` visual-staging change is also intentional. The saved
`startup-visual-ab.txt` compares seven guard conditions: only the idle,
non-monitoring case changes in that set. `startup-timing-ab.jsonl` records one
isolated startup per placement, with waterfall-ready timestamps of 7.917 s
and 4.219 s respectively. These are whole-process timestamps from distinct
test builds, not isolated measurements of the guard or evidence of the
operator's normal profile. The test does not establish visual rendering on all
platforms. The recorded A/B was inspected without repeating it.
