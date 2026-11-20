# Review of names absent from test-source token search

The token inventory is a discovery aid, not a count of untested functions. The following entries have related functional A/B evidence. This mapping does not establish complete branch coverage.

| Methods | Evidence | Scope and remaining limitations |
| --- | --- | --- |
| `setFoxMode`, `setHoundMode`, `setNcontest` | `specialop-wrappers-ten-ab.txt`; `tests/tools/bridge_specialop_wrappers_ab.py` | The current probe has ten transitions, including the three added contest-boundary inputs. Its new before/after results match and the lab asserts the expected activity values. CAT/TX remain off; there is no RF transition. The older `specialop-wrappers-ab.txt` records only the original seven transitions. |
| `setPttPendingState`, `setPttConfirmedState`, `confirmLegacyPttTransition`, `failLegacyPttTransition` | `legacyptttransition-coverage-recheck-ab.txt`; `tests/tools/bridge_legacyptttransition_runtime_lab.py`; `src/bridge/DecodiumBridgeLegacyPttTransition.cpp` | The retained app probe starts a transition and checks pending, waits for dispatch and feedback confirmation, and checks timeout and simulated audio failure. The inspected implementation sets pending/confirmed states in begin and confirmation, and invokes the failure helper on timeout and audio-start failure. Both A/B results show the corresponding checks true. Thus these four helpers are exercised indirectly without needing separate tests for each name; feedback and audio failure are synthetic and no rig or relay is tested. |
| `finishSstvWavReplay`, `restoreSstvLiveAfterReplay` | `sstv-replay-cancel-coverage-recheck-ab.txt`; `tests/tools/bridge_sstv_wav_replay_cancel_ab.py` | Completion and restoration with synthetic audio; rebind uses a marker. |
| `queueSstvRxImageSave` | `sstv-image-save-guards-ab.txt`, `sstv-image-reset-repeat-ab.txt`, `sstv-auto-image-repeat-ab.txt`, and `sstv-autosave-{baseline,refactor}-{a,b}.txt` | Actual app A/B tests exercise storage-unavailable guards, manual image saving and duplicate suppression, automatic save after stopping reception, and raw-audio association. Two-app virtual-audio Robot C12 runs also verify received PNG and gallery records in the release and refactored apps. Earlier baseline receptions were incomplete and one passing run had a high last-column error; neither is proof of every database failure, image race or RF case. |
| `mamBuildSlotMessage` | `mamstate/ft8-before.json`, `mamstate/ft8-after.json`, `mamstate/ft4-before.json`, `mamstate/ft4-after.json`; `tests/tools/bridge_mamstate_modes_runtime_lab.py`; `src/bridge/DecodiumBridgeMamDispatch.cpp` | The retained FT8 and FT4 before/after reports each contain the same 1500 Hz `W1AW 9H1SR JM75` stream. The app lab reads `mamTxStreams` during virtual TX, and dispatch constructs that stream's message by calling the builder. This is indirect evidence for that message path, not an instrumented call count or proof of every QSO slot and report option. |


| `launchTuneAudio` | `tune-legacy-repeat-ab.txt`; `tests/tools/bridge_tune_legacy_audio_ab.py` | Matching BlackHole audio and silence after stop; legacy TX ownership selected and CAT disabled. Not a physical PTT test. |
| `saveSettingsInternal` | `save-settings-restart-ab.txt`; `tests/tools/bridge_save_settings_restart_ab.py` | Settings persisted across isolated app restarts; not every field or shutdown race is covered. |
| `saveSstvRxRawAudioImpl` | `sstv-raw-audio-repeat-ab.txt`; `tests/tools/bridge_sstv_raw_audio_ab.py` | Empty buffer guard and synthetic WAV export to an actual RIFF file; not active-export cancellation. |
| `handleSstvStudioPreparedChanged` | `sstv-studio-archive-ab.txt`; `tests/tools/bridge_sstv_studio_archive_ab.py` | `DecodiumBridgeSstv.cpp` connects the studio signal to this slot. Both retained A/B results report generation, draft, archive and duplicate handling. This signal-driven synthetic-image test is not an on-air transmission. |
| `initialiseSstvTx`, `releaseSstvTxBridgeOwnership` | `tests/tools/bridge_sstv_tx_init_capture_ab.py`, `tests/tools/bridge_sstv_tx_lifecycle_loopback_ab.py`; see TX sections in `tests/BRIDGE_REFACTOR_AB.md` | Coordinator and virtual calibration / image TX exercise startup and ownership release. No hardware PTT or device failure. |
| `activateHamDrmRxTap`, `deactivateHamDrmRxTap`, `disconnectSstvProducerTaps`, `disableSstvProducerTaps` | `hamdrm-taps-repeat-ab.txt`, `radiofax-taps-repeat-ab.txt` and corresponding producer-tap loopback scripts | Retained results show exact 225-byte HAMDRM recovery and complete Radiofax reception before and after, via virtual audio. These are integration evidence for the relevant routes and shutdown, not an instrumented call count for each helper or proof of every teardown branch. |
| `setAutoDetachFullSpectrum`, `setSpectrumFpsCap`, `setUiFramelessPopouts`, `setUiStyle` | UI preferences A/B and restart evidence described in `tests/BRIDGE_REFACTOR_AB.md` | Property changes and restart behavior were compared, but actual visual rendering was not. Earlier style tests affected a native preference; its former value is unknown and was not guessed or restored. |
| `setAdvAutoModeEnabled` | `tests/tools/bridge_advanced_decode_runtime_lab.py`; `advanced-boundary-{baseline,refactor}-{a,b}.txt` | The QML probe writes `advAutoModeEnabled` false and true, including a repeated true, and checks the resulting effective controls and history boundaries. Retained baseline/refactor results for both variants report passing; this is a property-driven application check, not live RF decoding. |

On 2026-11-10 the current token inventory listed 25 method names absent from test sources. All 25 are named in the mapping above, and the explicitly referenced test scripts exist. This cross-check does not independently establish that each indirect call ran, that every placement was compared, or that all branches are covered. A function name appearing in a test does not itself prove the function ran.

## Additional review of low-reference methods

These names did appear in the broader test index but had no direct name match
in a Python tool. The following evidence was inspected without rerunning it.

The new `bridge_audio_buffers_runtime_ab.py` invokes the release and current
buffer methods in isolated apps: a synthetic active-TX flag preserves the
buffers and schedules a retry, then an idle call clears them. Both method
versions passed (`audio-buffers-runtime-ab.txt`). CAT is disconnected; TX state
is simulated and no device or physical transmission is started. The existing
two-app BlackHole continuity result remains separate evidence.

The methods `openDiagnosticLog` and `openHamlibUpdatePage` have a single
literal test-source reference, but `bridge_appinfo_ab.py` extracts their
release/current bodies and compiles both with the shared app-info harness. The
retained `appinfo-ab.txt` checks the expected log path and Hamlib URL through a
desktop-service substitute; the separate app probe reads the diagnostic path,
version and screen geometry. Neither opens a real browser or file manager.

Binary `readFileBytes` and `writeFileBytes` already had release/current
temporary-file A/B coverage in the full runner. The additional
`bridge_fileio_runtime_ab.py` calls both through the actual QML Bridge in
isolated release/current app sessions, verifies exact bytes on disk, bounded
read/truncation and rejection of an empty destination. Both passed with CAT
and TX off; see `fileio-runtime-ab.txt`. Files are confined to temporary
directories; the test does not exercise every filesystem error.

`teardownAudioCapture` is named only in its C++ harness. The A/B driver
`bridge_audiostop_ab.py` compares the release/current sections and executes
same-thread and worker-thread teardown with Qt dispatch and substitute audio
objects. The separate two-app BlackHole test observes stop/restart capture
continuity; it does not independently establish every teardown branch or
physical-device behaviour. Existing results were inspected without a rerun.

`openFt2LinkReceivedFilesDirectoryAsync` is named only in the received-file
harness. The full runner compares the release/current received-file methods,
and the harness exercises asynchronous completion with a substituted desktop
opener. Separate before/after app probes save synthetic text and binary files
to temporary directories and verify their bytes. These do not establish that a
native file-manager window opened, or that the folder-opening path ran in the
app probe. The existing comparisons were not repeated.

`loadWorkedBeforeHistoryAsync` has one literal test-source reference, in
`bridge_refactor_asynchistory_harness.cpp`. The A/B driver extracts both the
release and moved loader, and the harness executes actual Qt thread-pool jobs
with temporary files and substituted parsers, including a profile path change
and a file change while work is in flight. The separate startup app probe
observed the existing shared Qt test dataset without altering it; it did not
establish parser correctness or complete isolation. The worked-history app
probe separately imported, queried and removed a synthetic ADIF record. These
tests cover distinct portions of the path; none proves every asynchronous race.

The low-reference CW remote and rig methods are exercised by the retained
release/current harnesses `bridge_cwremote_ab.py` and `bridge_cwrig_ab.py`.
They cover remote key/PTT forwarding, busy guards, simulated CAT Morse and PTT
release. Separate application A/B sidetone tests capture virtual BlackHole
audio, and the local synthetic DecoLink relay tests the protocol separately.
These are complementary checks, not a complete remote-radio or physical PTT
test. The existing results were inspected without rerunning them.

The atomic decode-ready and model-emission timing accessors appear by name
only in `bridge_refactor_metrics_harness.cpp`. The full runner extracts the
release/current implementations and compares both; retained
`legacymirror-full-ab.txt` records unmarked, independent, elapsed and reset
cases for all four timestamps. The separate overlay A/B and application probe
exercise rates and timer controls with synthetic frames. They do not establish
GPU timing accuracy. Existing tests were not rerun for this review.

| Methods | Evidence inspected | Scope |
| --- | --- | --- |
| `onAlcCalibrationTick`, `finishAlcCalibration` | `bridge_alc_calibration_ab.py`, `tests/bridge_refactor_alc_harness.cpp`, `alc_calibration/alc-{before,after}-{a,b}.txt` | The A/B harness runs both method bodies and calls the tick through convergence, timeout and missing telemetry; completion is observed through calibration state and stop actions. The app results cover target clamps and no-CAT rejection. Rig telemetry and carrier are simulated in the harness, not physical. |
| `importConfirmedAdifIntoLogbookAsync` | `bridge_callsign_confirmed_adif_ab.py`; callsign confirmed ADIF section of `tests/BRIDGE_REFACTOR_AB.md` | The app-level signal test imports a local synthetic ADIF and checks the resulting call and LoTW confirmation in an isolated logbook. It does not contact an external provider. |
| `rttyCanTransmit`, `rttyAlzaPtt`, `rttyMandaAudioTx`, `setRttyInAscolto`, `setCwInAscolto` | `tests/run_bridge_refactor_ab.py`; `tests/bridge_refactor_keyboard_audio_harness.cpp`; `legacymirror-full-ab.txt` | The full A/B runner compares the original methods with the extraction and executes both in a simulated bridge. The retained result covers mode and busy guards, keying, sample forwarding and listener notifications. Audio and CAT collaborators are substitutes; the result does not by itself prove radio or audio-device operation. |
| `loadWindowState`, `saveWindowState`, `saveWindowStatesAsync`, `resetWindowLayout` | `tests/run_bridge_refactor_ab.py`; `tests/bridge_refactor_window_state_harness.cpp`; `legacymirror-full-ab.txt` | The original and extracted methods are compared and run against temporary QSettings, including synchronous and asynchronous round trips and reset. This checks persistence behavior, not visual window positioning. |
| SSTV PTT and VOX timing methods in `DecodiumBridgeSstvTiming.cpp` | `tests/run_bridge_refactor_ab.py`; `tests/bridge_refactor_sstv_timing_harness.cpp`; `legacymirror-full-ab.txt` | The runner compares the original SSTV timing section to the extracted section and runs both with SSTV enabled and disabled. The retained results cover setter bounds, active-TX rejection and simulated coordinator acceptance/rollback. The coordinator is a test substitute; no physical relay timing is measured. |
| `fontSettingFont` | `tests/run_bridge_refactor_ab.py`; `font-{baseline,refactor}-{a,b}.txt`; font settings section of `tests/BRIDGE_REFACTOR_AB.md` | The source comparison covers the font helper and method bodies; both application variants exercise family, size and traits through accessors that call this helper. Actual visual rendering and a manual font chooser are outside this evidence. |
| `reloadDxccLookupAsync` | `bridge_localupdate_success_lab.py`; `localupdate/success-ab.txt`; local-update section of `tests/BRIDGE_REFACTOR_AB.md` | Both app variants download fixture data from loopback, save it and complete a DXCC reload with identical reported results. The endpoint and data are synthetic; external services are not covered. |
| `openFileDialog`, `saveFileDialog` | `bridge_file_dialog_runtime_ab.py`; `file-dialog-runtime-ab.txt` | Both source placements pass actual app checks for cancellation, opening a temporary file and choosing a save path. The non-native Qt dialog is used. |
| `sstvDetectedMode`, `sstvDigital` | `bridge_sstv_status_getters_ab.py`, `bridge_sstv_detected_mode_ab.py`; `sstv-status-getters-ab.txt`, `sstv-detected-mode-ab.txt` | Both app placements return empty mode before reception, expose the digital controller with HAMDRM enabled and identify Martin M1 from a synthetic transmission prefix. RF is not covered. |
| `startSstvRadiofaxTxAligned`, `startSstvRadiofaxTxProfile` | `DecodiumBridgeSstvRadiofaxTx.cpp`; `bridge_sstv_radiofax_tx_reject_ab.py`; Radiofax TX extraction section of `tests/BRIDGE_REFACTOR_AB.md` | The public start method calls these forwarding wrappers; the app rejection lab invokes it for invalid IOC, invalid rate and missing image in both placements. The separate Radiofax virtual-audio test demonstrates a centered advanced transmission, not every wrapper option. |
| `prepareStartTxSelection`, `repairOrRejectStalePartnerTxMessage` | `bridge_start_tx_selection_runtime_ab.py`; `bridge_partner_guard_repair_ab.py`, `bridge_partner_guard_reject_ab.py`; corresponding sections of `tests/BRIDGE_REFACTOR_AB.md` | App A/B exercises TX selection guards and synthetic partner repair/rejection; the partner guard also has a normal CQ virtual-audio comparison. These are not an on-air partner QSO. |

`bridge_mam_slot_message_ab.py` now directly compiles and invokes the
v1.0.737 and moved `mamBuildSlotMessage` bodies with the production FTX
message helper. All 192 combinations of four modes, two calls, six TX slots,
RR73 and quick-QSO flags matched and passed an argument-forwarding oracle;
see `mam-slot-message-direct-ab.txt`. The call-format classifier is a test
substitute. The separate existing FT8/FT4 app evidence above establishes a
real dispatch path, not physical transmission.

`bridge_sstv_tap_disconnect_ab.py` directly executes the before-extraction
fixture and current `disconnectSstvProducerTaps` and `disableSstvProducerTaps`
with six real Qt signal connections. Both disconnect every connection, tolerate
repeated disable and handle a null relay; the drain implementation is a test
substitute. See `sstv-tap-disconnect-direct-ab.txt`. Existing two-session
HAMDRM and Radiofax tests separately cover actual virtual-audio paths.

`updateAutoRxInputLevel` appears as different-or-ambiguous in the generic
source index because gain conversions, the maximum level and logging now use
forwarders. `bridge_autorx_ab.py` explicitly normalises those four names,
requires the original and extracted bodies to match, and executes both against
deterministic clipping, cooldown, hold, silence, low-signal and limit cases.
The retained `autorx-runtime.txt` separately records a two-session BlackHole
run in each app variant: the sender entered TX and the auto-enabled receiver
reduced gain. The level setter is substituted in the deterministic harness;
physical gain and RF are outside the app test. Existing results were inspected,
not rerun.

World-map replay: the retained harness in `bridge_worldmapreplay_ab.py`
compares v1.0.737 and moved replay bodies after normalising the logging
forwarder, then executes not-ready, TX-deferred, queued, complete and
interrupted-generation cases. The isolated app A/B
`worldmapreplay-wav-isolated-ab.txt` additionally shows a WAV-derived
contact reaching the band and RX paths in both method placements; its lab
redirects the Bridge legacy INI to avoid shared Qt test settings. This does
not verify the rendered map or establish that all other labs are isolated.
The separate freshness/reset methods have their own harness and app-reset
probe documented in `tests/BRIDGE_REFACTOR_AB.md`.

Indirect SSTV execution review: the existing app A/B probe starts and cancels
WAV replay, receives the finished signal, and verifies restored runtime state;
`finishSstvWavReplay` is connected to that signal and calls
`restoreSstvLiveAfterReplay`. The rebind is substituted by a marker, not a
physical capture check. The raw-audio app A/B invokes both public raw-save
entry points, which call `saveSstvRxRawAudioImpl`, and verifies a real RIFF
file in the isolated test profile. The Studio A/B calls `prepareImage`,
observes the resulting draft, and the source connects the preparation-change
signal to `handleSstvStudioPreparedChanged`. Retained before/after results are
`sstv-replay-cancel-coverage-recheck-ab.txt`, `sstv-raw-audio-repeat-ab.txt`
and `sstv-studio-archive-ab.txt`. No tests were rerun for this review.

Additional indirect-path review: retained app A/B evidence exercises the
Fox/Hound/contest setters through ten transitions
(`specialop-wrappers-ten-ab.txt`), legacy Tune audio and stop over BlackHole
(`tune-legacy-repeat-ab.txt`), save-settings across two isolated restarts
(`save-settings-restart-ab.txt`), SSTV TX startup and ownership release with
complete Radiofax loopback (`sstv_tx_lifecycle/before.json`, `after.json`), UI
preference writes/restarts (`uiprefs-restart-{baseline,refactor}.txt`, plus
`uiprefs-style-restart-{baseline,refactor}.txt` for style), and advanced
mode transitions (`advanced-boundary-*`). These are functional A/B examples
for those named functions, not exhaustive branch, visual-rendering, hardware
or RF proofs. Earlier macOS style tests touched native preferences, as already
disclosed in the main report. HAMDRM tap activation/deactivation remains
supported by two-app exact-payload loopback but lacks separate call-count
instrumentation for deactivation. No tests were rerun in this review.

HAMDRM shared audio tap follow-up: the existing two-app A/B script was run
with `DECODIUM_LAB_VERIFY_MONITOR_OFF=1`. Both source placements transferred
225 exact bytes and passed the lab's assertions that an initially disabled
monitor remains disabled and HAMDRM is inactive after completion. The native
RX backend's shared-audio hooks in `DecodiumBridgeSstv.cpp` call the named
activation and deactivation methods. The compact wrapper output is retained
in `hamdrm-taps-monitor-off-ab.txt`; it does not print call counters or prove
physical audio hardware or EasyPal interoperability.

Audio-level execution review: `bridge_audiolevels_ab.py` compares the release
and extracted bodies after normalizing the named shared helper forwards and
runs `bridge_refactor_audiolevels_harness.cpp`. That harness directly exercises
`sMeter`, automatic RX enable, `applyRxInputLevel` and TX level bounds with
substituted sinks. `audiolevels-runtime.txt` separately records matching
original/refactored isolated app results for auto/manual RX and TX bounds with
both modern and legacy backend active, CAT and monitoring off. Physical gain
and RF are not covered. No test was rerun for this review.

Decode metrics execution review: `run_bridge_refactor_ab.py` compiles the
before/after metrics implementations against
`bridge_refactor_metrics_harness.cpp`. The harness invokes all four timestamp
writers and all four elapsed-time readers, checking their initial values,
independence, elapsed time and reset. The preserved `full-ab-20261110.log`
contains `metrics: A/B OK` and all four result lines. Separately,
`decode-timing-ft8-matched-{baseline,refactor}.txt` records both app variants
decoding the same CQ on slot-aligned BlackHole audio. The end-to-end test
establishes one receive path, not that every timing metric was observed from
an actual decoder callback. No test was rerun for this review.

Radio control and cadence review: the retained full A/B log records SSTV
PTT/VOX timing comparisons with the feature enabled and disabled; their
coordinator is substituted. `cwsettings-audio-runtime.txt` and the main report
record the CW monitor enabled/disabled BlackHole audio scenario in both app
variants; CW remote and rig routing have A/B harnesses with simulated
collaborators. The two AutoCQ cadence result files each report a synthetic
one-call burst, listen period, two-call stop and FT8 virtual-audio decode. These
cover software and virtual-audio behavior, not physical PTT, RF or a real
remote radio. No tests were rerun in this review.

QSO and ADIF review: retained full-export and import-wrapper A/B results each
cover two synthetic inputs on both placements; repeat import exports one
matching QSO. `legacy-log-real-rig-disabled-ab.txt` records one synthetic
entry written to the isolated legacy ADIF file in each placement, with embedded
rig control forced off for that test. The main report documents temporary-file
A/B cases for append, error and normalization, plus loopback TCP delivery.
These cover selected software paths without writing to the operator's logbook;
no complete over-air QSO or physical CAT is implied. No test was rerun.

Network-path evidence review: the A/B ADIF TCP harness used a real local
listener and compared exact synthetic bytes; a separate app probe delivered a
synthetic log record to loopback. `remote-activity-websocket-ab.txt` records
matching authenticated local WebSocket activity/reset in both placements.
The main report describes synthetic-token authentication and command A/B,
and UDP Clear/Halt datagrams delivered over loopback with a test-only bind.
The SSTV startup sharing results record two encrypted images and Gallery import
through a local server in both placements. None of these demonstrates an
external peer, production UDP bind or physical radio. No test was rerun.

Decode-state review: existing before/after app results show a WAV-derived CQ
entering the active-stations model (`secondary-isolated-app-ab.txt`), context
reset and recovery (`decodecontext-runtime-ab.txt`), and decode-history
persistence with session closure (`decodepersistence-runtime-ab.txt`). The
legacy delta two-session test records one initial baseline timeout, then
successful CQ reception on baseline retry and refactor (`legacydelta-pair-ab.txt`).
Its cause remains unknown. The associated focused harnesses compare queue,
reset and persistence logic; external services and physical RF are outside
these results. No tests were rerun for this review.

Settings, services and reception review: retained `load_settings/`,
`save_settings/` and `reload_settings/` contain before/after app results for
two synthetic profiles. `lotw-migration-runtime-ab.txt` records matching
migration and synthetic lookups. The report documents local DX Cluster and
Cloudlog tests and synthetic QRZ/PSK signals, without claiming external
services. The matched FT8 baseline/refactor result files show the same CQ
decoded with audio beginning at 0.456/0.492 seconds of the slot. Earlier
mismatched/failed attempts remain recorded. These scoped observations are not
proof of every preference, credential, external response or RF path. No tests
were rerun in this review.

Spectrum review: `spectrum-cpu-repeat-ab.txt` records the same 956-bin FFT and
999 Hz peak from a synthetic 1 kHz input in both placements.
`spectrum-gpu-handoff-repeat-ab.txt` records one 4096-sample handoff with the
same bounds, and `spectrum-visible-repeat-ab.txt` records matching unsupported-
GPU fallback selection. The main report also documents a subsequent CPU frame
under controlled timing. These results do not establish native GPU rendering,
physical audio capture or RF. No tests were rerun in this review.
