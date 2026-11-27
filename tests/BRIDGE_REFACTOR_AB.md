# Bridge refactoring: A/B checks

The evolving requirement-level completion review is in
`tests/bridge_refactor_evidence/completion_audit.md`; it records evidence,
known gaps and the limits of any test-count estimate.

SSTV helper controls: `bridge_sstv_helpers_controls_ab.py` was rerun on
2026-11-09 and passed two isolated app scenarios in each placement, exercising
mode choices, controls and Radiofax profiles with CAT disabled. Compact output:
`bridge_refactor_evidence/sstv-helpers-controls-repeat-ab.txt`. This is a
control-state test, not reception or RF proof.
The related `bridge_sstv_helpers_qso_share_ab.py` was rerun on 2026-11-09;
both placements passed QSO association and signed encrypted local two-session
image transfer/gallery import checks. Compact output:
`bridge_refactor_evidence/sstv-helpers-share-repeat-ab.txt`. This is loopback
evidence and does not establish external server deployment or RF behavior.

## Legacy compilation follow-up, 2026-11-09

The local Ninja graph contains all 410 `DecodiumBridge*.cpp` sources in both
`wsjtx` and `decodium_qml`; both targets compiled with all corresponding object
files present. The legacy build initially failed because
`DecodiumBridgeMode.cpp` unconditionally included the RTTY band-plan header
while the target had no RTTY dependency. The include is now guarded by the
same `DECODIUM_HAS_RTTY` condition as its use. The existing mode-switch and
period A/B application labs were rerun successfully with CAT and TX disabled;
compact evidence: `bridge_refactor_evidence/bridge-legacy-build-and-mode-ab.txt`.
This checks the local macOS build and synthetic mode transitions, not Windows,
Linux, physical rig switching or an RTTY-enabled legacy build.
As a focused disabled-feature check, the QML target's actual compiler command
was rerun for `DecodiumBridgeMode.cpp` with `DECODIUM_HAS_RTTY=0`, the RTTY
include path removed, and `-fsyntax-only`; it passed. This checks that source
file in the disabled configuration, not a complete second application build.

## Evidence discovery index

### Current triage (2026-11-10)

The entries below distinguish a later successful check from an earlier failed
attempt. They are not a complete function-by-function coverage audit.

| Item | Current evidence | Open limitation |
| --- | --- | --- |
| Periodic TX dispatch | Direct before/after dispatch and two-session virtual-audio tests passed; see the periodic scheduler follow-ups. | An earlier single 13.106 s activity observation remains unexplained. |
| Native JT65 reference | The native FFT-direction correction and before/after Bridge tests decoded the independent recording; see the JT65 correction entry. | Normal legacy-mirror reception and live RF were not established by that forced-snapshot test. |
| CW completion | The CW ownership correction passed before/after virtual-audio tests, including two successive requests. | Physical PTT/RF was not tested. |
| Robot C12 | The release/refactor comparisons and later virtual loopbacks expose a quality/completeness issue in both placements. | The cause of the partial image and edge error is not established. |

The user's requirement is A/B and functional proof for touched functions,
not exhaustive testing of every conceivable branch or unrelated pre-existing
defect. Whether the available evidence meets that requirement for every moved
function still requires review; filename and token counts alone cannot settle it.

Run `python3 tests/tools/bridge_evidence_inventory.py` to regenerate
`tests/bridge_refactor_evidence/bridge_evidence_inventory.json`. The index maps
explicit `DecodiumBridge::` names in split modules to textual mentions in test
source files, excluding Markdown documentation to avoid self-references. It is
a navigation aid, not a coverage report: a
mention does not prove that a test executes the method or compares both
placements; a missing mention does not prove that the method is untested.
Overloads, inline functions, indirect calls and definitions missed by the
simple expression require review. On 2026-11-09 it found 410 modules, 1,281
module/name pairs and 30 pairs without textual mentions in test source. Earlier
figures included documentation, so they are not directly comparable. For example, the
documented ADIF export and SSTV WAV replay tests exercise behaviour indirectly
even though some individual method names are absent from test text. These
figures therefore cannot be interpreted as a count of missing tests.

`tests/tools/bridge_test_anchor_inventory.py` separately checks literal test
insertion anchors against the current main Bridge file. On 2026-11-09 it found
14 potentially stale anchors after the UI-actions and offline A/B scripts were
repaired and rerun. It is a heuristic, not a full test-runner validation. The
two repaired tests passed in both placements; compact results are in
`bridge_refactor_evidence/ui-offline-repeat-ab.txt`.
Subsequent repairs and passing before/after reruns cover manual NTP, local DX
Cluster commands, DecoPort hub refresh and hub rows; compact output is in
`bridge_refactor_evidence/anchor-repair-batch-ab.txt`. The anchor heuristic
then reports 10 candidates. The NTP test uses a loopback destination and the DX
Cluster test uses a local TCP peer; CAT and real RF are not exercised.
The frequency-update and rig-discovery app A/B scripts were subsequently fixed
and rerun successfully; `frequency-rigdiscovery-repeat-ab.txt` records the
results. Frequency changes used an isolated CAT-disabled profile, and rig
discovery checked result structure without controlling a radio. The heuristic
then reported eight candidates.
RTL-SDR status and self-check A/B scripts were then repaired and rerun; both
placements agree in isolated idle app sessions. See
`bridge_refactor_evidence/rtlstatus-selfcheck-repeat-ab.txt`. These tests check
idle status and result structure, not RTL hardware or every diagnostic. The
anchor heuristic then reported six candidates.
Second-instance validation and disabled sharing-configuration A/B scripts were
subsequently repaired and passed in both placements; see
`bridge_refactor_evidence/secondinstance-sharing-repeat-ab.txt`. They check
invalid instance names and configuration defaults, not launching a second app
or a live sharing connection. The anchor heuristic then reported four cases.
The explicit audio-telemetry and FT2 playback-completion scripts were then
repaired and passed in both placements; compact output is in
`bridge_refactor_evidence/txfinish-telemetry-repeat-ab.txt`. The latter uses
virtual audio with CAT off, not physical RF. The heuristic then reported two
candidates.
The TX-enable and destructor A/B scripts were also repaired and passed in both
placements, using simulated TX state with CAT off; see
`bridge_refactor_evidence/txenable-destructor-repeat-ab.txt`. The current
literal-anchor heuristic reports zero candidates. This does not prove that
other, dynamically constructed test anchors are valid or that all functions
have complete A/B runtime coverage.
On 2026-11-09 all 944 Python scripts then present in `tests/tools` parsed
successfully via Python's in-memory `compile()` (no bytecode written). A
further inspection of literal `index`, `find` and `split` references found two
apparent main-file misses; both refer to `DecodiumBridgeSstv.cpp`, where the
anchors exist. Seven apparent `original_main` misses likewise target the SSTV
file and are not evidence of a stale main-Bridge reference. This is static
syntax and reference inspection; it does not establish that the 944 scripts
compile their fixtures, execute successfully, or cover every touched function.

Run `python3 tests/run_bridge_refactor_ab.py --base v1.0.737` with a C++17 compiler, `pkg-config`, Qt6Core and Qt6Multimedia installed. The script compares extracted code with the specified revision and runs both versions through focused harnesses. Where stated in the script it compiles the actual extracted translation unit; other harnesses execute extracted method bodies with simulated collaborators. These checks do not replace application or radio tests.

## TX timing evidence, 2026-11-07

The A/B harness exercises the PCM offset calculation with controlled slot times, invalid inputs, full-frame protection, late trimming, mono/stereo alignment and several mode lead-ins. The bridge and legacy application targets build on the local Mac. This is a numerical and integration-build check, not proof of an RF transmission.

Two separate Decodium sessions were then run without CAT using BlackHole. The immediate FT8 send started an output stream, but the modern receiver decoded no message; the lab trigger explicitly bypasses normal slot timing. The legacy receiver reported no incoming frames. A second attempt used normal AutoCQ timing with the receiver running and output assigned to BlackHole; the logs reported AutoCQ enabled but did not show a TX start. A third attempt enabled monitoring on both virtual-audio sessions and again reported AutoCQ enabled without a logged TX start. Neither AutoCQ attempt proves transmission or decoding. All sessions finished. The real CAT/radio were not used by these tests.

These initial attempts did not establish an end-to-end result. Later successful timed TX/RX observations are recorded under “FT8 two-session follow-up”; the offset path's numerical A/B and the application loopback have distinct scopes, as stated there.

## SSTV Robot C12 two-session comparison, 2026-11-07

Tested the published v1.0.737 Apple Silicon DMG (release digest `2616c4b80c9ea588e90080e81858760742b742dc75da14bca229af8e0fef49bf`) against the locally rebuilt refactor. Each case used two applications, isolated settings, Cocoa, VOX, BlackHole 2ch, no CAT, and a different synthetic image. The published build uses Qt 6.12.0 and the local build Qt 6.11.2, so this is a useful comparison with a runtime-version confounder, not a controlled identical-runtime experiment. Real CAT and radio were not used.

| Pattern | Published 1.0.737 MAE / p95 | Refactor MAE / p95 | Gate |
| --- | --- | --- | --- |
| A | 8.8196 / 65 | 8.8288 / 65 | Both fail (limits 5 / 15) |
| B | 13.1537 / 61 | 13.2259 / 61 | Both fail (limits 5 / 15) |

The failed gate in both versions is especially visible at the last image column. Some other attempts timed out before terminal receive; those were not included as successful images. Matching failures do not prove full interoperability or eliminate all regression risk. Compact logs, hashes and reference/received images for the four completed cases are preserved under `tests/sstv/evidence/bridge-refactor-robot-c12-2026-11-07/`. The FT8 loopback was subsequently successful as described below; this does not resolve the separate Robot C12 quality failure.

### Follow-up: preserving FT8 slot timing

The test-only environment switch `DECODIUM_LAB_FTX_SLOT_TIMING=1` suppresses the lab's forced-immediate bypass only when Qt test mode is active. With no switch, an isolated test-mode launch still logged the original immediate-bypass behaviour; with the switch, a separate isolated test-mode launch logged preserved slot timing. Both launches used CAT disabled, and the default-behaviour check scheduled no transmission before quitting.

A two-session FT8 attempt then used separate `DECODIUM_LAB_SETTINGS_DIR` roots, BlackHole devices and CAT disabled. The sender reported a CQ request and an opened BlackHole output. The receiver started and dispatched FT8 decoding, but its recorded decode result had `nout=0`; no `9H1SR` decode was observed. Therefore this test does not establish a successful end-to-end decode or prove that the extracted PCM offset method was reached. Further investigation is required. The real radio and CAT were not used.

### FT8 two-session follow-up

With both sessions monitoring separate virtual inputs, CAT disabled, isolated settings and the test-only slot-preserving switch, an independent BlackHole capture measured approximately 12.7 seconds of non-silent FT8 audio. The receiver saved `CQ 9H1SR JM75` in its new test-profile decode database. A second run using `tests/tools/bridge_ft8_two_app_lab.py aligned` also decoded that message, with the first measured non-silent block about 0.44 seconds after a 15-second boundary. This is a virtual-audio integration result, not RF proof. The numerical old/new A/B of the extracted offset method remains in `run_bridge_refactor_ab.py`; the two-session run alone does not instrument whether that method was called.

A comparison using the lab's original immediate-start behaviour decoded the message when the first measured block was about 1.37 seconds into a slot. A second immediate-start run emitted audio about 10.31 seconds into a slot but produced no matching RX decode and exited with a failing test status. These observations support preserving slot scheduling in the test; they do not establish that every possible timing will succeed or fail.

## FTX settings extraction, 2026-11-07

The 12 settings methods in `DecodiumBridgeFtxSettings.cpp` are compared with v1.0.737 after normalising the logging wrapper. The A/B harness runs the method bodies against isolated QSettings and simulated bridge signals, checking both limits or toggle transitions and unchanged-value notification suppression. The production QML and legacy targets both build, and `test_settings_profile_restart` and `test_qso_sequencer_rules` pass. These simulated-collaborator tests do not establish actual QML interaction or RF behaviour for each setting.

The separate `tests/tools/bridge_ftx_settings_runtime_lab.py` probe ran two distinct value sets through all 12 actual Bridge properties from QML in isolated test-mode application sessions. Both sets passed, with CAT and monitoring disabled. This is an A/B comparison of input sets on the refactored application, not an old/new runtime comparison, and does not demonstrate radio transmission. The probe temporarily instruments the build copy of Main.qml and restores it; run it without other build instances. Both temporary settings roots were removed after the test.

The FTX settings module was subsequently extended by 15 FT2/FT8 toggles. The same baseline comparison and simulated-collaborator checks cover each toggle, including caller-queue scoring on enable. The two isolated application probes now exercise 27 actual QML bridge properties with distinct input sets; both passed with CAT disabled. `test_caller_scoring`, `test_qso_sequencer_rules`, and `test_settings_profile_restart` also passed. The application probes are not old/new binary comparisons or RF tests.

## Decode colours extraction, 2026-11-07

The colour helper functions and Bridge methods were moved to `DecodiumBridgeDecodeColors.cpp`, with shared declarations in `DecodeColorHelpers.h`. Baseline A/B checks verify helper and method bodies against v1.0.737 and exercise 20 properties, invalid names, defaults, saved settings, legacy synchronisation, notification suppression and highlighting precedence using simulated bridge collaborators. Both production targets build, and `test_settings_profile_restart` passes.

Two isolated application sessions run `tests/tools/bridge_decode_colors_runtime_lab.py` with distinct colours through the real QML/Bridge methods. Both pass the enabled/bold/background/precedence and strikethrough checks with CAT disabled. This is not an old/new binary comparison or visual/RF proof. The probe temporarily modifies the build copy of Main.qml and restores it; do not run concurrently with another build session.

The application colour probe was expanded to all 20 categories and repeated after restarting each of two isolated profiles. It checks the enable flag, bold flag, user background, strikethrough and reload. TX and CQ use different background colours so their priority check is discriminating. Both profiles passed on initial write and after restart; CAT remained disabled. This checks actual Bridge/QML state and persistence, not pixel rendering or a before/after release comparison.

A direct old/new runtime comparison was then performed using the published 1.0.737 Tahoe Apple Silicon DMG (SHA-256 `1efbc8f29b8cdd8d70f21c436c5a8bf2c0d7102da525c012d9d01dfde41b0e0a`, matching its published checksum) and the local refactor. Identical QML probes exercised all 20 categories in two profiles, on first write and restart, including distinct TX/CQ colours to test priority. Both applications passed all four cases and their concise logs match (`tests/bridge_refactor_evidence/decode-colors-*-macos.txt`). The release and local builds use different Qt versions; this compares observable Bridge/QML settings and persistence on this Mac, not rendered pixels or RF. Both used isolated settings, no monitoring and no CAT.

The 27 FTX settings were also compared through the actual QML/Bridge using the checksum-verified v1.0.737 Tahoe ARM64 release and the local build. Each binary passed the same two distinct value sets and retained them after restarting its isolated profile. The four concise results match in `tests/bridge_refactor_evidence/ftx-settings-{v1.0.737,refactor}-macos.txt`. This checks setter/getter and persistence behaviour in the real applications; it does not prove RF or every downstream effect of these options, and the Qt versions differ. CAT and monitoring were disabled.

## Decode dedup extraction, 2026-11-07

Native/FT2 decode deduplication indices and RX mirror index rebuilding were moved to `DecodiumBridgeDecodeDedup.cpp`. The old/new method bodies match after normalising forwarding wrappers; the focused A/B harness exercises cache capacity, repeated entries, session changes, TX/empty/old-entry filtering, FT2 and index rebuilding with simulated key functions. Both application targets compile. The existing FT8 auto-TX source-wiring test was updated to inspect the extracted timing file and passes, as does the Q65 continuity test.

An isolated two-application FT8/BlackHole loopback then emitted and decoded `CQ 9H1SR JM75`; the compact result is `tests/bridge_refactor_evidence/decode-dedup-ft8-loopback-macos.json`. CAT was disabled and no RF was used. This confirms an FT8 path still operates but does not by itself prove each dedup branch was reached or cover FT2 radio operation.

A broader check of 12 existing tests that inspect Bridge source files identified two stale path assumptions after extraction. `test_b4_worked_before` now examines the actual colour module; `test_mam_tx_payload_policy` verifies that two placeholder calls remain in the main file and one is in the caller-queue module, preserving the original total. Both were rebuilt and all 12 tests passed. These are source-wiring and existing functional test results, not evidence of on-air operation.

## Ready profiles and AutoCQ extraction, 2026-11-07

Seven Bridge methods were moved to `DecodiumBridgeFtxProfiles.cpp`. A/B method-body checks and a simulated-collaborator harness cover weak, contest, CPU and balanced profiles, manual-clear guard, AutoCQ boundaries, persistence and legacy-backend callbacks. Both application targets build; `test_auto_cq_call_policy`, `test_caller_scoring`, and `test_settings_profile_restart` pass.

The published checksum-verified v1.0.737 Tahoe ARM64 application and local build passed the same real QML test of all four ready profiles, AutoCQ maximum bounds and persistence after restart with isolated settings and CAT/monitoring disabled. Concise results match in `tests/bridge_refactor_evidence/ftx-profiles-{v1.0.737,refactor}-macos.txt`. This is not an RF or actual legacy-backend radio test, and the binaries use different Qt versions.

## TX messages extraction, 2026-11-07

TX1–TX6 accessors and setters, selected-message editing, validation and reset were moved to `DecodiumBridgeTxMessages.cpp`. The old/new method bodies match after forwarding-wrapper normalisation. The A/B harness exercises boundaries, notifications, cache/precompute hooks, custom CQ, resets, mode dispatch and a simulated legacy backend; its encoder results are simulated and do not validate actual waveform encoding. Both application targets build. `test_custom_cq_persistence` was updated to inspect the moved methods and forwarding wrapper; this and `test_tx_dispatch_guard` and `test_ft8_auto_tx_decode_gate` pass.

A separate real-application comparison ran the same QML editing, bounds, custom-CQ, actual FT8 encoder validation and reset probe on the checksum-verified v1.0.737 Tahoe ARM64 application and the local refactor. Both passed with matching concise evidence in `tests/bridge_refactor_evidence/tx-messages-{v1.0.737,refactor}-macos.txt`. CAT and monitoring were disabled; this is not a radio transmission, and Qt versions differ.

The TX-message runtime comparison was repeated on both real applications with an explicit assertion that `legacyBackendActive` is true before editing. Both passed with CAT disabled; concise matching logs are in `tests/bridge_refactor_evidence/tx-messages-legacy-{v1.0.737,refactor}-macos.txt`. This observes QML/Bridge behaviour while legacy is active, but does not read back the embedded legacy widget values or key a radio. The 13 source-wiring regression tests were also rerun and passed.

## Logbook profiles extraction, 2026-11-07

Logbook profile listing, backup, creation, addition, switching and deletion moved to `DecodiumBridgeLogbookProfiles.cpp`; the existing ADIF document declarations are shared through `AdifDocumentInternal.h`. The source A/B comparison normalises forwarding wrappers to the existing ADIF writer and counter. The focused harness uses temporary files and simulated Bridge collaborators, covering missing files, collisions, backups, URL paths, invalid requests, active/inactive deletion and replacement of the last profile. Both app targets build and the existing worked-before and profile-restart tests pass; the worked-before source test retains a bounded check after the moved method.

The checksum-verified 1.0.737 Tahoe ARM64 release and local build both passed the same QML application probe for creating, switching, backing up and deleting two new logbooks. Each used a random test-mode application name and isolated settings; before deletion the probe verifies both paths are under `.qttest` and contain the random name. CAT and monitoring remained disabled. The concise matching results are in `tests/bridge_refactor_evidence/logbook-profiles-{v1.0.737,refactor}-macos.txt`. This does not cover every failure path in the real application, and the Qt versions differ.

The logbook module was extended to include path selection, filename sanitisation, profile registration and settings load/save. Old/new source and isolated QSettings harness checks pass for naming, unique profile names, path fallback and save/reload. Both applications build; worked-before, profile-restart and QSO-sequencer tests pass. The real QML logbook probe now creates two random-name test profiles, restarts the same isolated profile, then switches, backs up and deletes only the two paths after checking their `.qttest` and random-name markers. Both the checksum-verified v1.0.737 application and the local build pass; matching concise logs are `tests/bridge_refactor_evidence/logbook-settings-{v1.0.737,refactor}-macos.txt`. CAT and monitoring remained disabled; the Qt versions differ.

## Time-sync extraction, 2026-11-07

`applyNtpSettings` and `configureNtpClientForMode` were moved to `DecodiumBridgeTimeSync.cpp`. Old/new method bodies match after logging-wrapper normalisation. The focused A/B harness uses isolated settings and simulated clocks/NTP clients to test default and legacy settings precedence, enable/disable, stored offset, FT2/FT4/FT8 timing and missing-client behaviour. A separate two-translation-unit executable checks that the actual inline `PrecisionTime.hpp` global offset is shared across files. Both app targets build; the profile restart, QSO sequencer and FT8 auto-TX gate tests pass.

The checksum-verified 1.0.737 application and the local build both pass the same real QML test of enable/disable and stored offset with `127.0.0.1` as the configured NTP destination and CAT/monitoring disabled. Matching logs are `tests/bridge_refactor_evidence/timesync-{v1.0.737,refactor}-macos.txt`. This does not demonstrate successful NTP synchronisation with a server or on-air timing; the Qt versions differ.

## ADIF export extraction, 2026-11-07

`exportAdif` and the import delegation were moved into the logbook module. Old/new source comparisons normalise forwarding wrappers for band and UTC helpers. Isolated temporary-file A/B tests cover empty paths, copying an existing ADIF file, exporting to the same path, converting an ALL.TXT fixture including field content, and import delegation (simulated). Both applications build and selected existing logbook/QSO tests pass.

The checksum-verified v1.0.737 application and local build also passed the QML logbook lab with an additional actual ADIF export to a temporary file whose `<EOH>` header was checked, followed by restart and removal of the generated test logbooks. CAT/monitoring were disabled. Matching concise logs are `tests/bridge_refactor_evidence/adif-export-{v1.0.737,refactor}-macos.txt`. The runtime case verifies an empty generated logbook, not an actual imported contact or RF operation; the Qt versions differ.

A further application A/B used `tests/tools/bridge_adif_roundtrip_runtime_lab.py` to create a unique test-mode logbook, import a synthetic ADIF record asynchronously, verify one QSO, export it, and check its call, date, time, frequency and EOR fields. The checksum-verified v1.0.737 application and local build both passed; CAT and monitoring were disabled. Before importing or deleting, the lab checks the generated logbook path contains both `.qttest` and its random application identifier. Matching logs are `tests/bridge_refactor_evidence/adif-roundtrip-{v1.0.737,refactor}-macos.txt`. This is a synthetic local contact, not an on-air QSO; the Qt versions differ.

## SSTV storage state extraction, 2026-11-07

SSTV audio-job, storage, autosave and WAV-replay state accessors and the autosave setter were moved to `DecodiumBridgeSstvStorageState.cpp`. Old/new method bodies match. A/B harnesses cover both `DECODIUM_HAS_SSTV=0` and `=1`, unavailable controllers, storage-not-ready rejection, enable/disable, no-op updates and simulated job/replay state. Both app targets build and the existing SSTV TX-state, coordinator and QML settings tests pass.

The checksum-verified v1.0.737 Tahoe ARM64 app and local build both passed the same real QML probe for ready storage, initial job/replay state and autosave enable/disable with isolated profiles and CAT/monitoring disabled. Matching evidence is `tests/bridge_refactor_evidence/sstv-storage-{v1.0.737,refactor}-macos.txt`. The real probe does not receive or save an image, nor exercise real replay/audio-job execution; the Qt versions differ.

### SSTV automatic-save two-session follow-up

On macOS, the checksum-verified v1.0.737 release and the local refactor were each tested with two isolated Decodium instances, BlackHole 2ch, VOX, CAT disabled and two synthetic Robot C12 patterns. A test-only QML probe enables autosave and waits for `sstvRxSaveState == saved`; the lab also requires complete TX/RX and verifies the isolated RX gallery database contains a complete received Robot C12 record and a PNG. All four final runs passed these storage checks. Logs are `tests/bridge_refactor_evidence/sstv-autosave-{baseline,refactor}-{a,b}.txt`; two earlier baseline attempts stopped with an incomplete image and remain documented in `sstv-autosave-baseline-incomplete-{1,2}.txt`.

The storage probe uses permissive image-error limits (255/255), while leaving normal lab defaults unchanged. It does **not** establish image-quality acceptance: for pattern A the release/refactor MAE was 8.883/8.843 and p95 65/65; for B it was 12.516/12.516 and p95 59/59. All four remain outside the normal MAE 5/p95 15 gate, with last-column errors up to 255. Different Qt versions and virtual audio are limitations; no RF was used. The updated lab options require an explicit environment setting, and the QML build/bundle copy is restored after each run.

### SSTV WAV replay follow-up

`tests/tools/bridge_sstv_wav_replay_runtime_lab.py` generated two different 12 kHz mono PCM test tones, replayed each to completion, verified active state, filename, non-regressing progress, completed progress and no error, then started and cancelled a longer WAV. The checksum-verified v1.0.737 macOS application and the local refactor both passed both cases; matching logs are `tests/bridge_refactor_evidence/sstv-wav-{baseline,refactor}-{a,b}.txt`. Settings were isolated and CAT/monitoring disabled. These are synthetic tones and demonstrate replay lifecycle rather than reception of a valid SSTV image; Qt versions differ.

## AutoCQ cadence extraction, 2026-11-07

AutoCQ eligibility, burst/listen cadence, completion accounting and legacy cadence forwarding moved to `DecodiumBridgeAutoCqCadence.cpp`. Old/new bodies match after normalising the logging wrapper. The A/B harness uses real `AutoCqCallPolicy.hpp` with a simulated Bridge, clock and legacy backend, and exercises eligibility, partner/disabled guards, ignored failures, burst timing, QTimer wake-up, mode-change reset and max-call stop. Both app targets build; AutoCQ policy, FT8 auto-TX gate and QSO sequencer tests pass.

The checksum-verified v1.0.737 release and local build were each tested with two real application instances, isolated settings, CAT disabled and audio routed only through BlackHole. A synthetic noise stream on BlackHole 16ch keeps the sender RX-health guard active; the sender transmits FT8 to BlackHole 2ch and the other instance decodes it. With one CQ per burst, two listen cycles and a two-call maximum, both tests observed two completed TX transitions, at least 28 seconds between the first completion and second start, automatic disable after the second call, and an RX decode. The measured gaps were approximately 46.76 seconds in both runs. Compact logs are `tests/bridge_refactor_evidence/autocq-cadence-{v1.0.737,refactor}-macos.txt`. An earlier baseline attempt with silent virtual input did not transmit because the RX-health guard suspended AutoCQ; its failed test output is retained as `autocq-cadence-baseline-silent-input-failed.txt`. These are virtual-audio tests, not RF, and the Qt versions differ.

## ADIF parser/writer extraction, 2026-11-07

The existing bounded ADIF record parser and document loader/writer were moved to `AdifDocumentInternal.cpp`; declarations and record types are in the shared internal header. The A/B harness executes the old source and compiles the actual new translation unit with the real export sanitizer, covering missing input, multi-record parsing, truncation, oversized fields, per-record field caps, UTF-8 round trips, unset-field sanitisation and BOM handling. Both app targets build; ADIF sanitizer, worked-before and profile-restart tests pass.

The checksum-verified v1.0.737 application and the local build each imported and re-exported two different synthetic contacts via isolated test logbooks, including a UTF-8 accented comment. The test checked the saved contact fields and removed only its own verified test logbook. All four runs passed; concise matching logs are `tests/bridge_refactor_evidence/adif-document-{baseline,refactor}-{a,b}.txt`. CAT/monitoring were disabled and no real contact log was used. The two application builds use different Qt versions.

## ADIF normalization extraction, 2026-11-07

ADIF date/time normalization, display mode, frequency/band mapping, imported record normalization and deduplication keys were moved to the internal ADIF document module. The old/new source bodies match after linkage normalization; the A/B harness compiles the actual new translation unit and exercises normalized/raw keys, date/time, submode, frequency fallback, band and record fields. Both app targets build and the ADIF sanitizer, worked-before, profile-restart and QSO-sequencer tests pass.

The isolated application round-trip probe now imports the same synthetic QSO twice and verifies that the exported ADIF still contains exactly one record. The checksum-verified v1.0.737 app and local build both passed with two different contacts, including a UTF-8 comment; matching logs are `tests/bridge_refactor_evidence/adif-normalization-{baseline,refactor}-{a,b}.txt`. No real logbook was used; CAT/monitoring were disabled and the Qt versions differ.

### ADIF background import extraction

`AdifImportInternal.cpp` contains the unchanged background merge and confirmed-import workers, including atomic backup and concurrent-file-change guards. The A/B harness compares the original v1.0.737 bodies with the extracted implementation and runs both against isolated synthetic files: merge, duplicate, backup, invalid input, source and destination mutation, and LoTW/QRZ/eQSL confirmation cases. No external service is contacted.

The original and refactored applications each imported two different synthetic ADIF fixtures twice and exported one matching contact per fixture, with CAT disabled. Results are in `bridge_refactor_evidence/adif-import-{baseline,refactor}-{a,b}.txt`. These runtime checks cover the general import/export path; provider-specific cases were exercised in the isolated A/B harness, not through the provider UI. The original application and local build use different Qt versions. Both QML and legacy targets compiled; the focused ADIF, worked-before, settings and sequencer regression tests passed.

The bridge-side asynchronous import orchestration now lives in `DecodiumBridgeAdifImport.cpp`. Its methods are compared against v1.0.737 source, allowing only the existing shared logging wrapper. Both application targets compile. The two isolated application import/export scenarios were repeated after this extraction and match the earlier original-application A/B results; see `adif-async-refactor-{a,b}.txt`. Confirmed-provider worker cases are covered by the isolated A/B harness; provider UI and live services were not exercised.

Compatibility wrapper follow-up: `bridge_adif_import_wrapper_ab.py` places
`importFromAdif` and its progress setter in the original and extracted units,
then runs two isolated application scenarios per placement. Each invokes the
compatibility wrapper twice, observes progress reaching 100, verifies one
deduplicated QSO, and exports the resulting synthetic record. CAT is disabled.
Results: `bridge_refactor_evidence/adif-import-wrapper-ab.txt`. This confirms
successful-path progress and duplicate handling, not every progress or error
branch; the app runs compare placement, not independent release binaries.
The same scenarios now also reject a missing input, verify progress resets to
zero with the expected error status, then complete a valid import. Both
placements passed; see `bridge_refactor_evidence/adif-import-guards-ab.txt`.

### ADIF editing and selected export

`DecodiumBridgeAdifEditing.cpp` contains full and selected export, editing and deletion of contacts. Its implementations and the shared helper bodies are compared to v1.0.737. Both application targets compiled; four focused regression tests passed. The original and refactored applications each executed two isolated synthetic scenarios: selected export, rejection of active-log overwrite and stale selection, editing, deleting, repeat-delete and resulting QSO count. Result logs are `adif-edit-{baseline,refactor}-{a,b}.txt`; these tests do not exercise real logbooks or radio hardware. The build used local Homebrew Qt, which emits minimum-macOS-version link warnings.

The real-app A/B edit probe saves and reads back the comment `Modifica città ✓` before deletion. A subsequent extension exports the edited QSO to a second temporary ADIF and validates the exact UTF-8 bytes, the byte-length field and a single record. Both the checksum-verified 1.0.737 app and local build passed; `adif-edit-export-{baseline,refactor}.txt`.

Full-export follow-up: `bridge_adif_full_export_ab.py` places the identical
`exportToAdif` method in the original and extracted translation units and runs
two isolated app scenarios for each. The app rejects an empty export path,
imports and exports one synthetic QSO, and the lab verifies the resulting ADIF
fields and record count. CAT is disabled; the logs are
`bridge_refactor_evidence/adif-full-export-ab.txt`. The earlier source check
compares this method with v1.0.737; this runtime A/B specifically compares code
placement, not two independent release binaries.

The same four app scenarios also reject an export destination whose parent
directory does not exist, then successfully export to a valid path. Results:
`bridge_refactor_evidence/adif-full-export-errors-ab.txt`. This tests failure
to open a destination, not an error during writing or committing a save file.

### Advanced decoder controls

`DecodiumBridgeAdvancedDecode.cpp` contains the unchanged automatic/manual Neural Sync, Turbo Feedback and coherent averaging decisions. Both targets compiled and the complete A/B suite compares these methods with v1.0.737. Two isolated real-application scenarios on both versions cover manual enabled/disabled settings, initial history, sparse and full rolling history, reset on mode change, and the -22 dB boundary. Logs are `advanced-decode-{baseline,refactor}-{a,b}.txt`; CAT was disabled. These tests exercise decision controls, not actual FT8/Q65 RF decoding. The selected auto-TX gate and settings restart regression tests also passed.

The expanded real-app A/B probe also checks an average of exactly two decodes per slot and verifies that assigning the already-enabled automatic mode does not clear its history. Both manual-setting variants passed against the checksum-verified v1.0.737 app and the local build; `advanced-boundary-{baseline,refactor}-{a,b}.txt`. The app builds use different Qt versions and do not establish RF decoding.

### Panadapter FFT state

`DecodiumBridgePanadapterFft.cpp` contains the unchanged FFT availability and runtime backend setters. Both QML and legacy targets compiled. The full A/B suite compares both bodies with v1.0.737, and isolated application runs on both versions check the unavailable guard, CPU fallback, default and explicitly named backends in two variants. See `panfft-{baseline,refactor}-{a,b}.txt`. These exercises verify control state, not a measured GPU FFT or visual waterfall rendering. Three selected panadapter/waterfall regression tests passed. CAT was disabled in the application runs.

### UI preferences

`DecodiumBridgeUiPreferences.cpp` contains the original UI quality, style, pop-out and spectrum FPS setters. Both QML and legacy targets compiled, and the full A/B source comparison permits only the existing shared logging wrapper. Two isolated real-application variants compared original and refactored behavior for style validity, quality validity, pop-out toggles and FPS boundaries; see `uiprefs-{baseline,refactor}-{a,b}.txt`. Separate isolated write/restart runs cover quality, auto-detach, frameless pop-outs and FPS in both placements; see `uiprefs-restart-{baseline,refactor}.txt` and `tests/tools/bridge_ui_preferences_restart_lab.py`. The earlier restart probe did not check style; the subsequent expanded probe described below does. None of these results establishes visual presentation. CAT was disabled. The selected settings restart, GPU recovery and waterfall recovery regression tests passed.

The earlier restart logs do not demonstrate style persistence: the probe at that time did not set or check style. The corrected probe was run against the checksum-verified v1.0.737 Tahoe ARM64 application and the local refactor, each with two isolated write/restart scenarios using Fusion and Material. All eight phases passed; see `uiprefs-style-restart-{baseline,refactor}.txt`. The binaries use different Qt versions. The quality check switches to a non-flat theme before inspecting the property. Actual visual style/rendering remains outside these tests.

Correction to UI test isolation: on macOS the explicit application-wide style uses native QSettings and is not redirected by `DECODIUM_LAB_SETTINGS_DIR`, which redirects only INI stores. Earlier UI test runs could therefore change the user's saved application style. The labs no longer write that style; the previous test evidence should not be interpreted as fully isolated for that setting. The saved native `UI.Style` was observed as `Material` after those runs; its previous value is unknown. Do not restore an assumed value. The quality persistence gap was subsequently checked with a non-flat theme and two write/restart variants against both versions; logs are `uiprefs-quality-{baseline,refactor}.txt`. Style visual presentation is still unverified.

### FT2-Link access bridge

`DecodiumBridgeFt2Access.cpp` contains the original lock and password-verification bridge methods, calling the previously extracted crypto helpers. Both build targets compiled. The A/B harness exercises configured, missing, weak and malformed synthetic credentials, successful and failed verification, repeat operations and lock notifications. Two isolated application scenarios compare invalid-password rejection and locked state on original and refactored builds; see `ft2access-{baseline,refactor}-{a,b}.txt`. Successful unlock is covered by the synthetic harness, not by an application configured with user credentials. CAT was disabled. Four FT2-Link/settings regression tests passed.

Successful access follow-up: `bridge_ft2_access_success_ab.py` temporarily builds synthetic compile-time credentials and exercises the actual QML bridge in both source placements. A wrong password leaves access locked, the synthetic correct password unlocks it and an explicit lock restores the locked state. Both app results match in `ft2-access-success-ab.txt`; the credential-bearing source and production executable are restored afterwards. This tests synthetic credentials and source placement, not the user's credentials or an external service. CAT and TX are off.

### Decode timing metrics

`DecodiumBridgeDecodeTiming.cpp` contains time-sync mode classification, sample filtering, cycle aggregation and reset. Both targets compiled. A/B synthetic calls compare both implementations for mode recognition, FT4/FT8 SNR and timing boundaries, median and smoothing, decay, cleanup and reset. The selected FT8 gate regression passed.

The two-instance FT8 virtual-audio proof remains inconclusive as an A/B comparison. The original release lacks the offscreen plugin; its Cocoa retry emitted audio but did not decode a message, while the refactored Cocoa run received `CQ 9H1SR JM75`. The original log says its lab trigger bypassed slot timing, and the first recorded audio began about 8.36 s into a 15 s slot versus 0.40 s in the refactored run. Consequently these are not matched stimulus conditions and cannot establish a behavior difference or equivalence. See `decode-timing-ft8-*.txt`. CAT was disabled and no RF was used.

Follow-up: the matched two-instance FT8 test now schedules the same virtual-audio TX trigger near a UTC slot boundary in both applications. Both decoded `CQ 9H1SR JM75`; first audio was at 0.456 s and 0.492 s respectively into the 15 s slot. Logs are `decode-timing-ft8-matched-{baseline,refactor}.txt`. The earlier unmatched and missing-plugin runs remain recorded above. This demonstrates one FT8 message via BlackHole, not physical RF or all timing-metric paths; CAT was disabled.

### WAV decode dispatch

`DecodiumBridgeWavDecode.cpp` contains the unchanged single-file and folder WAV decode dispatch. Both application targets compiled. The A/B harness compares extracted code with v1.0.737 and exercises synthetic 8/16-bit PCM conversion, queued buffer dispatch, missing/short/unsupported files, empty/missing folders and ordered batch dispatch. Two isolated application variants on each version exercised single-file and batch status and missing-file error with synthetic WAVs; see `wav-decode-{baseline,refactor}-{a,b}.txt`. They do not demonstrate decoding a real radio message from WAV. Three targeted decoder regression tests passed; CAT and monitoring were disabled during app probes.

Follow-up actual FT8 WAV message comparison: two generated signals are preserved under `bridge_refactor_fixtures`. The stage-0 FT8 test decoder independently recovered both messages, but neither v1.0.737 nor the refactored application produced a matching decode within 24 seconds after opening either WAV. CAT and monitoring were disabled and the application reported legacy TX active. See `wav-ft8-message-ab.txt`. The `feedAudioToDecoder` legacy-RX early return is a plausible explanation, but active TX alone does not establish the RX route and the cause has not been confirmed. This is an existing, reproducible limitation in this setup, not evidence of successful end-to-end WAV decoding.

Follow-up legacy RX diagnosis: temporary test-only instrumentation showed `legacyRX=true` and `forced=false` in the normal WAV path, which takes the legacy-RX early return in `feedAudioToDecoder`. A temporary forced-snapshot experiment reached that decoder and decoded the two preserved FT8 WAV messages with a different local callsign; see `wav-ft8-legacy-diagnosis.txt`. The first second-fixture attempt used the same local call as the decoded station and did not display it; retrying with a different local call succeeded. The temporary experiment and logging were removed and both targets rebuilt, so no production behavior was changed. This identifies an existing legacy-WAV dispatch limitation; it does not verify a production fix, asynchronous batch behavior under a fix, or RF.

### Font settings

`DecodiumBridgeFontSettings.cpp` contains the unchanged font family resolver, fallbacks, size/trait accessors and setters. Both application targets compiled; the complete A/B suite compares both method and helper bodies with v1.0.737. Two isolated application variants on each version exercised enumerated families, family/size/trait roundtrip, label, chooser notification and reset, using a synthetic setting rather than the global application font. Logs are `font-{baseline,refactor}-{a,b}.txt`; CAT was disabled. An initial probe chose the first enumerated family and failed exact family/label equality on both versions; the corrected probe uses Arial when installed. Visual rendering, a real font-dialog choice, and global font persistence were not verified. The settings-profile restart regression passed.

### Pending log prompt fields

`DecodiumBridgeLogPrompt.cpp` contains the next-QSO comment, locator, time and spot override logic. Both QML and legacy targets compiled. The A/B harness covers trimming, grid normalization, spot override, dates, time-only input, reversed times and fallback; two real-application variants verify the synthetic comment in the preview and isolated settings, with CAT disabled and without logging or sending spots. Logs: `logprompt-{baseline,refactor}-{a,b}.txt`. The harness exposed existing behavior in both versions: a textual `UTC` suffix falls back instead of parsing because `T` is replaced before suffix removal; `Z` parses. This refactor preserves that behavior. The runtime probe does not demonstrate the complete QSO logging/spot path. Three selected logbook/settings/sequencer regression tests passed.

### ADIF record generation

`DecodiumBridgeAdifRecord.cpp` contains the original band lookup and record serialization; a wrapper exposes the existing frequency parser and the existing declaration now has external linkage. Both QML and legacy targets compiled. The A/B harness compares source and executes original and new serializers against synthetic normal, UTF-8/satellite, zone, invalid-frequency and time-fallback cases. In addition to its lightweight fake band lookup, both variants were compiled and run against the actual `models/Bands.cpp`, its Qt moc, the original frequency parser and the real ADIF sanitizer; results are `adif-record-real-{baseline,refactor}.txt`. Three ADIF/logbook/sequencer regression tests passed. No QSO was logged and no external upload or spot was sent; this does not prove the complete application log/upload pipeline. Linker warnings about local Homebrew Qt minimum macOS versions remain.

### Decode history

`DecodiumBridgeDecodeHistory.cpp` contains retention, the read-only SQLite connection, history/session/band/mode/stats queries and ADIF export. Both QML and legacy targets compiled; the A/B suite compares source and runs both implementations against an independently created temporary SQLite database. Cases cover callsign/band/mode/date/session/limit filters, parameterized input, sessions and stats, ADIF export, old-record pruning and orphan removal. The app A/B probes use fresh isolated sessions to exercise queries and empty export; see `history-{baseline,refactor}-{a,b}.txt`. Pruning and populated exports were verified in the temporary SQLite harness, not against user data. Three selected model/settings/logbook regressions passed. CAT and monitoring were disabled.

Populated application follow-up: `bridge_decode_history_app_ab.py` inserts a synthetic K1ABC decode in each unique test-mode app database, queries it by callsign and exports ADIF. Both v1.0.737 and current method bodies returned one row and produced identical ADIF containing the expected call, frequency and record terminator. A path guard requires both `.qttest` and the random test rig name before writing; CAT, monitoring and TX are disabled. The test inserts a record directly and does not simulate live decoding; both method versions compile in the extracted file.

The populated app test now inserts a second, different decode as a negative control; the filtered query and ADIF contain only K1ABC and exactly one record. Both method versions passed on 2026-11-10.

The same application test checks the two-record total, distinct 20m/40m bands, FT8 mode and the 40m filter. Both method versions returned the expected values on the isolated database.

The app A/B also submits a SQL-like callsign string against the two-record database and checks that it returns zero records in both versions. This is one input case, not a general security assessment.

The populated app A/B now also verifies a filter with no matching records returns false and writes only the ADIF header, with no record terminator. Both versions passed using temporary destinations.

An export to a nonexistent parent directory was added to that app test; both versions returned false. This tests an unavailable path, not every operating-system permission or disk-full error.

### MAM frequency planning

`DecodiumBridgeMamFrequencyPlan.cpp` contains the unchanged status, mode-dependent gap and CQ frequency planning methods. The A/B harness compares source and exercises FT2/FT4/FT8 spacing, occupied frequencies, bounds and zero count. Two isolated application variants compare mode-dependent status on original and refactored builds; `mamplan-{baseline,refactor}-{a,b}.txt` contains results. Both QML and legacy targets compiled; selected MAM payload and FT8 gate regressions passed. The application probe does not transmit or validate a live multi-stream QSO; CAT and monitoring were disabled.

### SSTV RX controls and mode choices

`DecodiumBridgeSstvControls.cpp` contains the unchanged RX control snapshot and selectable mode list. Existing anonymous-namespace formatters are accessed via thin wrappers; both QML and legacy targets compiled. Source equivalence was checked against v1.0.737 after accounting for earlier extraction of unrelated audio-job getters. Two app A/B variants compare mode choices and notch/FAX480 enabled, disabled and profile snapshots; see `sstvcontrols-{baseline,refactor}-{a,b}.txt`. Six SSTV mode, controls and QML regression tests passed. These tests do not demonstrate FAX480 signal reception or image quality; CAT and monitoring were disabled.

### SSTV RX configuration

The notch, FAX480 and four Radiofax configuration entry points now reside in `DecodiumBridgeSstvControls.cpp`; their bodies match v1.0.737. Both targets compiled. Two A/B application variants exercise enabling/disabling notch and FAX480, IOC 288/576, aligned and WMO profiles, invalid IOC and incompatible centered/WMO options; see `sstvconfig-{baseline,refactor}-{a,b}.txt`. The initial test used an incompatible centered/WMO combination and failed in both versions; the corrected test asserts rejection before checking valid WMO profiles. Fifteen SSTV control, Radiofax and QML regression tests passed. Application probes did not test image reception or RF and used no CAT/monitoring.

### AutoCQ recent-call deduplication

`DecodiumBridgeAutoCqDedupe.cpp` contains the unchanged recent abandoned/worked/logged checks and touch operation; two wrappers preserve the existing shared time-window helpers. Both app targets compiled. The A/B harness tests abandoned and worked expiry, logged calls, mode separation and sliding refresh with synthetic state; the helper windows and call parser are simulated in that harness. Two isolated application variants compare caller queue duplicate prevention and removal, without starting TX; see `autocqdedupe-{baseline,refactor}-{a,b}.txt`. These queue probes do not establish an on-air completed-QSO deduplication scenario. The first source-wiring regression still read the old file and failed; after updating its path, all three selected MAM/FT8/CQ regressions passed. CAT was disabled.

### Decode sample windows

The minimum and target sample-window methods moved to `DecodiumBridgeDecodeTiming.cpp`; their source matches v1.0.737. The A/B harness covers FT8, FT4, Q65/FST4, WSPR, JT and unknown modes using simulated period values. Both targets compiled and three selected decoder regressions passed. An FT8 two-instance BlackHole retry decoded `CQ 9H1SR JM75`, with first audio at 0.402 s in the slot; the earlier matching original baseline was 0.456 s. The immediately preceding refactored attempt decoded the message but was rejected by the timing guard because the first detected audio was at 13.495 s; it remains in `decode-samples-ft8-out-of-window.txt`. Results do not establish RF behavior or actual decoding in the other modes. CAT was disabled.

### Local DXCC and CALL3 data state

`DecodiumBridgeLocalDataState.cpp` contains the unchanged CALL3 search paths, CTY search paths and both metadata snapshots. Both QML and legacy targets compiled; the complete A/B suite compares the original code with the extracted bodies, allowing only linkage changes. Two isolated app variants compare stable metadata and paths for CTY and CALL3 in original and refactored builds; see `localdata-{baseline,refactor}-{a,b}.txt`. Three callsign/DXCC/settings tests passed. These probes do not download or replace user files, nor validate remote update services; CAT was disabled.

### Network preferences

`DecodiumBridgeNetworkPreferences.cpp` contains remote WebSocket port selection and UDP interface enumeration/read/write. Both targets compiled. A/B synthetic tests exercise active/configured/fallback ports, sorted interfaces, legacy fallback, setting and clearing the interface and requesting restart. The remote server and legacy store/restart are simulated; QSettings uses a temporary INI profile. Two isolated app variants compare read-only port/interface behavior with v1.0.737; see `networkprefs-{baseline,refactor}-{a,b}.txt`. Three selected settings/UDP regressions passed. No real UDP-interface change, remote connection or client restart was verified by the app probes; CAT was disabled.

Application persistence follow-up: `bridge_networkprefs_restart_ab.py` writes a synthetic UDP interface name, confirms it survives restart, clears it and confirms the cleared value after another restart. Both v1.0.737 and current method bodies passed in isolated apps. The legacy INI path is redirected to a temporary directory; the interface name is fictional, network is sandboxed, and successful UDP client operation on a real interface is not established. Both method versions compile in the extracted file.

That app A/B now also sets HTTP port 24001 and WebSocket port 25000 and confirms that the configured WebSocket port takes precedence and persists across all restarts in both versions. It does not open a WebSocket connection.

The same A/B first sets HTTP port 24001 and WebSocket port zero and verifies the fallback port is 24000. Both versions passed before testing explicit-port precedence and persistence.

### Decode period selection

The effective period and current-mode period getters now reside in `DecodiumBridgeDecodeTiming.cpp`. A/B source and synthetic tests cover standard modes and MSK144 legacy periods 5/10/15/30 s, invalid values, unavailable backend and mismatched mode. Both targets compiled; isolated app A/B variants verify FT2, FT4, FT8 and an allowed legacy MSK144 period. See `period-{baseline,refactor}-{a,b}.txt`. Three selected mode/decoder regressions passed. Simulated legacy period tests do not establish actual radio timing or all MSK144 backend configurations; CAT and monitoring were disabled.

### World map consumers

`DecodiumBridgeWorldMapConsumers.cpp` contains the unchanged feed-enabled, consumer-ready and consumer lifecycle methods. Both application targets compiled. The synthetic QObject A/B comparison covers null, duplicate and multiple consumers, removal, destruction and deferred replay scheduling with a simulated scheduler. Two isolated application variants compare registration, removal and destruction using a synthetic QML object; see `mapconsumers-{baseline,refactor}-{a,b}.txt`. The map-layer service and settings-profile restart regressions passed. The application probes do not verify actual LiveMap rendering, incoming contacts or replay; CAT and monitoring were disabled.

### FT8, WSPR and JT65 decode row parsers

`DecodiumBridgeDecodeRowParsers.cpp` contains the extracted row parsers. The A/B suite compares their bodies with v1.0.737 and executes both in the same Qt harness, covering valid FT8, WSPR and JT65 rows, malformed inputs, WSPR power/callsign rejection and frequency fallback. The existing UTC normalization function is unchanged and is reached through its existing forwarding function. Both application targets compiled and three selected FT8/WSPR regressions passed. A two-session BlackHole FT8 test decoded `CQ 9H1SR JM75`, with first detected audio 0.412 seconds into the slot; see `decoderows-ft8-refactor.txt` and the earlier original-build FT8 evidence. An earlier attempt also decoded the message but failed the slot-alignment guard and was excluded. At this stage WSPR and JT65 had harness coverage only. Later WSPR callback app A/B tests parsed synthetic rows (`wspr_callback/`), and forced-native recorded-WAV app comparisons decoded WSPR and JT65 reference signals in both Bridge placements; see the later WSPR reference and JT65 filter-correction entries. Those later tests do not establish normal legacy-mirror reception or RF. CAT was disabled.

### QSO search and cache

`DecodiumBridgeQsoCache.cpp` contains QSO count, cache invalidation and asynchronous warmup, search and statistics. The extracted methods and the shared snapshot/statistics helpers match v1.0.737 apart from linkage and existing forwarding calls. The A/B harness uses simulated snapshots and counts to exercise filtering, cached results, invalidation and warmup. Two synthetic logbook variants ran against both actual application builds: variant A covers edit, delete and selected export, while variant B additionally covers count, filters, statistics and a warmup request; see `qsocache-{baseline,refactor}-{a,b}.txt`. Both application targets and three selected settings/logbook regressions passed. The app tests do not exhaustively test large logs or concurrent mutation during warmup; CAT and monitoring were disabled.

### SSTV TX state and preflight

`DecodiumBridgeSstvTxState.cpp` contains the unchanged SSTV transmit readiness, preflight, diagnostics and state accessors. The A/B harness compares v1.0.737 and the extracted code with SSTV enabled and disabled, using simulated studio/coordinator/PTT states to exercise missing image, busy studio, multiple blockers, local readiness, diagnostics, active TX and TCI rejection. Both targets compiled and four SSTV state-machine, coordinator and studio regressions passed. Isolated original and refactored apps returned matching idle state and preflight diagnostics; see `sstvtxstate-{baseline,refactor}-{a,b}.txt`. Both variants observed the same idle state, not two distinct operating states. These probes did not transmit or test PTT hardware; CAT and monitoring were disabled.

### Target-call settings

`DecodiumBridgeTargetCallSettings.cpp` contains the unchanged target callsign, retry, timeout, period, pause and armed-watch setters. The A/B harness checks normalization, both bounds, duplicate notifications and temporary INI persistence. Two isolated app variants compare the original and refactored builds at opposite input limits, without starting a call; see `targetcall-{baseline,refactor}-{a,b}.txt`. Both application targets and two selected settings/FT8 regressions passed. Actual target-call operation and RF transmission were not exercised; CAT and monitoring were disabled.

### Target-call lifecycle

The start, stop, calling transition and no-QSO rearm methods also moved to `DecodiumBridgeTargetCallSettings.cpp`. The A/B harness compares their original and extracted bodies with simulated bridge state, TX requests and signal connections, covering empty target, Auto Call rejection, armed start, simulated calling, rearm and its cap, immediate calling and stop restoration. Two isolated app variants compare armed start/stop with TX disabled and different periods; see `target-lifecycle-{baseline,refactor}-{a,b}.txt`. Both application targets and two selected FT8/settings regressions passed. Timer expiry, actual decode-triggered calling, transmission and hardware PTT were not exercised end-to-end; CAT and monitoring were disabled.

### Target-call decode observation and retry

The target sender recognition, armed-watch trigger, TX-edge accounting and retry/pause methods now reside in `DecodiumBridgeTargetCallSettings.cpp`. Their source matches v1.0.737 apart from the logging forwarder. The A/B harness simulates decoded rows, callsign normalization, ghost suppression, TX and bridge operations; it exercises sender versus recipient, trigger, heard/reset, retry cap and a real event-loop pause/resume with simulated TX. Both app targets compiled and two selected regressions passed. An isolated app still enters and exits armed waiting with TX disabled, matching the earlier baseline (`target-decode-refactor-b.txt` and `target-lifecycle-baseline-b.txt`). The actual decode-to-call transition and hardware transmission were not exercised end-to-end; CAT and monitoring were disabled.

Target-call WAV follow-up: `bridge_targetcall_wav_runtime_ab.py` arms K1ABC, feeds an FT8 WAV containing its CQ to each isolated application and waits for the actual target-call state to switch from waiting to calling. Both placements passed; CAT and embedded rig control were disabled, immediate TX dispatch was suppressed, audio output was BlackHole and HALT left TX off. This exercises decoded-file-to-state integration, not a transmitted QSO or RF. Evidence: `bridge_refactor_evidence/target-call-wav-ab.txt`.

The `bridge_targetcall_decode_runtime_ab.py` follow-up invokes the actual
armed-watch entry on two synthetic FT8 decoded rows inside the app: a different
sender leaves the watch armed, while the target starts the calling state in
both placements. Immediate TX dispatch is explicitly suppressed, monitoring
and CAT are off, and no transmission occurs. Results:
`bridge_refactor_evidence/targetcall-decode-runtime-ab.txt`. This checks the
transition but does not drive a decoder with real audio or RF.
`bridge_targetcall_timeout_runtime_ab.py` additionally starts the real
10-second Target Call timer in isolated apps and confirms the calling state
ends in both placements, with immediate TX dispatch suppressed and CAT off.
Results: `bridge_refactor_evidence/targetcall-timeout-runtime-ab.txt`. Watchdog
priority, progressed-QSO and physical TX branches are not covered by this lab.

### Auto Call controls

`DecodiumBridgeAutoCall.cpp` contains Auto Call mode eligibility, enable/disable and SWR abort, limits, priority and counter reset. The source comparison and A/B harness cover unsupported mode, simulated SWR and active-QSO/target blocks, enabling, SWR abort, limits and temporary settings persistence. Two isolated app variants compare FT8 enable/disable and WSPR rejection with TX disabled; see `autocall-{baseline,refactor}-{a,b}.txt`. Both targets and two selected settings/FT8 regressions passed. Actual automatic selection of a CQ, physical SWR feedback and transmission were not exercised; CAT and monitoring were disabled.

### Frequency calibration and station offset

`DecodiumBridgeFrequencyCalibration.cpp` contains the calibration getters/setters, station offset lookup, CAT dial inverse and forward correction. The A/B harness uses simulated settings, bands and station records to compare the original with the extracted bodies, covering clamping, zero and nonzero calibration, transverter offsets, inverse mapping and invalid input. Both app targets and the profile restart test passed. Isolated original and refactored apps returned the same read-only zero calibration values (`freqcal-{baseline,refactor}.txt`). Actual band-model integration, nonzero legacy settings in the app and physical CAT tuning were not exercised; CAT and monitoring were disabled.

Nonzero application settings follow-up: `bridge_frequency_calibration_app_ab.py` compares v1.0.737 and current calibration method bodies in isolated applications. Both save and restore slope 12.5 ppm and intercept -45 Hz across restart and clamp out-of-range values to 200 ppm and -1000 Hz. The legacy INI path is redirected to a temporary directory and both method versions compile in the extracted file; actual CAT tuning and the normal user INI path are not exercised.

The same app test now adds a 20m station offset of 100 Hz and calls the actual forward and inverse calibration methods using the real Bands and StationList models. Both versions map 14,074,000 Hz to 14,074,230.92625 Hz and back, identifying 20m and the 100 Hz offset. This is a calculated value in an isolated app; no CAT command or radio tuning is performed.

### SWR and WSPR power protections

`DecodiumBridgeTxProtection.cpp` contains the SWR admission/stop and WSPR power-stop methods, using forwarding functions to the unchanged WSPR power helpers. The A/B harness compares the original and extracted methods with simulated telemetry and stop actions, covering disabled SWR checking, threshold boundary and clamp, SWR stop, WSPR safe/over-limit and throttling. Both targets and two selected settings/FT8 regressions passed. Isolated original and refactored applications expose matching read-only WSPR power options (`txprotect-{baseline,refactor}.txt`); these application probes did not trigger the protection. Real telemetry, physical PTT and an actual RF stop were not tested; CAT and monitoring were disabled.

Application SWR-admission follow-up: `bridge_swr_admission_app_ab.py` injects synthetic SWR readings into isolated apps and compares v1.0.737 and current protection method bodies. With protection disabled, high SWR is permitted; with a 2.5 threshold, 2.5 is permitted and 2.6 is blocked. Both variants passed with CAT and TX off. The test does not key a radio, supply real telemetry or verify interruption of active RF; both method bodies are compiled in the extracted file.

The app SWR A/B also verifies configured thresholds outside the allowed range are clamped to 1.5 and 5.0, with values at each boundary accepted and values just above rejected. Both variants passed on 2026-11-10 using synthetic telemetry.

Protection stop follow-up: `bridge_swr_admission_app_ab.py` now also executes the actual SWR and WSPR stop methods in both app placements with simulated TX/CW state and synthetic telemetry. Both stop above the configured limit and WSPR remains active at the tested limit. CAT and embedded legacy rig control were disabled; there was no RF or physical relay. Evidence: `bridge_refactor_evidence/swr-wspr-stop-app-ab.txt`.

The same app A/B now injects a synthetic active TX/CW state and high SWR, invokes the actual SWR enforcement method and verifies TX/CW state clears in both variants. CAT and monitoring are disabled and no audio or RF is started; the software-state result is not proof of a physical PTT release.

It also tests synthetic WSPR power enforcement: for a declared 30 dBm, 6 W remains allowed and 6.1 W clears synthetic TX/CW state in both method versions. These are injected software states and power readings, not a real RF power or PTT measurement.

### Legacy settings bridge

`DecodiumBridgeLegacySettings.cpp` contains the legacy INI path, configuration group selection, supported-key list and read/write methods. The original and extracted bodies match apart from logging and alias forwarding. The A/B harness uses a temporary INI, a simulated profile and a representative alias to check group precedence, writes to all expected groups, profile precedence and missing values. Both targets and two selected settings regressions passed. Isolated original and refactored apps returned matching read-only UDP and greyline settings (`legacysettings-{baseline,refactor}.txt`). Application-level writes across all legacy keys and radio behaviour were not exercised; CAT and monitoring were disabled.

### Station frequency records

`DecodiumBridgeStationFrequencies.cpp` contains station frequency list, add, update and delete methods, using a forwarder to the unchanged offset parser. The A/B harness uses simulated bands and storage but the original offset parser; it covers malformed input, Unicode minus, sorting, replacement, edits, deletion, failed persistence and invalid indices. A second A/B harness links the real Bands and StationList models and persists nonempty lists to a temporary QSettings INI (`stationfreq-real-ab.txt`); bridge state and the CAT offset lookup remain simulated. Both app targets and the settings-profile regression passed. Isolated original and refactored apps both read an empty station list (`stationfreq-{baseline,refactor}.txt`); nonempty persistence through the full app and CAT hardware were not exercised. CAT and monitoring were disabled.

The real-model station-frequency A/B was rerun on 2026-11-10 and passed all listed cases, including a simulated write failure. This rerun still uses a harness rather than the complete application, so it does not resolve the full-app persistence gap described above.

Full-app restart follow-up: `bridge_stationfreq_restart_ab.py` compiles the v1.0.737 station method bodies and the current bodies in turn, then writes a 20m/100 Hz/dipole row in an isolated application and reads the same row after restart. Both runs passed with CAT, monitor and TX disabled. The test redirects the legacy INI path to a temporary directory to protect real settings; both variants are compiled in the extracted source file, so this verifies the method versions and persistence in the app, not linker placement or the normal legacy INI path.

The restart test was expanded and passed for both versions through four application launches: add a station, edit its persisted offset and antenna, delete it after another restart, then confirm the deletion persists after a final restart. The same temporary INI-path and no-radio limitations apply.

The test now includes an additional read-only restart between editing and deletion and checks the edited 200 Hz offset and antenna explicitly. This five-launch sequence passed for both method versions on 2026-11-10.

### Satellite options

`DecodiumBridgeSatelliteOptions.cpp` contains satellite-file discovery and the mode list. The A/B harness compares the original and extracted methods using temporary `sat.dat` files, including duplicate and invalid rows, empty names and fallback when the preferred file exceeds the size limit. Both targets and the selected satellite half-duplex regression passed. The isolated applications reported the same satellite count and mode list (`satellite-options-{baseline,refactor}.txt`); the app comparison did not compare every satellite entry. CAT and monitoring were disabled.

### TCI audio

`DecodiumBridgeTciAudio.cpp` contains TCI transport selection, TX/tune stream start and stop, capture and PCM handling; the original code is preserved through logging, waveform and mode forwarding functions. The A/B harness uses a simulated CAT and audio sink to compare disconnected and connected gates, capture, FT8 and FT2-Link PCM handling, TX exclusion, empty/precomputed wave, tune and stop. Both app targets compiled, the audio mode-change policy regression passed, and the isolated app period smoke passed with CAT disabled. No real TCI server, radio or end-to-end TCI audio was exercised.

### RTL-SDR PCM and IQ samples

`DecodiumBridgeRtlSdrSamples.cpp` contains PCM and IQ sample handlers. The A/B harness uses a simulated RTL input and reduced IQ ring to exercise monitoring and demodulator gates, FT8/FT2-Link PCM delivery, TX exclusion, sample rate checks and ring wrap. Both app targets and two selected audio/FT8 regressions passed; an isolated application period smoke also passed. Actual RTL-SDR hardware, full-size ring behaviour under sustained traffic and end-to-end RF reception were not exercised. CAT and monitoring were disabled in the app smoke.

### RTL-SDR spectrum processing

The asynchronous RTL-SDR spectrum method also moved to `DecodiumBridgeRtlSdrSamples.cpp`. The A/B harness runs both original and extracted code with the real `RtlSdrRfSpectrum::compute` FFT and synthetic complex IQ, checking empty and busy gates, publication, frequency span and suppression after the view is disabled. Both app targets and the FFT and panadapter GPU recovery regressions passed. The bridge/output are simulated; actual RTL-SDR reception, the live panadapter and remote waterfall delivery were not exercised.

### RTL-SDR audio playback

`DecodiumBridgeRtlSdrPlayback.cpp` contains the listening audio start, stop and sample routing methods. The A/B harness uses a real Qt worker thread with simulated input and audio output, covering invalid rates, missing devices, audio/weak-signal/TX gates, device fallback, queued samples and stop. Signal connections and actual hardware playback are simulated. Both application targets, two audio/RTL regressions and an isolated application period smoke passed. Actual RTL reception, operating-system output, output error callbacks and delayed-stop recovery were not exercised; CAT was disabled in the app smoke.

### U.S. state and LoTW controls

`DecodiumBridgeUsStateLotw.cpp` contains the U.S. state display/update controls and LoTW enable, lookup and update triggers. The A/B harness uses simulated data manager and reload calls with temporary settings to cover persistence, repeated settings, offline gating, callsign lookup and update triggers. Both targets and two selected settings/logbook regressions passed. Isolated original and refactored apps returned matching read-only state (`uslotw-{baseline,refactor}.txt`). Real LoTW downloads, external services and complete dataset contents were not tested; CAT and monitoring were disabled.

### LoTW cache loading

The LoTW CSV helpers, asynchronous cache loader and network download method also moved to `DecodiumBridgeUsStateLotw.cpp`. The A/B harness uses actual temporary files, QtConcurrent and an event loop with simulated locations; it covers CSV deduplication, missing and existing cache offline, forced update offline and migration of a legacy cache. A second A/B harness simulates the network reply through Qt signals and verifies successful download, error with cached users, reported error without users and empty CSV. Both targets and two selected settings/logbook regressions passed; isolated app read-only state matches the earlier baseline. The remote LoTW service and production cache migration were not exercised.

Application cache follow-up: `bridge_lotw_cache_app_ab.py` loads a fresh synthetic CSV in isolated apps, verifies two case-insensitive known calls, deduplication and an unknown call, and compares v1.0.737/current control bodies. Both passed with CAT, monitoring and TX disabled and network sandboxed. The cache path is redirected to a temporary file; the real LoTW service and production cache are not exercised, and both control versions compile in the extracted file.

With `--empty-cache`, the same A/B checks a header-only CSV: both versions report the invalid-cache diagnostic, finish updating and expose no users while network access is sandboxed. This does not verify recovery using the live LoTW service.

After introducing the empty-cache option, the populated-cache A/B was rerun and still passed in both versions. The new LoTW, history, network and calibration scripts also passed Python syntax checks; their inspected unique Qt test profiles and temporary source markers were absent after execution.

An additional actual-app A/B test, `tests/tools/bridge_lotw_migration_runtime_ab.py`, creates a synthetic legacy `lotw_users.txt` beside a redirected shared cache, verifies byte-for-byte migration and checks case-insensitive lookups and an absent call. Both placements passed with CAT and TX disabled; results are in `bridge_refactor_evidence/lotw-migration-runtime-ab.txt`. The shared cache path is instrumented to a temporary directory, so this does not touch or prove migration of real user data.

Two additional app A/B scenarios compare multiple synthetic legacy candidates and an already-existing shared cache. The first selects the newer non-empty candidate; the second preserves the existing shared cache even when another candidate exists. Both placements passed and performed the same lookups. Logs: `bridge_refactor_evidence/lotw-newest-runtime-ab.txt` and `lotw-existing-runtime-ab.txt`. Both the shared path and profile candidate directory were redirected to temporary paths; real user files and the remote service were not used.

`bridge_lotw_cache_runtime_ab.py` now loads a synthetic CSV through the real
asynchronous app path in both placements. A test-only cache-path substitution
keeps the file in a temporary directory; both apps identify two callsigns,
reject an absent one and finish updating with CAT/TX off and network denied.
Results: `bridge_refactor_evidence/lotw-cache-runtime-ab.txt`. The app lab does
not test actual download, production cache migration or the external service.
`bridge_lotw_download_runtime_ab.py` additionally substitutes a loopback HTTP
URL and temporary cache path in both placements. The real app downloads a
synthetic CSV, writes identical bytes to cache, loads its callsigns and finishes
updating in both runs, with CAT/TX off. Results:
`bridge_refactor_evidence/lotw-download-runtime-ab.txt`. This does not test
external HTTPS or the production service.
`bridge_lotw_empty_download_runtime_ab.py` repeats the local-server comparison
with a header-only CSV. Both placements finish updating, report that no valid
users were present, and do not create user entries; the temporary cache contains
the received bytes. Results: `bridge_refactor_evidence/lotw-empty-download-ab.txt`.
`bridge_lotw_network_error_runtime_ab.py` first loads the temporary cache, then
receives HTTP 503 from the loopback server. Both placements report the failure,
retain the loaded callsigns and leave cache bytes unchanged. Results:
`bridge_refactor_evidence/lotw-network-error-ab.txt`.
The no-cache HTTP 503 path is also covered in the actual app by
`bridge_lotw_network_empty_cache_ab.py`: both placements report the error,
finish updating without users and leave the temporary cache absent. See
`bridge_refactor_evidence/lotw-empty-cache-error-ab.txt`.

### Decode depth and legacy hints

`DecodiumBridgeDecodeDepth.cpp` contains decode frequency bounds, depth and single-decode settings, FT4 adaptive depth limits and legacy depth/QSO hint conversion. The A/B harness covers temporary settings persistence, clamping, FT4 deep and latency-guard conditions, low-CPU handling and legacy bit conversion using simulated pressure inputs. Both targets and two selected FT8/legacy regressions passed. Isolated original and refactored apps exposed the same initial settings (`decode-depth-{baseline,refactor}.txt`). A subsequent two-app BlackHole FT8 comparison decoded `CQ 9H1SR JM75` in both versions with first audio at 0.446 s and 0.416 s respectively (`decode-depth-ft8-{baseline,refactor}.txt`); an earlier baseline attempt decoded the message but failed the slot guard and is retained separately. This establishes a virtual-audio FT8 case, not broader decode yield or live CPU-pressure adaptation. CAT was disabled.

### MAM controls and slot views

`DecodiumBridgeMamControls.cpp` contains MAM stream settings, DigiPileup controls, slot/payload views, manual slot/queue actions and sequencer availability. The A/B harness uses simulated slots and log actions to cover settings, view contents, slot logging/drop/clear, queue clearing and sequencer gating. Both app targets and the MAM payload/settings regressions passed; the source-inspection regression was updated to read the extracted module. Two isolated app variants matched at both setting bounds with TX disabled (`mam-controls-{baseline,refactor}-{a,b}.txt`). Active real multi-stream QSOs and RF transmission were not exercised; CAT and monitoring were disabled.

### External URL handling

`DecodiumBridgeExternalUrl.cpp` contains the unchanged URL validation and opening logic. The A/B harness replaces desktop services with a simulated opener to cover empty input, blocked schemes, HTTPS, mailto, local files and opener failure. Both targets compiled; isolated original and refactored apps both rejected empty and prohibited URLs (`external-url-{baseline,refactor}.txt`). Actual browser launches and the Linux AppImage helper/fallback path were not exercised; CAT and monitoring were disabled.

### Working frequencies

`DecodiumBridgeWorkingFrequencies.cpp` contains frequency parsers, default merging, row management, QRG/JSON import/export and preset QSY selection. Source is compared to v1.0.737 allowing only forwarding wrappers. The separate `bridge_workingfreq_real_ab.py` harness links actual Bands/FrequencyList models, uses temporary QRG/JSON files, and compares defaults, invalid input, add/update/preferred/delete/reset, round trips and corrupt-file rejection; CAT QSY and bridge settings are simulated. Both app targets, selected frequency/profile regressions and the general A/B suite passed. Isolated original and refactored apps returned identical canonical SHA-256 digests over all 196 rows and all mode, region and band options (`workingfreq-full-{baseline,refactor}.txt`). Full-app persistence and actual radio QSY were not exercised.

### Audio spectrum and waterfall frames

`DecodiumBridgeSpectrum.cpp` contains the existing radix-2 FFT, audio spectrum, remote waterfall quantization and completed-frame processing. The A/B harness uses actual FFT calculations and simulated remote output to cover an impulse and tone, insufficient input, empty band, quantization, invalid span, stale frame, TX gating and smoothing. Both app targets and two spectrum regressions passed; an isolated app period smoke also passed. Real remote waterfall and radio input were not exercised; CAT was disabled in the app smoke.

### High-resolution panadapter FFT

The high-resolution panadapter FFT is now in `DecodiumBridgeSpectrum.cpp` alongside its shared internal FFT. A/B harnesses execute both the FFTW and fallback implementations with a synthetic tone, testing the peak, levels, invalid band and repeated workspace; both targets and two spectrum regressions passed. An FT8 virtual-audio pair decoded the same message in the original and refactored apps with audio at 0.446 s and 0.413 s into the slot. Both receiver logs confirm the FFTW panadapter path ran (`panfft-app-{baseline,refactor}.txt`); a refactored attempt outside the slot guard is retained separately. Visual rendering, fallback use in the full app, real RF and remote waterfall were not verified; CAT was disabled.

### Cabrillo export

`DecodiumBridgeCabrillo.cpp` contains the unchanged Cabrillo export with forwarders to the original path helpers. The A/B harness uses temporary log files and compares the exact exported bytes via SHA-256, covering two QSOs, malformed lines, generic and custom contest headers, missing input and an invalid parent path. Both targets and selected ADIF/settings regressions passed. Isolated original and refactored applications both rejected an empty destination (`cabrillo-{baseline,refactor}.txt`); full export through the app and import into third-party contest software were not exercised.

Full-app export follow-up: `bridge_cabrillo_app_ab.py` compares the v1.0.737 and current method bodies in isolated application sessions. Both export the same two synthetic QSOs to identical files and skip a malformed line. The test redirects the input log path to a temporary file; CAT, monitoring and TX are disabled. It does not test third-party import or the normal user-log path, and both method versions are compiled in the extracted source file.

The full-app test now also checks the expected callsign, QSO dates, times and frequencies in the output. Both versions passed these assertions on 2026-11-10.

### Received files and file dialogs extraction

`DecodiumBridgeReceivedFiles.cpp` contains the unchanged file-dialog and FT2-Link received-file operations, compiled into both application targets. The A/B runner compares the complete extracted code against `v1.0.737` and exercises the original and extracted received-file helpers and asynchronous workers using temporary files. The worker harness substitutes the desktop-file-manager call; it does not verify a real file-manager window. The focused dialog test checks the source-level activation/ownership contract, not interactive native-dialog behaviour.

The isolated application probe `tests/tools/bridge_received_runtime_lab.py` compares the baseline and refactored application, saving text and binary files and checking their bytes on disk. Both sessions disable CAT and monitoring and use temporary profiles. No hardware or RF operation is implied by these results.

The expanded actual-app probe also concurrently saves two different contents under the same requested name, checks both unique files and bytes, and rejects invalid Base64 without creating a file. The checksum-verified 1.0.737 app and local refactor both passed; see `bridge_refactor_evidence/received-extra-{baseline,refactor}.txt`. The app builds use different Qt versions.

Directory dialog follow-up: `bridge_directory_dialog_ab.py` invokes the actual
directory selector in isolated before/after app sessions, programmatically
rejects one dialog and selects a temporary directory in another, and checks
both results. The Qt non-native dialog and offscreen platform are used so this
does not verify the operating system's native dialog or manual interaction.
CAT and TX were off. Evidence: `bridge_refactor_evidence/directory-dialog-ab.txt`.
The same A/B probe was extended to select a temporary directory containing
spaces and an accented character; both placements passed selection and
cancellation. Evidence: `directory-dialog-unicode-ab.txt` in the same folder.

File dialog follow-up: `bridge_file_dialog_runtime_ab.py` invokes the actual
open and save methods in isolated app processes in both source placements. It
checks cancellation, selection of an existing temporary text file and a save
path containing spaces and an accented character. Both placements passed;
results are in `file-dialog-runtime-ab.txt`. The first automation attempt
returned empty paths because `QFileDialog::selectFile` did not populate the
offscreen dialog's filename field; a standalone Qt control reproduced that
behavior, and setting the field made the test pass. This uses Qt's non-native
dialog and does not prove interaction with the OS native save dialog.

### Update checker extraction

`DecodiumBridgeUpdates.cpp` contains the original update-check method and its unchanged disabled-build flag. The A/B harness compares the method to `v1.0.737`, tests offline state preservation and the disabled branch including repeated calls, and does not contact GitHub. The isolated application probe `tests/tools/bridge_updates_runtime_lab.py` checks the actual invokable and emitted status against the baseline. Because the feature flag is false, the network-response branch remains unreachable in these tests; this extraction does not enable update checks.

### Embedded web-server bridge extraction

`DecodiumBridgeWebServer.cpp` contains the original token helpers and web-server bridge methods; the A/B runner checks their source against `v1.0.737` (apart from the forwarding name for the unchanged logger). `tests/tools/bridge_webserver_runtime_lab.py` runs baseline and refactored binaries with isolated test settings and CAT/monitoring disabled. Each actual server binds a temporary port and is exercised over loopback: unauthenticated API access returns 401, token-authenticated access returns 200, both advertised URLs are populated, stop and restart succeed, and the test token is reused. The server normally listens on all interfaces during the short test; this is not evidence of remote production access or a complete security audit.

### CPU and low-end profile extraction

`DecodiumBridgeCpuProfile.cpp` contains the FT thread, low-CPU, weak-computer, low-end and graphics-reset settings methods. The A/B comparison checks the moved code against `v1.0.737`, normalising only the forwarding functions for shared helpers and diagnostics. The C++ harness exercises limits, cycling, auto mode, low-CPU, low-end UI restoration and graphics reset with temporary QSettings and mocked runtime-profile collaborators. `tests/tools/bridge_cpuprofile_runtime_lab.py` compares actual isolated app sessions for FT thread limits, auto/manual mode and Low CPU; it deliberately does not change the real Mac's native global low-end settings. No physical radio or cross-platform execution is implied by these checks.

### FT2 partner memory extraction

`DecodiumBridgePartnerMemory.cpp` contains the unchanged resume-on-reply, FT2 partner-memory and one-shot TX2 resend methods; the A/B suite compares the implementation to `v1.0.737` apart from the logger forwarding name. The temporary-settings harness tests disabled and conservative gates, TX-in-progress rejection, state restoration without changing the current TX number, expired entry removal, pruning, watchdog thresholds, one-shot resend and disabling settings. Sequencer selection is mocked in this harness. The isolated application probe `tests/tools/bridge_partner_runtime_lab.py` compares setting toggles with CAT and monitoring disabled and verifies that the session is not transmitting. It does not establish real RF or live QSO behaviour.

### Lazy decoder-worker extraction

`DecodiumBridgeDecodeWorkers.cpp` contains the unchanged lazy worker creation method. The A/B suite compares the body with `v1.0.737`, normalising the existing canonical-mode and logger forwarding calls. `tests/tools/bridge_workers_real_ab.py` runs both method versions with real QThreads and substitute QObject decoders, checking all eight worker categories, JT worker sharing, duplicate creation and shutdown guard. `tests/tools/bridge_workers_runtime_lab.py` changes eight modes in actual isolated app sessions, with CAT and monitoring disabled, and compares the modes and worker-initialisation diagnostics. In this macOS legacy configuration the observed bridge-owned workers are Q65, WSPR, LegacyJT and FST4; the other modes cannot be claimed as real bridge-worker operation from this test. Neither test proves RF decoding.

### FT8/FT4 backlog and mode reset extraction

`DecodiumBridgeDecodeBacklog.cpp` contains the original FT8/FT4 backlog, early decode and mode-change reset methods. The full A/B suite compares the extracted methods to `v1.0.737`; `tests/tools/bridge_backlog_ab.py` runs both implementations with real Qt queued callbacks and substitute workers, checking thresholds, retention, cancellation, reset and buffer clearing. `tests/tools/bridge_backlog_runtime_lab.py` compares actual application mode transitions and their diagnostic reset messages with CAT and monitoring disabled; the macOS legacy configuration in this test does not establish cancellation of an actively decoding FT8/FT4 worker. `test_ft8_mycall_ap` also passed as a separate actual FT8 worker regression, but it is not a mode-change or RF proof.

### Active FT8 decode and mode-switch follow-up

`tests/tools/bridge_active_decode_switch_ab.py` runs the baseline and refactored app with `DECODIUM_FORCE_STANDALONE_UDP=1`, CAT and monitoring disabled, and a generated FT8 WAV. In both runs the app reported `decoding=true`, switched FT8 to FT4, and cleared that state. After switching back to FT8 and clearing the decode list, it decoded a **different** WAV containing `CQ 9H1SR JM75`. A separate A/B run of `bridge_wav_ft8_message_lab.py` decoded `CQ K1ABC FN42` in both app versions. This is a file-based modern-backend proof; it does not prove RF reception, the legacy backend's active-decoder path, or independently instrument the exact instant at which the underlying decoder stops.

### Decode-list clear extraction

`DecodiumBridgeDecodeClear.cpp` contains the two existing band-activity and RX-list clear methods. The A/B suite checks the bodies and shared key helpers against `v1.0.737`. `tests/tools/bridge_decodeclear_ab.py` compares both versions with a real temporary ALL.TXT and substitute legacy backend, models and station objects, covering legacy cursor updates, current-QSO keys, queue and timer cleanup and FT2-Link delegation guard. `tests/tools/bridge_decodeclear_runtime_lab.py` forces the modern backend in isolated CAT-disabled app sessions, decodes an FT8 WAV and checks that both clear calls leave their lists empty. The actual legacy clear path and an RF signal are not exercised by the app probe.

### Asynchronous DXCC and worked-history extraction

`DecodiumBridgeAsyncHistory.cpp` contains the original asynchronous startup loaders. The A/B harness `tests/tools/bridge_asynchistory_ab.py` uses the real Qt thread pool and temporary files with substitute DXCC/ADIF parsers, exercising chained loading, profile changes, file changes and stale serial rejection. The application probe `tests/tools/bridge_asynchistory_runtime_lab.py` compares the actual startup DXCC entity count and worked/QSO counters with CAT and monitoring disabled. A worked-history dataset already existed in the shared Qt test environment, so the probe observes its counts without assuming an empty log or modifying that dataset. It does not independently validate every real parser or profile-change race.

### CPU pressure and rendering guard extraction

`DecodiumBridgeCpuPressure.cpp` contains the pressure, FT4 guard, low-CPU runtime and render-stall methods. The A/B runner compares the implementation to `v1.0.737`, accounting for methods previously extracted to the decode-depth module and for forwarding names. `tests/tools/bridge_cpupressure_ab.py` compares both versions with real Qt timers and the real `Ft8MicroStallGuard`, while substituting audio/legacy workers and OpenMP controls; it exercises pressure counters, cancellation calls, guard deadlines, low-CPU limits and stall activation/recovery. Actual isolated app sessions in `tests/tools/bridge_cpupressure_runtime_lab.py` compare thread limits and render-visibility calls. Three focused CTests passed. Windows D3D11 behaviour is checked by source-level regression only; no Windows runtime or GPU-driver recovery was exercised.

### Decode context reset extraction

The mode/band context resets and `ALL.TXT` cursor initialisation now reside alongside decode clearing in `DecodiumBridgeDecodeClear.cpp`. `tests/tools/bridge_decodecontext_ab.py` compares both implementations with a real temporary cursor file and substituted legacy/model objects, covering mode change, band change, small QSY, cleared keys and missing files. `tests/tools/bridge_decodecontext_runtime_lab.py` compares real isolated app sessions: an FT8 WAV is decoded, a switch to FT4 clears the lists, and a different FT8 WAV decodes after switching back. The real-app probe forces the modern backend and disables CAT and monitoring; actual legacy and physical-band changes remain outside that proof.

### Secondary decode queue extraction

`DecodiumBridgeDecodeSecondary.cpp` contains the original legacy and native secondary queues. The A/B harness `tests/tools/bridge_secondary_ab.py` uses real Qt event processing with substitute collaborators to check ordering, duplicate suppression, mode filtering and side-effect routing across batches. The isolated app probe `tests/tools/bridge_secondary_runtime_lab.py` forces the modern backend, decodes an FT8 WAV and confirms that the CQ caller appears in the actual active-stations model, with CAT and monitoring disabled. Legacy queue side effects and real PSK Reporter, UDP, map and RF operation are simulated or outside the runtime proof.

### Legacy incremental decode-model extraction

`DecodiumBridgeLegacyDecodeDelta.cpp` contains the unchanged incremental legacy-model update and period-boundary helper functions. The A/B harness `tests/tools/bridge_legacydelta_ab.py` checks guards, order, separators, delayed updates and invalidation with substitute models. `tests/tools/bridge_legacydelta_realmodel_ab.py` repeats the comparison using the actual `DecodeListModel` and Qt timers; `test_decode_list_model` also passed.

The two-session `tests/tools/bridge_legacydelta_pair_lab.py` uses BlackHole 2ch/16ch, CAT disabled and synthetic callsigns. The refactored app received the CQ with the legacy backend active. The original timed out on its first attempt and received the CQ on a repeat; the exact reason for the first failure was not established. An earlier offscreen attempt did not start because the original bundle provides only the Cocoa platform plugin. These results demonstrate successful virtual-audio reception in both versions, with a non-deterministic first attempt; they do not prove hardware RF or isolate whether every legacy model delta branch executed.

### Legacy ALL.TXT path and append extraction

`DecodiumBridgeLegacyAllTxt.cpp` contains the existing legacy path selection and modern-decoder append methods. `tests/tools/bridge_legacyalltxt_ab.py` compares both implementations against temporary files with substituted path/legacy providers and the real message normalizer, covering embedded path priority, local fallback, formatted writes and TX/legacy guards. The isolated app probe `tests/tools/bridge_legacyalltxt_runtime_lab.py` checks that the selected path belongs to its unique Qt test profile, decodes an FT8 WAV with the modern backend and verifies the written CQ in `ALL.TXT`; CAT and monitoring are disabled. This does not prove real legacy-backend file writing or RF reception.

### Worked-before history extraction

`DecodiumBridgeWorkedHistory.cpp` now contains the worked-before index and DecoLog-link settings; the shared band/call key functions are declared in `DecodiumBridgeWorkedHistory.h`. `tests/tools/bridge_workedhistory_ab.py` compares the baseline and extracted behaviour for base calls, dates, band/mode, grid, DXCC, targeted row refresh and link configuration with substitute services. `tests/tools/bridge_workedhistory_runtime_lab.py` imports a synthetic ADIF record into an isolated logbook in each actual app, checks that the callsign appears in the worked index, then checks search/edit/delete and removes the synthetic logbook. CAT remains disabled. The focused B4 CTest also passed; the runtime probe does not connect to an external DecoLog service.

### PSK Reporter bridge extraction

`DecodiumBridgePskReporter.cpp` contains the existing reporter settings, station metadata and spot-selection/replay functions. `tests/tools/bridge_psk_ab.py` compares both implementations with a mock reporter and temporary settings, covering rounding, identity, partial/self filtering and replay freshness without network traffic. `tests/tools/bridge_psk_runtime_lab.py` compares time-window controls in actual isolated app sessions while reporting remains disabled. No PSK Reporter transmission or external-service interoperability was tested.

PSK replay application follow-up: `bridge_psk_replay_app_ab.py` invokes the actual replay method and reporter in isolated app sessions using synthetic recent, stale and partial decode entries. Both source placements queue only the recent complete call; the stale and partial calls are not queued. The test disables reporting immediately afterward, denies external networking in a sandbox and keeps CAT/TX off. Results: `psk-replay-app-ab.txt`. This tests local queue admission, not delivery to PSK Reporter or any external service.

### Station identity and generated messages

`DecodiumBridgeStation.cpp` contains station identity, grid, WSPR power and standard TX-message regeneration. The A/B source comparison normalises only forwarding helpers; `tests/tools/bridge_station_ab.py` exercises both method versions with substitute services and message helpers. `tests/tools/bridge_station_runtime_lab.py` compares actual generated FT8, custom CQ and WSPR messages in isolated app sessions with the modern backend, CAT and monitoring disabled, without transmitting. `test_custom_cq_persistence` passed after updating its source-location assertion. The legacy station-identity and moon-overlay collaborators are not independently exercised by the runtime probe.

### Decode persistence extraction

`DecodiumBridgeDecodePersistence.cpp` contains the original decode append and write-behind worker lifecycle. `tests/tools/bridge_decodepersistence_ab.py` compares both routing implementations using actual Qt queued calls and a substitute worker, including deduplication, TX exclusion, SSTV/keyboard gates and unresolved-data redaction. The macOS app probe `tests/tools/bridge_decodepersistence_runtime_lab.py` compares baseline and refactored sessions with CAT/monitoring disabled, verifies an FT8 WAV decode appears in each isolated SQLite database, and checks the corresponding session has an end timestamp after exit. It does not test database corruption, physical RF or every legacy decoder path.

### Decode flow and shared trace extraction

`DecodiumBridgeDecodeFlow.cpp` contains the original smooth-flow controls, list trimming and queued release. `DecodiumBridgeTraceScope.h` contains the shared main-thread tracing scope. The A/B runner compares the moved implementations to `v1.0.737`, normalising the forwarding helpers. `tests/tools/bridge_decodeflow_ab.py` compares the original and extracted methods with real Qt timers and isolated temporary settings, using substitute model, normalization and persistence collaborators; it covers row caps, retained RX/TX rows, queued release, persistence routing and the immediate flush when smooth flow is disabled. `test_decode_list_model` and `test_decode_list_reset_qml` passed. An isolated baseline/refactored application probe decodes an FT8 WAV and checks the visible decode and station model with CAT and monitoring disabled; it does not establish that the multi-row smooth queue was exercised in the application, nor does it test physical RF.

### Teleprinter decode rows and JTTY ALL.TXT

`DecodiumBridgeTeleprinter.cpp` contains the original RTTY/JTTY row creation and JTTY ALL.TXT writer. `tests/tools/bridge_teleprinter_ab.py` compares the methods with `v1.0.737` using actual Qt date handling and temporary file I/O, substituting the bridge's decode-list sink and path provider. It covers empty input, rounded audio frequencies, confidence, UTC conversion and RX/TX file lines. `tests/tools/bridge_teleprinter_runtime_lab.py` invokes the actual bridge slots in isolated original and refactored macOS application sessions and checks both decode rows and both ALL.TXT lines, with CAT and monitoring disabled; it cleans up only the unique test profiles it creates. This checks the bridge functions with synthetic text, not audio demodulation or physical RF.

### Diagnostic overlay metrics

The diagnostic frame-timing ring, cumulative decode counters and overlay timer have moved to `DecodiumBridgeDecodeMetrics.cpp`. `tests/tools/bridge_overlay_ab.py` compares the baseline and refactored code using real Qt timers and substitute bridge state; it checks the 32-sample ring, counter rates, backend fallback, reset and timer start/stop. `tests/tools/bridge_overlay_runtime_lab.py` compares real isolated macOS app sessions, toggles the overlay, supplies synthetic frames and a decode row, and observes the rate and reset with CAT and monitoring disabled. A first baseline probe sampled at a fixed 350 ms and missed the displayed rate; the repeat watched timer ticks and passed for both versions. It does not validate GPU timing accuracy or physical RF.

### FT8 known-CQ startup cache

`DecodiumBridgeKnownCq.cpp` contains the existing ALL.TXT cursor update and startup cache seeding logic. `tests/tools/bridge_knowncq_ab.py` compares the baseline and refactored methods with real temporary files and substitute FT8 seeding functions, checking missing files, cursor size, call extraction, source gating, age filtering and duplicate CQ replay. `tests/tools/bridge_knowncq_runtime_lab.py` creates a uniquely named Qt test profile with a synthetic ALL.TXT, starts each actual macOS application with CAT and monitoring disabled and checks its startup diagnostic reports the expected hash call. Replay is disabled in the app probe; the real Fortran cache contents and RF are not independently verified. Only the test profiles created by the probe are deleted.

### Audio input and output levels

`DecodiumBridgeAudioLevels.cpp` contains the RX/TX level accessors and setters, automatic input-level control and manual override logic. The A/B runner compares the moved bodies and shared conversion helpers to `v1.0.737`; `tests/tools/bridge_audiolevels_ab.py` exercises bounds, unchanged-value guards, automatic/manual transitions and routing to substitute audio and legacy sinks. `tests/tools/bridge_audiolevels_runtime_lab.py` checks the actual controls in isolated baseline and refactored macOS sessions, with CAT and monitoring disabled and no transmission. The application probe passes with both the modern and legacy backends active; actual gain delivered to hardware and RF output remain unverified.

### UI stall diagnostics

`DecodiumBridgeUiStall.cpp` contains the existing UI-stall detection and pressure-trigger methods. `tests/tools/bridge_uistall_ab.py` compares the original and extracted bodies with real elapsed timers and substitute pressure/logging collaborators, covering hidden-window reset, repeated short stalls, severe stalls and inactive-monitor gating. `tests/tools/bridge_uistall_runtime_lab.py` induces a 1.2-second event-loop stall in each isolated macOS app and confirms a diagnostic gap of at least 1100 ms, with CAT and monitoring disabled and no TX. The first attempts checked the wrong log destination; Decodium opens its diagnostic log at the shared application path before the test identity is set. The successful probe reads only newly appended bytes there and does not delete that shared log. Actual pressure activation while receiving or transmitting and physical RF are not exercised in the app probe.

### QSO log preview and comments

`DecodiumBridgeLogPreview.cpp` holds the existing QSO preview and automatic comment selection. `tests/tools/bridge_logpreview_ab.py` compares baseline and extracted logic using substitute report, time, settings and distance helpers, covering live, pending and snapshot state, report fallback, date ordering and comment policies. `tests/tools/bridge_logpreview_runtime_lab.py` compares actual isolated macOS app sessions for a synthetic callsign, grid, mode, default comment and manual comment, with CAT and monitoring disabled and without logging or transmitting. A first default-comment attempt failed identically in both apps because it did not explicitly select the default-comment policy; the repeated test disabled the competing policies and passed. The app probe does not cover every snapshot/contest branch or a real QSO commit.

### Automatic RX input level

`DecodiumBridgeAutoRxLevel.cpp` contains the original automatic RX gain decisions. The A/B runner compares the function and conversion helpers to `v1.0.737`; `tests/tools/bridge_autorx_ab.py` uses deterministic samples and timestamps with a substitute level setter to cover clipping, cooldown, manual hold, silence, low-signal rise and caps. `tests/tools/bridge_autorx_pair_lab.py` runs two CAT-disabled macOS instances connected through BlackHole, confirms that the virtual sender enters TX and that the receiver keeps automatic level enabled and reduces its initially high gain. The original and refactored versions both passed. This is a virtual-audio proof of the gain-reduction path, not physical RF or an exhaustive proof of every adaptive branch; only uniquely named test profiles are removed.

### RX audio health

`DecodiumBridgeAudioHealth.cpp` contains the original RX stream health classification, warning and watchdog gating. `tests/tools/bridge_audiohealth_ab.py` compares baseline and extracted logic with substitute capture, mode and callback collaborators, covering inactive capture, quiet FT2-Link/long synchronized modes, unhealthy/overdriven input and the TX hold. Both original and refactored macOS apps also passed `tests/tools/bridge_autorx_pair_lab.py`: a CAT-disabled sender and receiver exchange virtual audio via BlackHole, the sender enters TX and the receiver reduces gain through the health callback. The pair test does not independently prove the warning text or all watchdog branches in the real app, nor physical RF.

### RX capture recovery on mode changes

`DecodiumBridgeAudioRecovery.cpp` holds the existing mode-change capture ownership and delayed monitor recovery methods. `tests/tools/bridge_audiorecovery_ab.py` compares original and extracted code using real Qt timers and substitute audio/legacy collaborators, covering retained capture, exclusive legacy handoff, stale reopen and monitor rearming. The focused `test_audio_mode_change_capture_policy` passed after updating its source-location assertion. In `tests/tools/bridge_audiorecovery_pair_lab.py`, original and refactored macOS receivers switched FT8 → FT4 → FT8, stayed in monitor and reduced gain on audio sent by a second CAT-disabled instance over BlackHole. Both the standalone and legacy receiver configurations passed; legacy activation was explicitly checked for the latter. These are virtual-audio checks, not physical RF or a full verification of all platform audio backends.

### RX audio watchdog recovery

The existing `restartAudioCaptureFromWatchdog` method now resides in `DecodiumBridgeAudioRecovery.cpp`. `tests/tools/bridge_watchdog_ab.py` compares baseline and extracted code using real Qt timers and substitute capture, RTL-SDR and legacy backends; it exercises FT2-Link guards, TCI cooldown, RTL and legacy routing, safe recovery, stale timer cancellation and the platform-specific no-callback path on the host. The refactored app also passed the legacy two-session BlackHole mode-change reception probe described above, compared with its recorded baseline; this checks normal RX continuity, not activation of the actual watchdog. Actual watchdog recovery with a failing device, Linux PipeWire and physical RF remain unverified.

### RTTY exit recovery wiring

`scheduleRttyExitRxRecovery` now resides in `DecodiumBridgeAudioRecovery.cpp`. `tests/tools/bridge_rttyexit_ab.py` compares the baseline and extracted method using the real `RttyRxRecovery` controller and Qt timers with substitute audio and radio collaborators, covering local and legacy recovery and cancellation for remote/RTTY sources. `test_rtty_rx_recovery` passed. `tests/tools/bridge_rttyexit_pair_lab.py` compares original and refactored macOS apps with legacy RX: after RTTY → FT8, monitoring remains active and the receiver responds to audio from a second CAT-disabled app over BlackHole. The app probe does not independently inspect each recovery-controller callback or physical RF.

### Audio capture stop and teardown

`DecodiumBridgeAudioCaptureStop.cpp` contains the original capture stop and teardown methods, including their macOS diagnostic guard. `tests/tools/bridge_audiostop_ab.py` compares baseline and extracted code using real Qt thread dispatch and substitute input, sink, RTL and TCI objects; it checks cancellation serials, source-specific stop paths and same-thread/worker-thread teardown. `tests/tools/bridge_audiostop_pair_lab.py` compares original and refactored macOS apps: a CAT-disabled receiver stops and restarts monitoring, then responds to audio sent through BlackHole by a second isolated app. This exercises normal virtual capture continuity, not physical RF or all platform-specific audio teardown paths.

### Idle audio buffers and pending MAM payload

`DecodiumBridgeAudioBuffers.cpp` contains the existing TX cache invalidation, pending MAM cleanup and idle audio-buffer release. `tests/tools/bridge_audiobuffers_ab.py` compares original and extracted methods using real Qt containers, mutex and timer with substitute bridge state; it covers active-TX protection, active-RX and pending-data preservation, timer bounds and MAM invalidation. `test_mam_tx_payload_policy` passed after updating its source-location assertion. `tests/tools/bridge_audiobuffers_pair_lab.py` compares original and refactored macOS sessions: the receiver stops and restarts monitoring, waits beyond the nominal idle-release delay, then responds to a second app's BlackHole audio. The app probe does not directly observe the timer callback or MAM payload clearing; it is a virtual-audio continuity check, not physical RF.

### TX audio scheduling

`DecodiumBridgeTxScheduling.cpp` contains the existing spectrum-timer coordination and Windows TX audio process-priority boost/restore. `tests/tools/bridge_txscheduling_ab.py` compares the baseline and extracted methods on the host and with substitute Windows priority APIs, including failed boost/restore and retry; the simulated API is not a Windows runtime test. `tests/tools/bridge_txscheduling_pair_lab.py` compares two CAT-disabled macOS sessions on BlackHole for each version: an FT8 sender transmits and a legacy receiver decodes its CQ. This observes virtual transmission and reception but does not measure spectrum timer activity, Windows scheduling or physical RF.

### TX slot timing and FT2 asynchronous retry

`DecodiumBridgeTxTiming.cpp` now contains the existing synchronous-slot guards, FT2 safe-delay calculation and asynchronous retry scheduler alongside the previously extracted PCM offset routine. The A/B runner compares both groups and their shared helpers against `v1.0.737`; `test_ft8_auto_tx_decode_gate` also passed after updating its source-location count. `tests/tools/bridge_txtiming_ab.py` exercises alignment, late-slot decisions, retry and stale-request guards, and phase-lock avoidance with a deterministic clock and substitute TX callbacks. Original and refactored macOS applications exchanged FT8 and FT2 CQ messages over BlackHole with CAT disabled and a legacy receiver. The first refactored FT2 attempt timed out without a decode; a repeat succeeded, while the original succeeded on its first attempt. The cause of that variability was not established. The virtual pair does not prove every late-slot or phase-lock branch ran, and is not physical RF.

On 2026-11-10 `test_ft8_auto_tx_decode_gate` was updated to read the extracted UTC timer module; it and three related source regression CTests passed: `bridge_refactor_evidence/bridge-source-regressions-20261110.txt`. This fixes test source assembly, not production TX behaviour.

The controlled timing A/B harness additionally checks the exact synthetic deadline: 999 ms accepted, 1000 ms late, and the next 15-second boundary accepted. Both source versions pass; `bridge_refactor_evidence/tx-timing-boundaries-ab.txt`. The harness injects a 1000 ms deadline and is not a measurement of an actual radio relay or FT8 audio duration.

### FT2 timing follow-up

The prior intermittent FT2 pair test used a fixed ten-second delay after process startup, which can place the manual send after the 680 ms safe-start deadline. In one refactored failure the shared diagnostic log recorded a request at 2519 ms, rejected as late; the instrumented sender never entered TX. `tests/tools/bridge_ft2late_runtime_lab.py` deliberately requests TX late and confirms the same outcome in both original and refactored macOS apps: the status reports a deferred FT2 transmission, TX enable is false and no TX occurs during the next five seconds. This is observed existing behaviour, not proof of a refactoring regression. `tests/tools/bridge_ft2timing_pair_lab.py --safe-slot` requests TX near the beginning of a slot, observes the sender enter TX and confirms decoding in both versions over BlackHole. The log is shared with the application and was preserved; these virtual tests do not establish physical RF behaviour or diagnose all possible FT2 timing failures.

### Automatic QSO log snapshots

`DecodiumBridgeAutoLogSnapshot.cpp` contains the original partner inference, pending snapshot and late-snapshot lifecycle. `tests/tools/bridge_autologsnapshot_ab.py` compares original and extracted logic with substitute callsign, report and distance collaborators, covering partner precedence, transmitted-versus-received reports, grid fallback and late expiry. `test_mam_tx_payload_policy` passed after its source-location assertion was updated. `tests/tools/bridge_logpreview_runtime_lab.py` also compares the actual application preview including the extracted effective-QSO-time accessor in isolated baseline/refactored sessions; it does not trigger real automatic snapshot capture or commit a QSO. Physical RF and complete automatic QSO lifecycle remain outside this runtime proof.

### Automatic snapshot application follow-up

`tests/tools/bridge_autologsnapshot_runtime_lab.py` now exercises automatic snapshot capture in the actual original and refactored macOS applications: a CAT-disabled standalone sender plays a synthetic FT8 TX5 to BlackHole, the completion requests the QSO confirmation, and the probe verifies call, grid, received report and preservation after the current call changes. The probe enables confirmation before transmitting, rejects the prompt and checks the synthetic call was not written to the isolated ADIF log. Both versions passed. This validates the TX5-to-prompt path, not a complete two-way automatic QSO or physical RF.

Prompt snapshot extraction: `bridge_promptsnapshot_ab.py` compiles the original and extracted capture/clear functions and compares pending, late and fallback timestamps, field trimming and clearing. `bridge_autologsnapshot_runtime_lab.py` exercises the actual automatic TX5 prompt and rejection using isolated settings and virtual audio; it does not prove prompt acceptance, a complete two-way QSO or RF behaviour.

Log prompt lifecycle extraction: `bridge_promptlifecycle_ab.py` compiles both baseline and extracted functions against the same Qt event-loop harness and checks manual TX guard, empty call, prompt notification, fallback, acceptance, rejection, call precedence and direct logging. The live `bridge_autologsnapshot_runtime_lab.py` comparison verifies actual automatic prompt and rejection using virtual audio and isolated settings; acceptance and native fallback are covered by the harness, not a real QSO or physical radio.

Audio device cache access extraction: `bridge_audiocacheaccess_ab.py` compares original and extracted accessors and scheduling using a Qt event loop and simulated devices, including stale and synchronous refresh, defaults, timer and shutdown. `bridge_audiorecovery_pair_lab.py --legacy-rx` exercises audio device selection and RX recovery with two actual local sessions over BlackHole; this is virtual audio evidence on macOS, not physical RF or Linux/Windows runtime coverage.

Audio input selection extraction: `bridge_audioinputselection_ab.py` compares baseline and extracted selection with simulated devices, including names, identifiers, defaults and simulated Linux Pulse success/failure. The legacy-RX two-session BlackHole lab exercises input selection and audio recovery in actual macOS applications; simulated Linux paths do not establish real PipeWire or Linux runtime behaviour.

Audio device cache refresh extraction: `bridge_audiocacherefresh_ab.py` compares original and extracted refresh using simulated device changes, cache invalidation, notification suppression, diagnostics and Linux Pulse list additions. The legacy-RX two-session BlackHole test supplies macOS application-level evidence; the simulated Linux branch is not a Linux runtime test.

Audio cache helpers: `bridge_audiocachehelpers_ab.py` compares baseline and extracted device-description de-duplication and cache-signature generation with simulated names and identifiers. The refresh harness is rerun and the two-session legacy-RX BlackHole lab checks the integrated macOS audio path; no RF or other operating-system runtime is implied.

Decode model filter extraction: `bridge_decodemodelfilter_ab.py` compares the baseline and extracted filtering, partner membership, ordering and period separators with controlled collaborators. The application WAV decode-and-clear lab checks the real decoding flow in both versions; it does not independently inspect every model row or establish RF behaviour.

Decode visibility extraction: `bridge_decodevisibility_ab.py` compares the original and extracted methods using the real worked-filter policy with controlled ghost and telemetry predicates. It covers worked-band, CQ preservation, TX, bypass, ghost, telemetry, display text, empty row and yesterday. The isolated WAV decode-and-clear lab provides application-level evidence of a successful decode, not a visual verification of every filtering option.

Decode visibility app follow-up: `bridge_decode_visibility_app_ab.py` invokes both actual visibility overloads in isolated apps with the real telemetry classifier and temporary settings. Both source placements show a normal CQ, suppress a synthetic telemetry-only message and a worked-band entry, and restore the worked entry under filter bypass. CAT/TX and external networking are disabled. Results: `decode-visibility-real-app-ab.txt`. This checks decisions in the running app, not visual rendering or RF.

Decode UI scheduling extraction: `bridge_decodeuischedule_ab.py` compares original and extracted timer scheduling, coalescing, pending flags, cache invalidation and UI workload limits with a Qt event loop. The isolated WAV decode-and-clear application test checks the actual decode flow but does not independently verify every rendered model update.

Full Spectrum snapshot extraction: `bridge_fullspectrumsnapshot_ab.py` compares timer deferral, burst handling, model handoff and absent-model behaviour in an event-loop harness. `bridge_fullspectrum_runtime_lab.py` decodes the same WAV in baseline and current isolated applications and verifies that Band Activity and Full Spectrum models contain rows; this is not an RF test or a pixel-level visual comparison.

Decode UI notifications extraction: `bridge_decodeuinotifications_ab.py` compares the baseline and extracted Qt timer coalescing, RX normalization flags and reentrant band notifications. The isolated WAV application lab checks that Band Activity and Full Spectrum models become populated in both versions; it does not verify all visual updates or RF reception.

Decode UI snapshot extraction: `bridge_decodeuisnapshot_ab.py` compares the original and extracted worker on synthetic classified rows, ghost and telemetry filtering, ordering, separators, deduplication and RX clearing with controlled normalization collaborators. The full-spectrum WAV lab compares actual baseline/current application decoding and model population; it does not prove every stale-generation race or RF reception.

Legacy ownership extraction: `bridge_legacyownership_ab.py` compares baseline and extracted selection for FT8, Q65, RTTY, WSPR, JT65, FT2-Link, remote and RTL sources, backend availability, dual audio and SSTV, with macOS/non-macOS branches simulated. The two-session BlackHole test checks actual macOS legacy RX recovery; simulated platform branches do not establish Windows or Linux runtime behaviour.

Legacy refresh extraction: `bridge_legacyrefresh_ab.py` compares the original and extracted backend guards, timer startup, burst timings and queued reentrancy with a Qt event loop. The legacy-RX BlackHole two-session test checks actual macOS audio recovery and mode changes; CAT and physical RF remain outside that test.

Legacy dialog synchronization: `bridge_legacydialogsync_ab.py` compares baseline and extracted ordered backend calls with a recording substitute, covering station identity, audio channel clamping, FT2-Link, RTTY, Q65 and Hound. The two-session BlackHole legacy-RX lab checks actual macOS reception and mode transitions; the simulated backend test does not command physical CAT or verify RF transmission.

Legacy RX handoff extraction: `bridge_legacyrxhandoff_ab.py` compares guards, normal and dedicated capture handoff, session invalidation and refresh scheduling with controlled collaborators. Focused policy and GPU-recovery tests check source contracts. The two-session BlackHole legacy-RX lab checks macOS audio/mode recovery; it is not physical CAT, RF or GPU pixel-level proof.

Legacy PCM recovery extraction: `bridge_legacypcmrecovery_ab.py` compares original and extracted capture ownership, SSTV guard, TX deferral and timed retry with controlled collaborators. Focused audio policy/GPU source-contract checks and the two-session legacy-RX BlackHole lab provide further evidence; these do not establish real RF, CAT or GPU visual output.

The controlled A/B harness now additionally verifies that a scheduled retry does not rearm when fresh PCM has arrived or while TX is active. Both source versions produced the same expected result; `bridge_refactor_evidence/legacy-pcm-recovery-expanded-ab.txt`. These are simulated state transitions, not actual hardware recovery.

Decode shutdown extraction: `bridge_decodeshutdown_ab.py` compares callback guards, worker shutdown, timer stop and CAT restoration ordering with simulated objects; the WAV application lab confirms both versions decode, populate models and exit normally. `bridge_active_decode_exit_lab.py` additionally requests application exit after observing the decoding property active and verifies exit before the lab timeout in both versions. This does not prove the exact worker instruction was active at the instant of exit or control a real CAT device.

Local QSY guards: `bridge_qsyguard_ab.py` compares original and extracted guard decisions using simulated frequencies, including stale reports, target confirmation, manual tuning, expiry and legacy RX frequency. The legacy-RX BlackHole pair checks general application audio continuity, not the actual CAT/QSY guard integration; no physical radio commands are sent.

`bridge_qsyguard_runtime_lab.py` also compares actual baseline/current local QSY updates to 14.074 and 7.074 MHz with CAT disabled and no TX. It does not inject CAT frequency reports, so stale-report integration remains established only by the simulated guard harness.

Visual feed scheduling: `bridge_visualfeedscheduling_ab.py` compares original and extracted queue flushing, two-row chunks, readiness and TX-deferred cluster notifications using controlled consumers and a Qt timer. `bridge_visualfeed_runtime_lab.py` decodes an isolated WAV and observes the K1ABC world-map contact signal in baseline and current applications; it does not compare rendered map pixels or connect a live DX cluster.

World map queue extraction: `bridge_worldmapqueue_ab.py` compares original and extracted DXCC enrichment, queue bounds, duplicate filtering, unresolved entries and TX deferral with controlled services. The isolated WAV application lab observes the same K1ABC map contact signal in both versions; it does not check map rendering or live radio.

World map reset extraction: `bridge_worldmapreset_ab.py` compares entry-key fallback, freshness, exclusion of TX rows and queue reset with controlled entries. `bridge_worldmapreset_runtime_lab.py` observes a WAV-derived contact followed by the actual reset signal and empty decode list in both applications; it does not compare map rendering.

World map replay extraction: `bridge_worldmapreplay_ab.py` compares original and extracted asynchronous replay, duplicate suppression, stale generations and TX deferral with controlled consumers. The application lab decodes a WAV, disconnects/reconnects a map consumer and observes the same contact a second time in both versions; it does not test visual pixels or RF.

World map QSO path extraction: `bridge_worldmapqso_ab.py` compares original and extracted path emission, closure, reopening, own-call guard and grid fallback using controlled collaborators. The WAV application lab verifies map contact delivery, not a complete live QSO path or RF.

World map grid extraction: `bridge_worldmapgrid_ab.py` compares original and extracted CALL3 parsing, asynchronous cache merge, remembered-grid priority and coordinate fallback with a temporary fixture and controlled DXCC lookup. The isolated WAV application test observes a map contact signal, not every CALL3 or live RF scenario.

World map entry replay extraction: `bridge_worldmapentry_ab.py` compares original and extracted CQ, directed traffic, 73 closure, deferred delivery and filtering with controlled collaborators. The WAV application lab observes the same CQ contact signal on both versions; it is not a real QSO or a visual comparison.

World map selection extraction: `bridge_worldmapselection_ab.py` compares single/double click, roster arming, delayed slot dispatch and cancellation with simulated TX controls. The application lab selects one synthetic contact with CAT disabled and confirms TX remains off in both versions; it does not test a real transmission.

Legacy ADIF mirror extraction: `bridge_legacyadifmirror_ab.py` compares the original and extracted import, duplicate handling, write failure and active-QSO suppression using an in-memory substitute. The isolated synthetic ADIF application test exercises import/export/edit/delete, but does not invoke the legacy mirror callback itself or send a QSO externally.

The controlled A/B harness now also submits whitespace-only and malformed input, and verifies a lowercase duplicate does not add a record or emit additional change events. Both placements pass; `bridge_refactor_evidence/legacy-adif-mirror-expanded-ab.txt`. Parsing and storage remain substitutes in this test.

Legacy ADIF mirror real-file follow-up: `bridge_legacyadifmirror_realfile_ab.py` compiles the baseline/current mirror methods against the actual ADIF parser, sanitizer and atomic file writer, using a unique temporary directory. It verifies two synthetic records, duplicate suppression, active-QSO detection and a failed write; bridge services remain controlled substitutes and no real log or external service is touched.

FT2 Log Bridge control extraction: `bridge_ft2logbridge_ab.py` compares status, asynchronous lookup, simulated launch and UDP configuration warnings without starting external processes. The isolated app lab verifies status availability, disabled autostart and no running bridge; it does not test an actual external client or UDP transmission.

Decode alerts extraction: `bridge_decodealerts_ab.py` compares original and extracted alert priority, cooldown and wanted-call matching with isolated QSettings and simulated playback. The WAV application lab checks the decode pipeline but does not verify audible output.

Panadapter registration extraction: `bridge_panregistration_ab.py` compares original and extracted multiple-target registration, duplicate prevention and QPointer cleanup. The isolated WAV application lab checks that a panadapter registers and decoding populates models; it does not verify the rendered waterfall pixels or physical RF.

Station telemetry scheduling extraction: `bridge_stationtimer_ab.py` compares the baseline and extracted implementation with real Qt timers and controlled clock, TX state and send callback. It covers even/odd slots, fallback, forced delay, repeated arming and suppression while transmitting or cancelled. The isolated application A/B lab invokes telemetry with an invalid grid and verifies rejection and TX off; this configuration rejects immediately and does not exercise the deferred callback in the running application. The WAV decode lab checks general application operation. No CAT commands or RF transmissions are made; live telemetry transmission remains unverified by these labs.

Station telemetry scheduling follow-up: the isolated application A/B lab now enables virtual BlackHole input, selects MSK144 and requests telemetry outside its TX period with an invalid grid. In both versions the diagnostic log records deferral and then rejection of the invalid grid. CAT is disabled and the request observes TX off. The app has more than one pending-telemetry recheck path, so these observations do not isolate the timer callback from every other recheck; the controlled Qt timer harness covers that callback.

Decodium RX terminal launcher extraction: `bridge_rxterminal_ab.py` compiles both versions against real Qt process and filesystem APIs in temporary directories. It checks executable absent, terminal absent, a fake gnome-terminal launch and xterm fallback with the expected arguments. The isolated application A/B lab compares availability to the executable actually installed beside each binary, without launching it. Windows console creation flags are source-compared but not runtime-tested on Windows; no real terminal or RX application is launched by the harness.

CW monitor settings extraction: `bridge_cwsettings_ab.py` compares the original and extracted getters, setters and output enumeration with isolated real QSettings, real Qt device enumeration and a controlled sidetone object. It verifies defaults, clamping, persistence, device decoding, clear and change notifications. The two isolated application runs compare the QML-visible settings and default output entry with CAT and monitoring disabled. They do not play an audible tone or verify a real output device.

RX recording control extraction: `bridge_rxrecording_ab.py` compares start, stop, no-manager, already-active and simulated start-failure paths using a controlled WavManager. The isolated application A/B lab starts and stops the actual recorder and inspects the generated PCM mono 12 kHz WAV header in Qt test Documents; it removes only its own newly created file. Monitoring and CAT are disabled, so this proves file creation and control, not recorded input sample content or real RF.

RX recording audio follow-up: `bridge_rxrecording_audio_lab.py` runs each application with CAT disabled and BlackHole input, plays a synthetic 1 kHz tone into the virtual device, and checks the recorded 12 kHz PCM WAV contains at least one second of nontrivial audio. The generated test WAV and isolated profile are removed afterwards. This is virtual-audio evidence, not reception over physical radio.

CW monitor settings audio follow-up: `bridge_cwsettings_audio_lab.py` selects BlackHole 2ch through the application output list and captures that device while invoking the CW test with the monitor enabled and disabled. Both baseline and current produce a detectable tone only in the enabled case, with CAT disabled and TX off. This proves virtual-output audio, not acoustic playback or RF.

CW monitor test extraction: `bridge_cwtest_ab.py` compares disabled, transmitting, already-pending, failed playback and successful playback paths, including the Qt timer releasing the admission guard. The application audio lab compares baseline/current audible output on BlackHole with the monitor enabled and silence when disabled; CAT is disabled and TX is off. It does not test physical speakers or RF.

CW monitor playback extraction: `bridge_cwplay_ab.py` compares the enable guard, sidetone creation, device decoding, volume, event sequence, frequency, reuse and failed enqueue with a controlled sidetone. The actual application audio lab compares the baseline and extracted implementation through BlackHole for enabled/disabled CW test playback with CAT disabled and TX off. This does not verify physical speakers or RF.

Decode indicator extraction: `bridge_decodeindicators_ab.py` compares update, clamping, turbo threshold, duplicate-notification suppression and reset with controlled signal counters. The isolated application A/B lab invokes the actual bridge slots and verifies all six QML-visible indicator values before and after reset, with CAT and TX disabled. It does not assert visual LED pixels or independently trigger every decoder worker.

Station telemetry send extraction: `bridge_stationsend_ab.py` compares tuning and expired-request guards, sync and FT2 deferral, invalid grid, fresh/disabled weather, real codec round trip and unsuccessful TX cleanup using controlled clock-adjacent state and simulated TX. The isolated application A/B lab checks real slot deferral followed by invalid-grid rejection on BlackHole input, with CAT disabled and TX off. It does not prove real telemetry RF transmission, cross-thread dispatch, or live weather retrieval.

Station weather extraction: `bridge_stationweather_ab.py` compares offline/disabled/invalid-grid guards, request coordinates, synthetic network failure, malformed JSON, parsed weather and expiry using real Qt JSON and the telemetry codec with a controlled network substitute. The isolated application A/B lab verifies that a disabled weather service returns no preview and does not transmit. It does not test a live weather service or a successful network response within the actual app.

Startup latency investigation and fix: `bridge_startup_order_ab.py` compares real Qt timer ordering, startup failure fallback and shutdown using controlled backend collaborators. `bridge_startup_legacy_order_lab.py` deliberately stalls the QML bootstrap in isolated baseline/current app sessions: baseline starts legacy initialization before the main window, whereas current waits until main QML is ready. `bridge_startup_visual_ab.py` compares staging guards for idle, monitoring, decoding, TX, tuning and FT8 safe slots. `bridge_startup_measure_lab.py` records baseline/current app timings; both use test profiles, no CAT, no monitor and disabled QML disk cache, so these timings are not a measurement of the operator's full profile. A two-session BlackHole legacy-RX audio recovery/mode-change A/B passed. A further two-session live FT8 decode succeeded on current but timed out twice on baseline; opening the reference FT8 WAV with legacy enabled did not meet the decode-model condition on either version. The inconsistent/failed decode cases are recorded in `startup-legacy-*-unresolved.txt` and are not counted as passing interoperability or RF evidence. Physical CAT, RF and actual operator-profile startup remain to be checked.

Startup profile growth diagnosis: `bridge_profile_growth_ab.py` proves the baseline copied an existing sibling profile into a new one and the fixed implementation copies global settings without copying initialized sibling profiles. `test_settings_profile_restart` covers preservation of normal grouped settings and profile isolation. With sanitized temporary copies of the operator INI, test-mode bridge constructor timings were 28503 ms before and 1900 ms after removing ten known synthetic test sections; those are isolated test timings, not the live-profile measurement. Test mode without an explicit settings path was checked to create a test-specific settings file while leaving the operator INI hash unchanged. Ten known test sections and their named MultiSettings entries were removed from the operator file after a full private backup, preserving all retained bytes; the active profile was not one of the removed profiles. The prior INI had accumulated profiles from older test runs; no claim is made that every removed line was created by a particular run.

Spectrum visibility extraction: `bridge_spectrumvisibility_ab.py` compares show/hide, remote waterfall retention, active TX guard and QSO visual rearm using real Qt timers and a controlled remote server. The isolated baseline/current application lab toggles visibility and invokes rearm with legacy monitoring and BlackHole input, CAT disabled and TX off. QSO-waterfall and GPU recovery source-contract tests pass. This does not compare rendered pixels, remote streaming or perform a real QSO.

ALL.TXT folder handling extraction: `bridge_alltxtfolder_ab.py` compares selected and fallback paths, existing/missing file behaviour and a rejected folder-open request with actual temporary files and a controlled desktop-service substitute. The baseline/current isolated application lab confirms the exposed ALL.TXT path belongs to the test profile with legacy backend active and CAT/TX disabled; it does not open a desktop file manager or alter the operator's log. Existing ALL.TXT and QSO-related source-contract tests pass after updating their source locations.

ALL.TXT folder Unicode follow-up: the same A/B harness also creates an existing log file under a directory containing spaces and non-ASCII characters and checks the precise local URL sent to the desktop-service substitute. Both original and extracted implementations passed; this is not a native file-manager observation.

ALL.TXT native folder follow-up: an isolated current-build macOS probe called `openAllTxtFolder()` with the legacy backend's test-profile path ending in `embedded-ft2/ALL.TXT`, which did not exist. The call returned true and Finder displayed the test profile directory containing `embedded-ft2`, matching the existing missing-file behaviour checked by the A/B harness. CAT, monitoring and TX were disabled and network access was denied. This observes the current build only and does not establish the behaviour of other desktop platforms.

The application portion is reproducible with `python3 tests/tools/bridge_alltxtfolder_native_mac.py`; it checks the isolated path, successful return and TX state, restores its temporary source changes and rebuilds. A subsequent run exited successfully. The script does not itself inspect the Finder window; the visual observation above was separate.

CW port reservation extraction: `bridge_cwports_ab.py` compares CAT keyer availability, active lease precedence, serial CAT/PTT conflicts, network bypass and canonical aliases with controlled collaborators; `bridge_cwports_registry_ab.py` additionally exercises the real process-local serial lease registry. The isolated app A/B lab checks CW controller availability through its bridge hook with CAT disabled, and the CW controller/CAT tests pass. Physical serial devices, Windows COM aliases in a Windows runtime, real PTT and RF are not tested.

CW rig-control extraction: `bridge_cwrig_ab.py` compares Morse forwarding, stop, startup/mode/remote PTT-on guards and PTT-off release using a simulated CAT backend. The isolated app A/B lab checks CW controller/keyer availability with CAT disabled; CW controller and CAT text tests pass. Actual Morse over CAT, physical PTT and RF are not exercised.

CW remote extraction: `bridge_cwremote_ab.py` compares remote key capability, busy guards, event forwarding, PTT, audio routing and abort with simulated collaborators. Existing DecoLink protocol tests exercise a local synthetic relay, and the baseline/current application CW sidetone lab verifies BlackHole playback with CAT disabled. The first current audio capture during concurrent compilation saw no tone; a repeat after the full suite finished passed. These tests do not establish a real remote-radio session, CAT PTT or RF operation.

CW transmit admission extraction: `bridge_cwadmission_ab.py` compares local keyer, busy, audio output, remote link, blocked link and remote-key-disabled decisions with isolated QSettings and controlled devices. The baseline/current application lab queries the actual CW controller admission hook with CAT disabled and confirms TX remains off; CW controller tests pass. Physical CAT, real remote link and RF transmission are not exercised.

CW tuning extraction: `bridge_cwtune_ab.py` compares invalid-frequency, disconnected, busy, calibrated local CAT and remote routing using controlled collaborators. The isolated baseline/current application lab invokes the actual QML-exposed CW tuning with CAT disabled and checks rejection and unchanged frequency. Existing CW CAT tests pass. The live CAT frequency command, remote radio and RF are not exercised.

QSO completion extraction: `bridge_qsocompletion_ab.py` compares TX disarm, queued continuation, protection of a different active QSO and CQ latch cleanup using controlled collaborators. The baseline/current macOS application lab logs a synthetic QSO into a new isolated test logbook with CAT/monitor off and network denied by sandbox, checking count, field cleanup and TX off; profiles are removed afterwards. TX dispatch and waterfall tests pass. It does not establish real RF, external logbook delivery or on-air multi-answer behaviour.

QSO UI-stall probe extraction: `bridge_logstall_ab.py` compares the baseline and extracted event filter/timer with a synthetic 120 ms main-thread stall, and `bridge_logstall_real.py` compiles and runs the actual new translation unit. Both check that two reports are emitted and an event longer than 60 ms is identified. The isolated baseline/current QSO application lab confirms synthetic logging still works with CAT/monitor off and network denied. It does not quantify production UI responsiveness or perform an on-air QSO.

ADIF append and logbook folder extraction: `bridge_adifappend_ab.py` compares baseline and extracted methods for first write, append, write failure, state notifications and folder-opening with temporary files and controlled serializer/desktop-service substitutes. The baseline/current isolated application QSO lab exercises actual serialization into a synthetic logbook, verifying one logged QSO, cleared call and TX off, with CAT and monitor disabled and networking denied. The folder-opening comparison uses a substitute and does not verify Finder or another desktop file manager.

Logbook folder follow-up: the A/B harness checks the actual local URLs sent to the desktop-service substitute for existing and newly created folders (`bridge_refactor_evidence/adif-folder-target-ab.txt`). A separate current-build macOS application probe opened its isolated test-profile logbook folder in Finder; the Finder window and displayed file URLs confirmed the target. The probe returned success with TX, CAT and monitoring disabled and networking denied. This native Finder observation covers the current build only; the baseline comparison uses the substitute, and other platforms were not tested.

On 2026-11-10 the append A/B harness passed again. Separately, the checksum-verified 1.0.737 app and local build each imported the same synthetic UTF-8 ADIF twice and exported one matching QSO; `adif-roundtrip-repeat-{baseline,refactor}.txt` under `bridge_refactor_evidence`. This app round-trip is not a direct test of every `appendAdifRecord` error branch.

Active logbook reload extraction: `bridge_logbook_reload_ab.py` compares the original and moved method using actual ADIF parsing of a temporary two-record file and a missing file, checking calls, counts, cache invalidation, signals and map callback with controlled collaborators. The current macOS offscreen application lab creates and switches isolated profiles across a restart, backs up and deletes them with CAT and network disabled. The previously recorded baseline profile lab covers create, switch, backup and removal; its environment differs from this offscreen run. No physical radio or desktop file manager is exercised.

Font-scale extraction: `bridge_fontscale_ab.py` compares baseline and moved methods for increasing, decreasing, clamping at 0.5 and 2.0 and notification counts. The offscreen macOS application lab invokes the actual bridge controls and checks both boundaries with CAT and network disabled. This does not assess the visual appearance of every UI surface.

Application information extraction: `bridge_appinfo_ab.py` compares original and extracted diagnostics path, desktop URLs, version and screen geometry using a real offscreen Qt screen and a controlled desktop-service substitute. The current application lab reads version, diagnostic path and screen geometry through QML, with CAT and network disabled; it does not open the browser or file manager and does not validate real multi-monitor placement.

Startup transient-QSO extraction: `bridge_startupqso_ab.py` compares original and moved reset/purge methods using isolated QSettings, including clearing stale QSO data, preserving unrelated values, TX disabled and both CQ-message branches. The offscreen macOS application lab seeds a synthetic test profile with a stale call, locator, report and TX message, then confirms cleanup, default report and TX off with CAT and network disabled. No on-air operation is performed.

Process-priority extraction: `bridge_priority_ab.py` compares all priority classes, clamping, unchanged settings and API failure using simulated Windows calls and isolated QSettings. The offscreen macOS application lab checks the exposed setting with CAT and network disabled; changing it has no operating-system priority effect on macOS. No native Windows process-priority or administrator-permission test is claimed.

Decode filter controls extraction: `bridge_filtercontrols_ab.py` compares ghost-filter control, bypass persistence, cache invalidation, model rebuilds and legacy CQ-only synchronisation using isolated QSettings and simulated collaborators. The offscreen macOS application lab toggles bypass and ghost settings via QML with CAT and network disabled; it does not establish radio reception or validate a live legacy decode list.

Keyboard-mode logging extraction: `bridge_keyboardlog_ab.py` compares CW/RTTY validation, report defaults, call and locator normalization, comments, mode override and duplicate-log reset with simulated logging. The offscreen macOS app lab logs one RTTY and one CW synthetic QSO in an isolated test logbook with CAT, network and TX disabled. The existing RTTY test passes; no on-air exchange or external logging is claimed.

Legacy decode text extraction: `bridge_legacytext_ab.py` compares baseline and extracted offset/tail readers and compiles the actual new translation unit, using temporary files with CRLF, line limits, offsets and missing/invalid input. The offscreen macOS application lab decodes a synthetic FT8 WAV and checks ALL.TXT output with CAT, network and TX disabled; it does not directly prove the legacy incremental mirror path or real RF reception.

FT8 hash candidate extraction: `bridge_legacyhash_ab.py` compares baseline and extracted candidate parsing, matching, ambiguity, time and frequency tolerance and paths. It runs the actual translation unit with both simulated collaborators and the production message normalization, callsign rules and time helpers. The offscreen macOS FT8 WAV application lab verifies decode and ALL.TXT output with CAT, network and TX disabled; it does not prove recovery of a hashed callsign in the full application.

Legacy FT8 hash bridge extraction: `bridge_legacyhashbridge_ab.py` compares the original and moved bridge method with synthetic ALL.TXT files and production candidate parsing, file readers, callsign rules and time helpers. It checks successful resolution, wrong mode, no match and ambiguity. An offscreen application FT8 WAV decode verifies ordinary reception and ALL.TXT writing; a hashed-call recovery through the full application was not observed.

Legacy incremental mirror extraction: `bridge_legacyaugment_ab.py` compares baseline and moved implementations against real temporary ALL.TXT files with controlled filter/semantic collaborators, exercising initial priming, appended RX records, duplicate suppression, TX and different-mode rejection, truncation and RX-pane behaviour. The offscreen macOS application lab separately verifies FT8 WAV decoding and ALL.TXT writing with CAT, network and TX disabled; the complete live legacy incremental mirror is not observed by that application lab.

Additional incremental mirror A/B cases: `bridge_legacyaugment_ab.py` also exercises upgrading a partial RX timestamp, replacing MSK144 zero time, and rejection by simulated semantic/user filters. These use real temporary files and controlled collaborators, not a full live legacy receive session.

The incremental mirror A/B additionally compiles the unmodified production translation-unit text in a test translation unit with isolated collaborators, exercising the same temporary-file cases. It is not an invocation on a live DecodiumBridge instance.

An attempted offscreen full legacy incremental-mirror lab did not reach its probe: denying all network traffic caused legacy UDP error dialogs and delayed backend startup until the test timed out. This attempt is inconclusive and must not be counted as a passing end-to-end test; a future run needs isolated local UDP endpoints or an equivalent test-only configuration before retrying.

Full legacy incremental mirror runtime: `bridge_legacyaugment_runtime_lab.py` runs the actual macOS legacy backend offscreen in a unique test profile, primes a test-only ALL.TXT, writes a synthetic FT8 RX line and waits for it in the actual decode list. The harness exclusively binds local UDP destination ports 2237 and 2239 to its own sinks, aborts if unavailable, restricts outbound sandbox traffic to those two loopback destinations, and disables CAT, monitor and TX. This is a current-build functional test; baseline equivalence is established separately by the controlled A/B harness, not by the same full application setup.

RTTY rig-mode selection extraction: `bridge_rttyrigmode_ab.py` compares normalization, persistence, RTTY mode application and CAT4OM/Hamlib/TCI/OmniRig routing with simulated managers, including disconnected hardware. The offscreen macOS application lab verifies mode-setting persistence with CAT, network and TX disabled; the RTTY test passes. No real CAT command or RF operation is exercised.

RX decode append extraction: `bridge_rxappend_ab.py` compares duplicate suppression, RX clear guards, SSTV suspension and legacy RX/TX behaviour with controlled collaborators. The offscreen macOS application lab decodes an FT8 WAV, verifies the synthetic active partner's message appears in both band and RX lists, and clears both; CAT, network and TX are disabled. The first lab attempt lacked an active partner and did not qualify for Signal RX; the corrected setup passed. Live RF and legacy TX are not exercised.

TX decode row extraction: `bridge_txappend_ab.py` compares empty input, report updates, CQ classification, local callsign and RX timeline marking with controlled collaborators. An offscreen macOS app lab sends a synthetic FT8 message via BlackHole 2ch, confirms a TX row in the RX timeline, uses no CAT or network, and rejects the synthetic log prompt without committing. This is virtual audio rather than RF.

RX list rebuild extraction: `bridge_rxrebuild_ab.py` compares original and moved method for retained entries, deduplication, eligibility, clear markers, no-op rebuilds and legacy guard with controlled collaborators. The offscreen app lab decodes a synthetic FT8 WAV, checks RX presence across a frequency change and ensures a cleared row is not reintroduced by a subsequent rebuild. CAT, network and TX are disabled.

Signal RX policy extraction: `bridge_rxpolicy_ab.py` compares local-call matching, unrelated and partner traffic, ghost rejection and TX acceptance with controlled collaborators. The offscreen macOS app lab decodes a synthetic FT8 WAV and checks that unrelated traffic is excluded from RX until the matching partner is selected; CAT, network and TX are disabled.

Decode UTC helper extraction: `bridge_decodeutc_ab.py` compares original and moved normalization, parsing and midnight delta, and compiles the actual new module. FT8 hash resolution A/B suites and the offscreen FT8 WAV decode/RX rebuild lab pass; CAT, network and TX are disabled in the application lab.

Decode chronology extraction: `bridge_chronology_ab.py` compares midnight ordering, explicit timestamps, TX visual time and stable sort tie-breaks, including the actual new translation unit. The offscreen app lab verifies synthetic FT8 decode and RX list rebuilding with CAT, network and TX disabled; its single decoded fixture does not test chronological ordering inside the full app.

The offscreen legacy multi-row observation (`bridge_chronology_runtime_lab.py`) injects three synthetic FT8 lines in order C/A/B (times 12:00:45, 12:00:15, 12:00:30). The current build exposed the raw and Band Activity model order C/A/B, not chronological order. This setup exercises the legacy incremental-append path, which may not call the extracted chronological normalization; the original Band Activity filter also preserves source order. No equivalent full-application baseline run has established whether this is pre-existing, so do not interpret the observation as an A/B pass or regression proof. It remains a limitation requiring comparison/investigation.

Legacy arrival-order follow-up: `bridge_legacyorder_source_ab.py` confirms the baseline and current source are identical for unique append, the legacy band-delta insertion branch, and Band Activity filtering; this supports but does not prove the same full-app ordering. An attempted offscreen baseline app run with the system Qt failed at launch because the baseline was built with a newer Qt ABI; it is inconclusive and is not a passing comparison.

Decode timestamp extraction: `bridge_decodetimestamp_ab.py` compares baseline and moved timestamp/date conversion and fallback and runs the actual translation unit. The offscreen app lab checks that a synthetic FT8 WAV decode has a nonzero timestamp and survives RX-list rebuilding; CAT, network and TX are disabled. Midnight ordering remains separately covered by `bridge_chronology_ab.py`.

Display deduplication extraction: `bridge_displaydedup_ab.py` compares FT8/FT4/FT2 slot and frequency buckets, richer-entry selection and RX/TX separation; it also exercises the actual production module. An offscreen app FT8 WAV/RX rebuild lab passes with CAT, network and TX disabled; it does not establish duplicate selection in the complete application.

Full application display-dedup check: `bridge_displaydedup_runtime_lab.py` injects two FT8 legacy log lines in the same slot/frequency bucket with -20 and -10 dB, triggers a model rebuild and checks that the actual Band Activity model retains exactly one row at -10 dB while the raw list contained both. CAT, monitor and TX are disabled; UDP is sandboxed to laboratory-owned loopback sinks. This is a functional test of the current build; baseline A/B remains the separate module-level comparison.

UTC slot token extraction: `bridge_utcslot_ab.py` compares baseline and moved code for FT8, FT4, FT2 and minute slots, invalid values and midnight, and links the actual production unit. The offscreen app FT8 WAV lab checks a valid decode timestamp and RX list behaviour with CAT, network and TX disabled; FT4/FT2 full decoder operation is not covered by that app lab.

QSO snapshot extraction: `bridge_qsosnapshot_ab.py` compares ADIF row normalization, chronological sorting, statistics, export fingerprint and missing-file handling using real temporary ADIF parsing and a controlled geographic-distance collaborator. The offscreen app lab exercises import, search, statistics, selected export, edit and deletion of a synthetic UTF-8 QSO with CAT, network and TX disabled. `test_b4_worked_before` also passes.

ADIF support extraction: `bridge_adifsupport_ab.py` compares lightweight record counting across a 64 KiB boundary, missing-file handling, worked-call rebuilding and FT4/Q65/CW mode mapping using temporary files. The offscreen app lab imports a synthetic UTF-8 QSO, edits its mode from FT8 to FT4, verifies search/stats and deletes it; CAT, network and TX are disabled.

Full ADIF export destination follow-up: `bridge_adif_full_export_ab.py` now checks an existing directory as an invalid output destination in both placements and both synthetic logbook variants. All four runs rejected it and subsequently exported the QSO successfully to a normal file. Legacy rig control and CAT were disabled in the test; this checks an output-open failure, not a write or commit failure after opening. Evidence: `bridge_refactor_evidence/adif-directory-export-ab.txt`.

Signal report extraction: `bridge_signalreport_ab.py` compares SNR parsing, clipping, positive/negative reports, R prefix, empty and malformed values, including the actual production unit. The offscreen app lab exercises the report property with CAT, network and TX disabled. It does not establish an on-air QSO.

FTX message construction extraction: `bridge_ftx_helpers_ab.py` compares the baseline and production-linked helpers over 3,625 combinations of mode, callsigns, message slots, flags and CQ prefixes. The harness uses controlled substitutes for the callsign classification and hash collaborators; normalization and report formatting use the production implementations. `bridge_tx_messages_runtime_lab.py --require-legacy` exercises editing all six TX slots, CQ override, validation and reset in an isolated offscreen legacy session, with CAT, monitor and transmission disabled and UDP restricted to test-owned loopback sockets. The app test does not establish an on-air exchange or exhaustively cover non-standard callsigns.

Decoded identity extraction: `bridge_decodeidentity_ab.py` compares callsign and locator extraction for CQ, directed, malformed, compound and empty messages against v1.0.737, using the unchanged token helpers and production sequencer rules in a controlled bridge harness. `bridge_decodeidentity_runtime_lab.py` verifies that two real synthetic FT8 WAV decodes expose the expected `fromCall` and `dxGrid` in the application, with CAT, network, monitoring and TX disabled. The WAV checks do not cover every directed or compound-message case in the application.

Local decode-call candidates: `bridge_localcall_ab.py` compares all sources of candidate own calls, including configured and simulated legacy callsigns, previous and pending TX messages and current TX text, using production token rules. The isolated legacy application lab injects two directed FT8 lines and checks that the peer is identified correctly in both directions; CAT, monitor and TX are disabled and UDP is restricted to test-owned loopback sockets. It does not exercise a real RF exchange or every candidate-source combination in the full application.

QSO field extraction: `bridge_qsofields_ab.py` compares locator validation, simulated legacy synchronization, outgoing and incoming report normalization including placeholder/real status, RR73, change notifications and message regeneration using production token and report rules. The isolated app lab verifies valid and invalid locator input, received-report formatting and RR73 switching with CAT, network, monitor and TX disabled. Outgoing-report and placeholder branches are exercised in the controlled A/B harness, not independently in the app lab.

Per-call QSO limit extraction: `bridge_qsolimit_ab.py` compares limit clamping, notifications and isolated settings persistence, plus real temporary ADIF files with multiple contacts, base callsigns, bands, FT2 submode preference, old dates and missing files. The isolated application lab checks low/high clamping and persists a value into a temporary settings profile with CAT, network, monitor and TX disabled. The ADIF recount is exercised in the A/B harness with real files and a controlled bridge, not inspected inside the running application.

UDP decode reporting extraction: `bridge_udpdecode_ab.py` compares filtering and dispatch for three destinations, FT8 and WSPR fields, malformed inputs and QSO filter migration using simulated message clients and a controlled FT8 parser. Two isolated application labs decode different synthetic FT8 WAVs and capture matching outgoing UDP packets on private loopback ports; CAT, monitoring, TX and all other network destinations are disabled. WSPR wire interoperability and legacy-backend UDP emission are not established by the app labs.

UDP status extraction: `bridge_udpstatus_ab.py` compares the baseline against the moved implementation with simulated clients across three destination filters, legacy gating, report defaults, special-operation limits and QSO progress. Two isolated application sessions capture actual UDP packets with WSJT-X magic, Status type and the synthetic local callsign on an exclusively owned loopback port, while decoding different FT8 fixtures. CAT, monitoring and TX are disabled and outbound network access is limited to that port. The app lab does not decode and independently verify every field in the status datagram or exercise legacy-backend UDP output.

Raw ADIF UDP extraction: `bridge_adifudp_ab.py` compares the raw and N1MM forwarding methods and target helpers with v1.0.737. It sends and receives real datagrams on a private loopback socket, checks invalid destinations, disabled forwarding, normalized ADIF band, and port validation. The isolated application lab logs one synthetic QSO and captures its ADIF on a private loopback port, with CAT, monitoring and TX disabled and all other outbound network destinations denied. This does not verify a real external logger or RF QSO.

ADIF TCP forwarding extraction: `bridge_adiftcp_ab.py` compares the moved function with v1.0.737 and runs both versions against a real temporary loopback TCP server, verifying disabled and empty-payload cases and the exact delivered ADIF bytes. An isolated application lab logs a synthetic QSO and receives its ADIF over TCP on a private loopback port with CAT, monitor and TX disabled and other outbound destinations denied. A refused connection was also checked in the A/B lab; timeout and write-error paths were preserved in source but not independently induced.

UDP logged-QSO fan-out extraction: `bridge_udpqso_ab.py` compares v1.0.737 and moved implementation across 32 combinations of legacy state, destination settings and client availability using simulated clients and ADIF destinations. An isolated app logs one synthetic QSO and captures actual WSJT-X QSOLogged and LoggedADIF packet types with the synthetic call on an exclusively owned loopback port. CAT, monitoring and TX are disabled and other outbound destinations denied. The app lab checks packet types and call, not every field or legacy backend forwarding.

External ADIF orchestration extraction: `bridge_externaladif_ab.py` compares destination selection, invalid input, UDP failures, immediate submission and asynchronous QRZ/Cloudlog confirmation or failure against v1.0.737 using simulated services. An isolated application calls the real upload entry point for an invalid request and a synthetic UDP ADIF request, then captures the expected packet on an exclusively owned loopback port with CAT, monitoring and TX disabled and all other network destinations denied. Actual QRZ/Cloudlog service interaction and their network callbacks were not exercised.

QSO band and duplicate checks extraction: `bridge_qsoband_ab.py` compares frequency band boundaries and fallback, UTC-day per-call limits, base callsigns and the 600-second recent-worked/logged window against v1.0.737, using the production Radio callsign rules and a controlled band manager. The isolated app lab verifies limit configuration and persistence, but does not exercise the moved private duplicate/limit checks through a live AutoCQ interaction.

Decode filter settings extraction: `bridge_filtersettings_ab.py` compares the moved code to an exact pre-extraction snapshot (the current implementation includes an optimisation newer than v1.0.737). It exercises temporary profile and legacy settings, alias precedence, duplicate terms, territory normalization and bypass with a simulated settings-profile/alias collaborator. An isolated app decodes a synthetic FT8 WAV with the filter off, checks that the call is absent after enabling a blacklist, and observes it again with bypass enabled; CAT, monitoring, network and TX are disabled. The probe also checks the diagnostic log between unique test markers for the explicit blacklist rejection of the synthetic decode.

Decode user-filter evaluation extraction: `bridge_userfilters_ab.py` compares v1.0.737 and the production-linked moved filter logic across 98 entries and filter/scope combinations, with production message/token normalization and the production callsign classifier. It covers blacklist, whitelist, pass rules, continent exclusion, calling-only, wait/pounce scope, empty messages and TX rows. The isolated FT8 WAV application probe verifies the unfiltered and bypass cases and explicitly confirms a blacklist rejection in the diagnostic log; other branches are covered in the A/B harness rather than the app.

Decode token classifier extraction: `bridge_tokenclass_ab.py` compares grid, telemetry and callsign classifications with v1.0.737 and links the production module, exercising locators, RR73, compound and placeholder calls and hexadecimal telemetry. The user-filter A/B harness now also links the actual classifier, rather than a substitute. Isolated application labs verify a synthetic FT8 decode's callsign/grid and an explicit diagnostic blacklist rejection with bypass recovery. The first identity-lab attempt was rejected by a previously active test blacklist; the rerun explicitly enabled bypass in the test profile.

Unresolved decode and display extraction: `bridge_unresolved_ab.py` compares v1.0.737 with the production-linked module for placeholder/locator, local-peer placeholders, FST4W recognition and display ordering over 72 cases using the production classifier and token rules. Isolated app WAV tests check two ordinary CQ messages, associated calls/grids and display order with CAT, monitoring, network and TX disabled; they do not exercise unresolved hashes through the application. An existing baseline observation is retained: for `<...> FN42`, the grid-only helper returns false while the FST4W-specific helper returns true in FST4W. The token `...` is accepted by the directed-CQ modifier predicate before the placeholder check, so the helper skips it. A synthetic FT8 legacy ALL.TXT row with this text did not surface in the application within the observation timeout; the application-level effect remains unverified. The pre-existing behaviour was not changed during the refactor.

Decode validation rule extraction: `bridge_validation_ab.py` compares 16 moved validation functions against v1.0.737 and links the production module, exercising amateur and compound calls, synthetic calls, CQ modifiers, report/signoff tokens, contest payloads, invalid prefixes, and FT2 trailing payloads. Two isolated application sessions decode synthetic FT8 WAVs and verify the calls/grids with CAT, monitoring, network and TX disabled. The app probes exercise valid FT8 decode paths; the individual rejection and contest branches are covered by the controlled A/B harness, not end-to-end RF tests.

Decode semantic decision extraction: `bridge_semantic_ab.py` compares v1.0.737 and the moved methods over 224 mode, contest, active-QSO, placeholder and message combinations using production validation and telemetry rules and a controlled hash-context collaborator. An isolated legacy application lab injects a valid CQ and an invalid incomplete callsign into test ALL.TXT; it verifies the valid row and the explicit rejection diagnostic with CAT, monitoring and TX disabled. It does not exercise all FT2, contest or unresolved-hash branches in the running app. A first trial using `0Z4SYH` did not produce the intended rejection and was replaced with the demonstrably invalid `X9` case; no behaviour was changed.

Decode telemetry extraction: `bridge_telemetry_ab.py` compares v1.0.737 and the production-linked preliminary validation and telemetry classification over 26 inputs, including callsigns, hex strings and malformed messages. The semantic A/B harness now links the actual telemetry classifier. In an isolated legacy application session, a synthetic valid FT2 CQ is displayed and an FT2 telemetry line is explicitly rejected in the diagnostic log; CAT, monitoring and TX are disabled.

Ghost decode filter extraction: `bridge_ghost_ab.py` compares the v1.0.737 and moved implementation with sequential strong/weak/repeated signals, malformed messages, FT2 telemetry, RTTY text and TX rows. The controlled harness uses simulated DXCC and geography collaborators. The isolated application lab exercises the real filter and its weak-signal repeat state, malformed R+grid, FT2 telemetry, RTTY bypass and disabled setting, with CAT, monitoring, network and TX disabled. The app test does not establish real RF reception or every DXCC/geographical branch.

Directed ghost-filter extraction: `bridge_directedghost_ab.py` compares the three moved methods against v1.0.737 for first/repeated weak calls, simulated cached hashes, active and late partners, invalid DXCC, grid mismatch and TX rows. It uses real callsign validation with simulated DXCC, geographic distance and hash-cache collaborators. An isolated legacy app session checks two synthetic directed messages are attributed to the expected peers; it does not independently prove each directed-ghost suppression branch in the running app, and no RF traffic is sent.

Legacy snapshot mirror extraction: `bridge_legacymirror_ab.py` compares the two moved overloads with an exact pre-extraction snapshot (the current implementation includes newer optimisations). It checks RX/TX parsing, 384-row cap, filtering, timestamp stabilization and reuse versus enrichment, using production message/time normalization and the real alternate DecodedText parser. The Fortran standard-message classifier and bridge collaborators are controlled stubs; the alternate input is asserted not to match the fast parser, and its time, SNR, DT and frequency are checked. The isolated legacy app lab observes two synthetic directed messages via ALL.TXT with CAT, monitoring and TX disabled; that app lab exercises the adjacent augmentation path and does not independently prove parsing of nonempty legacy widget snapshots by the moved mirror methods.

Decode DXCC list refresh extraction: `bridge_decode_refresh_ab.py` compares the moved method with an exact pre-extraction snapshot and exercises changed and unchanged entries, TX exclusion, both lists, empty lists, normalization and notifications using controlled collaborators. `bridge_decode_refresh_runtime_lab.py` injects two synthetic legacy ALL.TXT messages in an isolated profile, toggles the US-state display to invoke the real refresh, and verifies that both rows survive with TX disabled. CAT and monitoring are disabled and network access is sandboxed. The runtime probe does not independently verify DXCC database contents or every signal/normalization branch; those branches use controlled A/B collaborators.

Decode enrichment extraction: `bridge_enrichment_ab.py` checks the two moved methods against the exact pre-extraction snapshot, allowing only the existing right-callsign wrapper. Two isolated application binaries built immediately before and after extraction decode synthetic FT8 CQ WAVs for K1ABC/FN42 and 9H1SR/JM75. Their returned calls, grids, DXCC countries, continents, prefixes, zones, display/provenance, worked/new-grid flags and colours match; the observations are saved in `bridge_refactor_evidence/decode_enrichment`. The application tests use CAT and monitoring disabled, TX disabled and sandboxed network. They cover two valid CQ cases; TX, unresolved hashes, worked-history variants and other enrichment branches are not independently exercised end to end by this app comparison. The snapshot check establishes unchanged source for those other branches, not functional runtime coverage.

Legacy waterfall row extraction: `bridge_legacy_waterfall_ab.py` compares the pre-extraction snapshot with the moved body (using the existing logging wrapper) and checks sample scaling, span, empty inputs, modern/legacy feed priority and remote publication guards using controlled collaborators. An isolated legacy application A/B test invokes the real slot with a synthetic row and observes the identical emitted panadapter values and bounds in QML, with CAT, monitoring and TX disabled and network sandboxed. For these two lab binaries the slot was temporarily exposed to QML solely for injection; the source header was restored to private before the final build. The test does not exercise physical audio, rendering on screen, or an actual remote client.

Caller scoring extraction: `bridge_caller_scoring_ab.py` compares all three moved methods against the pre-extraction snapshot, normalizing calls to existing logging and partner-inference wrappers. Its controlled harness uses the real scoring policy and simulated DXCC, message parsing and worked-history collaborators to check worked/new entities, priority, pinned callers, manual queue ordering and slot-row ranking. Isolated legacy application sessions A/B test two opposite caller orders, scoring activation, manual reordering and its persistence, with CAT, monitoring and TX disabled and network sandboxed. They do not establish autonomous caller scoring on actual RF or exercise slot-row ranking end to end in the application. Outputs are retained under `bridge_refactor_evidence/caller_scoring`.

Generic settings extraction: `bridge_generic_settings_ab.py` compares the compatibility helpers and both moved methods to their exact pre-extraction source, allowing only calls to existing alias, backend and logging wrappers. Two isolated legacy application A/B cases write and read synthetic profile values, ShowDXCC and mutually exclusive PromptToLog/AutoLog settings, with CAT, monitoring and TX disabled and network sandboxed. Results match and are stored under `bridge_refactor_evidence/generic_settings`. Credential storage, CAT/audio hardware, network services, other aliases and legacy INI migration are not independently exercised by this runtime check; identity of source alone does not establish their end-to-end correctness.

Graphics startup extraction: `bridge_graphics_startup_ab.py` compares the four moved methods and Windows watchdog helpers with an exact pre-extraction snapshot, allowing the existing logging wrapper. Two isolated macOS legacy app binaries A/B invoke graphics configuration with a hidden QQuickWindow and null (respectively accepted and rejected), repeat the ready notification, and confirm TX remains off; CAT and monitoring are disabled and network sandboxed. The QML startup itself completes in both cases. The Windows watchdog, its marker and actual on-screen rendering are not exercised by this macOS test. Results are in `bridge_refactor_evidence/graphics_startup`.

QML load-start timestamp follow-up: `bridge_qml_load_timestamp_ab.py` invokes
the actual method in isolated macOS app sessions before and after extraction,
and verifies that the recorded timestamp lies between the call's start and end.
Both pass with CAT and TX off; results are in
`bridge_refactor_evidence/qml-load-timestamp-ab.txt`. The invocation is a test
probe; it does not exercise the Windows-only watchdog or measure startup time.

Legacy JT callbacks extraction: `bridge_legacy_jt_ab.py` compares all five moved callbacks to their exact pre-extraction source modulo existing logging/dedup wrappers, and exercises FT4/Q65/MSK144/FST4 forwarding and the FST4 shutdown guard using a controlled recipient. Two isolated legacy app binaries A/B inject synthetic JT rows for K1ABC/FN42 and 9H1SR/JM75, checking calls, grids, frequencies, CQ flags, duplicate suppression and TX off. To allow direct injection, the slots were temporarily exposed in those lab binaries and restored to private for the final build. CAT and monitoring were disabled and network sandboxed. These are callback tests, not RF or end-to-end FT4/Q65/MSK144/FST4 demodulation; results are under `bridge_refactor_evidence/legacy_jt`.

WSPR callback extraction: `bridge_wspr_callback_ab.py` compares the moved callback against an exact pre-extraction snapshot modulo existing logging/dedup wrappers. Two isolated legacy app binaries A/B inject WSPR rows for DU1MGA/PK04 and K1ABC/FN42, plus duplicate, invalid and completion rows. They produce identical calls, grids, frequencies and messages with TX and WSPR upload disabled; CAT and monitoring are off and network sandboxed. The slot was temporarily exposed only in the lab binaries and restored private for the final build. This does not verify physical WSPR demodulation, remote upload, stale-session or SSTV branches end to end. Results are under `bridge_refactor_evidence/wspr_callback`.

Legacy PCM extraction: `bridge_legacy_audio_ab.py` checks the moved callback and shared FT2-Link PCM statistics against pre-extraction source modulo existing wrappers, and A/B runs the statistics on empty, threshold and full-scale sample sets. Two isolated legacy app binaries compare the real callback with two synthetic two-sample PCM inputs; diagnostic sample count, RMS, peak and nonzero count agree, with monitoring and TX disabled. The pre-extraction binary was an earlier lab build that still contained the same original audio callback; both lab binaries temporarily exposed the slot and it was restored private for the final build. The runtime test exercises the no-recorder/no-spectrum/no-FT2 forwarding path, not hardware, recording, spectrum feed or FT2-Link forwarding. Results are under `bridge_refactor_evidence/legacy_audio`.

Legacy PCM follow-up: `bridge_legacy_audio_paths_ab.py` runs both pre-extraction and moved callback code with the real PCM statistics and controlled recorder, ring and FT2-Link collaborators. It verifies little-endian samples, recording feed, spectrum ring, FT2 batching, cancellation on TX, dedicated modern feed suppression and the pending-sample cap. Two isolated legacy app A/B runs per version also record synthetic PCM to real 12 kHz mono WAV files under `.qttest/Documents`, verify exact sample bytes and delete only those files. The binaries temporarily expose the callback for injection; it is private in the final build. The controlled FT2/ring test does not prove real radio capture or FT2-Link reception.

ALC calibration extraction: `bridge_alc_calibration_ab.py` compares six moved methods against an exact pre-extraction snapshot modulo the existing logging wrapper, and runs both with real ALC math and simulated CAT/tune/settings collaborators. It checks target clamping, CAT and active-TX guards, convergence, cancellation, timeout and missing telemetry. `test_alc_calibration` passes. Two isolated legacy app A/B cases verify target clamps and unavailable-CAT rejection with monitoring and TX off; network is sandboxed. Physical ALC, rig telemetry and actual carrier calibration are not tested. Outputs are under `bridge_refactor_evidence/alc_calibration`.

DecoPort/FT2 drift settings extraction: `bridge_port_settings_ab.py` compares the five moved methods against their pre-extraction snapshot modulo the existing logging wrapper, using real QSettings and profile helpers in a temporary directory. It checks defaults, port bounds and root/profile persistence. The direct-profile test emits the same pre-existing `QSettings::endGroup: No matching beginGroup()` warning in both variants; the refactor retains this behaviour. Two isolated legacy app A/B cases verify flags and invalid/maximum port handling with CAT, monitoring and TX disabled and network sandboxed. They do not start a DecoPort service or use radio hardware. Evidence is under `bridge_refactor_evidence/port_settings`.

Sequencer option extraction: `bridge_sequence_options_ab.py` compares the four moved methods against the exact pre-extraction snapshot modulo the existing logging wrapper, and exercises AutoSeq, Wait & Pounce, legacy hold, settings persistence and FT2 accumulo using simulated legacy and FT2 collaborators. Two isolated legacy app A/B cases toggle the actual options and verify the resulting states and TX off, with CAT and monitoring disabled and network sandboxed. This does not verify an actual QSO or FT2 on-air behaviour. Evidence is under `bridge_refactor_evidence/sequence_options`.

Audio selection extraction: `bridge_audio_selection_ab.py` compares the moved setters and getters against the exact pre-extraction snapshot modulo forwarding wrappers for the existing device-resolution, identity, logging and Linux Pulse/PipeWire helpers. Two isolated legacy app A/B cases use nonexistent synthetic input/output names and out-of-range channels, verifying trimming, clamping, monitoring off and TX off; network is sandboxed. They do not open real devices or test Linux Pulse/PipeWire default-device changes. Evidence is under `bridge_refactor_evidence/audio_selection`.

Audio selection follow-up: `bridge_audio_selection_virtual_runtime_lab.py` compares two isolated legacy app binaries using the actual BlackHole 2ch and 16ch devices in both directions, with monitoring/TX disabled and network sandboxed. It explicitly refreshes the device list before selection and verifies names and stable nonempty device IDs; outputs match in `bridge_refactor_evidence/audio_selection`. A prior trial without refresh saw empty IDs, and the baseline reproduced that outcome; timing or cached enumeration is a plausible explanation, not a proven root cause. No audio stream or Linux Pulse/PipeWire operation was tested.

Settings save extraction: `bridge_save_settings_ab.py` compares the three moved methods to an exact pre-extraction snapshot modulo forwarding wrappers. Two isolated legacy app A/B cases perform synchronous and repeated asynchronous saves and check callsign, RX frequency and hold state, with CAT, monitoring and TX disabled and network sandboxed. A first attempt to start the second session encountered UDP port 2238 temporarily occupied; it was free on inspection and the retry passed. The application check does not independently cover every persisted field, secure credentials or shutdown races. Evidence is under `bridge_refactor_evidence/save_settings`.

The separate `bridge_save_settings_restart_ab.py` app test also restarts each isolated profile after synchronous and repeated asynchronous saves. Both source placements restored the expected synthetic callsign, RX frequency and hold state for both parameter sets, with CAT and TX disabled; `bridge_refactor_evidence/save-settings-restart-ab.txt`. Other settings and in-flight shutdown races remain outside this probe.

Restart follow-up: `bridge_save_settings_restart_ab.py` compares both source
placements with two isolated configurations each. Following synchronous and
repeated asynchronous saves, a new app process reads back the expected RX
frequency and hold-TX setting. CAT and TX are disabled; compact results are in
`bridge_refactor_evidence/save-settings-restart-ab.txt`. The lab also passes a
callsign on both command lines, so it does not establish callsign persistence.
It does not cover all settings, secure credentials or shutdown races.
The subsequent rerun removes `--lab-callsign` on restart and verifies that the
same callsign is read back in both scenarios and placements; see
`bridge_refactor_evidence/save-settings-restart-callsign-ab.txt`.

Settings load extraction: `bridge_load_settings_ab.py` compares the moved method against the exact pre-extraction snapshot modulo forwarding wrappers and checks shared watchdog/special-operation defaults retain their original values. Two isolated legacy app A/B cases start from synthetic settings files and compare callsign, RX/TX audio frequencies and Hold TX, with CAT, monitoring and TX disabled and network sandboxed. `test_settings_profile_restart` passes. During extraction the A/B suite caught relocation of the max thread constant; the constant was restored and the existing wrapper reused, then app comparisons repeated. The application test does not cover every saved field, credential or service startup path. Evidence is under `bridge_refactor_evidence/load_settings`.

Settings reload extraction: `bridge_reload_settings_ab.py` checks the moved reload method against its pre-extraction snapshot modulo existing forwarding wrappers. Two isolated legacy app A/B cases save synthetic CAT profiles, change input/output channels and reload the saved profiles, confirming identical restored values with CAT, monitoring and TX disabled and network sandboxed. The first baseline attempt did not persist the channel change before saving the profile; the revised test explicitly saves settings first. The app cases do not exercise connected CAT, active monitoring, audio restart or live rig telemetry. Evidence is under `bridge_refactor_evidence/reload_settings`.

CAT profile extraction evidence: the existing full A/B runner compares helper and method source sections with v1.0.737, allowing the log-forwarding name change, and runs both implementations in `bridge_refactor_cat_profiles_harness.cpp` against isolated settings and simulated CAT managers. A retained passing result is `bridge_refactor_evidence/legacymirror-full-ab.txt`; the source comparison was checked again against the current files. Separately, `bridge_cat_profiles_runtime_ab.py` compares the current profile implementation temporarily placed in the main Bridge file against the separated module, using two isolated legacy app sessions per placement. Both variants save a synthetic profile, rename and list it, reject an empty replacement name, reload its stored audio channels, delete it and reject a second deletion. The four results match and are retained in `bridge_refactor_evidence/cat_profiles/operations-ab.txt`. The app comparison uses the same current implementation in both placements, with CAT, monitoring and TX disabled and networking sandboxed; neither test is a connected-radio test.

The focused `test_settings_profile_restart` CTest was rebuilt and rerun on 2026-11-10 and passed. It is a profile regression check, not proof that every saved setting or external service survives restart.

The expanded app A/B lab additionally attempts to load a missing synthetic profile before saving and loading a valid one. Both placements return failure for the missing profile without changing input/output channels and restore the valid profile in two variants; `bridge_refactor_evidence/reload-missing-profile-ab.txt`. CAT and monitoring remain disabled.

CAT split calculation extraction: `bridge_cat_split_ab.py` compares the four moved methods against their pre-extraction snapshot modulo forwarding wrappers, and exercises rig/emulated split, XIT, TX dial, VHF guard and Doppler using simulated CAT and Doppler objects. Two isolated legacy app A/B cases set TX audio frequency with CAT, monitoring and TX disabled and network sandboxed. The app cases confirm the adjacent disconnected path, not connected split transactions; no physical rig or PTT was operated. Evidence is under `bridge_refactor_evidence/cat_split`.

PSK Reporter query extraction: `bridge_psk_queries_ab.py` compares the six moved methods against the exact pre-extraction snapshot modulo the existing logging wrapper. Isolated legacy app A/B cases exercise search for two callsigns and heard-by/map fetch failure paths under a network-denying sandbox; search/fetch indicators reset and results match with TX off. Send is invoked only with reporting disabled. The tests do not exercise successful external PSK Reporter responses, XML parsing or actual report delivery. Evidence is under `bridge_refactor_evidence/psk_queries`.

Local success follow-up: `bridge_psk_local_response_ab.py` uses a test-only URL
substitution to direct both placements to a loopback HTTP server. Real app
requests carry the expected callsign and limit; the returned synthetic XML
produces identical receiver rows, removes a duplicate, clears an invalid grid,
and finishes fetching in both placements. CAT and TX remain off; only loopback
networking is allowed. Results: `bridge_refactor_evidence/psk-local-response-ab.txt`.
This covers parsing and local request handling, not external HTTPS or delivery
to PSK Reporter.

Wait & Pounce guard extraction: `bridge_wait_pounce_ab.py` compares the moved method against its exact pre-extraction snapshot modulo existing forwarding wrappers and exercises disabled/unarmed/TX guards, filtering, duplicate and throttle cases with controlled collaborators. Two isolated app A/B cases decode synthetic FT8 CQ WAVs and verify the explicit unarmed guard in the diagnostic log, with TX remaining disabled, CAT and monitoring off and network sandboxed. A diagnostic log rotation initially made the test miss a present message; the probe now reads the previous and current log. Positive auto-reply and RF transmission were not exercised in the app. Evidence is under `bridge_refactor_evidence/wait_pounce`.

Decode-click extraction: `bridge_decode_click_ab.py` compares all three moved methods to the pre-extraction snapshot modulo forwarding wrappers and runs generated QSO messages over twelve FT8/FT4/FT2, Quick QSO and RR73 combinations plus custom CQ; it links production message helpers with a controlled callsign encoder. `bridge_ftx_helpers_ab.py` separately compares 3,625 helper cases. Two isolated legacy app A/B cases verify that an empty decode and an invalid-frequency DX spot do not arm TX or select a partner; CAT and monitoring are disabled and network sandboxed. Positive double-click, real PTT and RF paths are not exercised in the app. Evidence is under `bridge_refactor_evidence/decode_click`.

Decode-click positive follow-up: `bridge_decode_click_positive_runtime_lab.py` compares two valid CQ selections (K1ABC/FN42 and W1AW/FN31) in isolated legacy app binaries and confirms partner/grid and TX1–TX5 message text match. CAT is explicitly disabled, network sandboxed, monitoring off and output forced to BlackHole 2ch; the test invokes HALT immediately after selection and confirms TX and TX enable are off afterwards. It does not establish on-air operation or real PTT. The prior note saying the positive path was not exercised in the app is superseded by these tests. Evidence is under `bridge_refactor_evidence/decode_click`.

Wait & Pounce positive follow-up: `bridge_wait_pounce_positive_runtime_lab.py` uses the existing passive lab arming option and synthetic FT8 CQ WAVs for K1ABC/FN42 and 9H1SR/JM75. Both isolated app binaries select the same peer and grid, produce matching TX1/TX2 text and log the positive trigger. CAT is explicitly disabled, network sandboxed, monitoring off and output forced to BlackHole 2ch; the test invokes HALT immediately after detection and checks TX and TX enable are off. This supersedes the prior note that the positive app path was untested; physical radio/PTT and over-the-air QSO remain untested. Evidence is under `bridge_refactor_evidence/wait_pounce`.

Deferred AutoSeq extraction: `bridge_deferred_autoseq_ab.py` compares four moved methods against the exact pre-extraction snapshot modulo the existing logging wrapper, using real callsign/token helpers and controlled QSO collaborators. It covers stale mode, partner and payload, deferred 73, FT2 CQ preservation, clearing and partner memory. Two isolated legacy app A/B cases select synthetic CQs using BlackHole 2ch and CAT disabled, then immediately HALT; message outputs match. These app cases exercise the adjacent selection path, not a real deferred rearm or physical PTT. Evidence is under `bridge_refactor_evidence/deferred_autoseq`.

Resume-on-reply extraction: `bridge_resume_reply_ab.py` compares the two methods against the exact pre-extraction snapshot modulo the existing logging wrapper, using real token rules and simulated decoder/click collaborators. It checks manual hold, other partner, echo/ghost filters, matching reply, stale mode and timeout. Two isolated legacy app A/B sessions enable the option, select synthetic CQs through BlackHole 2ch with CAT disabled and then HALT, comparing the resulting states and messages. They do not inject a matching post-HALT reply into the real app or test RF/PTT. Evidence is under `bridge_refactor_evidence/resume_reply`.

Resume-on-reply positive follow-up: `bridge_resume_reply_positive_runtime_lab.py` drives two isolated legacy app binaries through FT2 CQ selection, TX3 selection and HALT, then injects a synthetic directed reply through the lab FT2 receive interface. For K1ABC and W1AW, both versions log the matching-resume trigger and produce identical partner/message state. CAT is disabled, networking sandboxed, monitoring off and output forced to BlackHole 2ch; a second HALT immediately follows the observed resume. The decoded reply has no grid and the resulting grid is empty in both versions. This supersedes the prior note that the matching post-HALT reply had not been tested in-app; no physical radio/PTT or over-air reception was used. Evidence is under `bridge_refactor_evidence/resume_reply`.

FT2 abort/rearm extraction: `bridge_autoseq_rearm_ab.py` compares both moved methods to an exact pre-extraction snapshot modulo the existing logging wrapper and tests abort cleanup, non-FT2 guard, stale rearm and preservation of deferred 73 with controlled collaborators. Two isolated legacy app A/B runs per version select synthetic CQs, HALT, inject directed FT2 replies and HALT after resume; the diagnostic confirms FT2 abort cleanup and resulting states match. CAT is disabled, monitoring off, network sandboxed and output BlackHole 2ch. The app test does not exercise every deferred-73/rearm branch or physical radio/PTT. Evidence is under `bridge_refactor_evidence/autoseq_rearm`.

CQ reply window extraction: `bridge_cq_window_ab.py` compares the three moved methods to an exact pre-extraction snapshot modulo the existing logging wrapper, using real UTC normalization/delta helpers with controlled mode/period collaborators. It checks arm/clear, FT8 pre-arm rejection, FT2 async tolerance and age rejection. Isolated legacy app A/B cases toggle AutoCQ in FT8 and FT2, confirm arm/clear diagnostic messages and HALT immediately, with CAT disabled, output BlackHole 2ch, monitoring off and network sandboxed. The app test does not verify a real stale CQ response or physical PTT. Evidence is under `bridge_refactor_evidence/cq_window`.

Directed report/self-echo guard extraction: `bridge_directed_report_ab.py` compares the two moved methods against the exact pre-extraction snapshot modulo the existing logging wrapper and uses real message/token rules for same and conflicting reports, report kind, active/recent/old self echo and case differences. Existing canonical self-echo comparison is case-sensitive; the baseline and moved version both retain it. Two isolated legacy app A/B cases inject conflicting synthetic FT2 reports for K1ABC and W1AW, observe the explicit suppression diagnostic and matching retained first message, with CAT, monitoring and TX off and network sandboxed. Real RF/self-echo was not exercised. Evidence is under `bridge_refactor_evidence/directed_report_guard`.

FT2 partner guard extraction: `bridge_ft2_partner_guards_ab.py` compares seven moved methods to the exact pre-extraction snapshot modulo the existing logging wrapper, using real message validation/token logic with controlled state collaborators. It checks partner lock, signoff addressed to the active partner, another partner, unresolved hash and signoff watch. Two isolated legacy app A/B cases exercise the adjacent FT2 resume path using directed synthetic RR73 after HALT, with CAT disabled, monitoring off, networking sandboxed and BlackHole 2ch output; a second HALT ends the test. The app test does not independently confirm every moved guard or on-air/PTT behaviour. Evidence is under `bridge_refactor_evidence/ft2_partner_guards`.
# TX message selection extraction

`DecodiumBridgeTxSelection.cpp` holds the unchanged `buildCurrentTxMessage` implementation. `tests/tools/bridge_txselection_ab.py` compares the saved original with the extracted implementation and exercises all six slots, out-of-range selection, CQ fallback, Hound mode, an active partner, and an empty CQ. `tests/tools/bridge_txselection_runtime_lab.py` checks all six values exposed by `currentTxMessage` in isolated before/after application sessions with CAT, monitoring and transmission disabled; the identical outputs are in `tests/bridge_refactor_evidence/txselection/`. This is not a physical radio or PTT test.
# TX slot guards extraction

`DecodiumBridgeTxSlot.cpp` contains unchanged TX slot selection and guard methods. `tests/tools/bridge_txslot_ab.py` compares the original snapshot with the extracted methods and exercises invalid and disabled slots, FT2 deferred-state cancellation, unchanged selection, CQ latch reset, transmission guards, and legacy backend selection through a controlled stub. The isolated application test `tests/tools/bridge_txslot_runtime_lab.py` compares valid, disabled and invalid slot selection before and after, with CAT, monitoring and transmission disabled. Matching outputs are saved in `tests/bridge_refactor_evidence/txslot/`. The stub is not a physical CAT or PTT test.
# TX timing controls extraction

The `setTxPeriod` and `setAlt12Enabled` methods now reside with TX slot controls. `tests/tools/bridge_txtimingcontrols_ab.py` compares their saved original implementations with the extracted code and checks normalization, duplicate updates, Hound restrictions and legacy calls using a controlled stub. `tests/tools/bridge_txtimingcontrols_runtime_lab.py` compares property changes in isolated application sessions with CAT, monitoring and TX disabled; results are saved in `tests/bridge_refactor_evidence/txtimingcontrols/`. Hound and CAT hardware were not exercised in the application test.
# MAM queue promotion extraction

`DecodiumBridgeMamQueue.cpp` contains unchanged manual queue promotion and slot lookup. The controlled A/B harness `tests/tools/bridge_mamqueue_ab.py` compares both versions for inactive MAM, unknown callers, operator-selected reordering, repeated selection, full capacity and existing slot lookup. In addition, `tests/tools/bridge_mamqueue_runtime_lab.py` exercises two actual MAM slots and manual promotion from a two-caller queue in isolated before/after application sessions; matching results are saved in `tests/bridge_refactor_evidence/mamqueue/`. The lab briefly enables the sequencer synchronously, then halts it, with CAT and monitoring disabled and the output on BlackHole. It does not establish RF transmission.

The controlled A/B test also checks that a whitespace-only caller does not promote a slot or emit a queue change; both placements passed (`bridge_refactor_evidence/mamqueue-empty-ab.txt`). This additional case uses simulated queue collaborators.
# MAM clicked-station insertion extraction

`mamEnqueueClickedStation` now resides in `DecodiumBridgeMamQueue.cpp`. `tests/tools/bridge_mamenqueue_ab.py` compares the original and extracted functions with controlled slot and queue dependencies, covering inactive operation, self-call and duplicate rejection, frequency collision spacing, full capacity and frequency bounds. `tests/tools/bridge_mamenqueue_runtime_lab.py` exercises those insertion cases in isolated before/after application sessions, saving matching results under `tests/bridge_refactor_evidence/mamenqueue/`. The application lab briefly enables the sequencer synchronously, then halts it with CAT and monitoring disabled and output on BlackHole; it is not an RF transmission test.
# MAM caller promotion extraction

`mamPromoteFromQueue` now resides in `DecodiumBridgeMamPromote.cpp`. `tests/tools/bridge_mampromote_ab.py` compares original and extracted methods with controlled dependencies, covering skipped entries, duplicates, cooldown, busy frequencies, report clipping and late-report priority invocation. The isolated application A/B test `tests/tools/bridge_mamqueue_runtime_lab.py` verifies actual promotion from a multi-caller queue; matching results are in `tests/bridge_refactor_evidence/mampromote/`. It briefly enables the sequencer synchronously and halts it, with CAT and monitoring disabled and BlackHole output; this is not an RF test.
# MAM late-report recovery extraction

`mamPromoteLateReportRecoveries` now resides in `DecodiumBridgeMamRecovery.cpp`. `tests/tools/bridge_mamrecovery_ab.py` compares the original and extracted methods with controlled slot state, including expired and unready recoveries, duplicates, busy frequencies, successful recovery and capacity. The isolated application A/B lab `tests/tools/bridge_mamrecovery_runtime_lab.py` injects an FT2 plain report while the slot is full and confirms promotion ahead of a normal queued caller. `tests/tools/bridge_mamlate_runtime_lab.py` additionally lets an active FT2 slot reach its retry limit, injects a late R-report while another slot is occupied, and verifies recovery to TX4 ahead of a queued caller. The latter test uses BlackHole input and output with CAT disabled, including virtual audio transmission. Matching results are in `tests/bridge_refactor_evidence/mamrecovery/`; these do not prove RF interoperability.
# MAM slot pruning extraction

`mamPruneSlots` now resides in `DecodiumBridgeMamPrune.cpp`, with the existing shared cooldown read through an accessor. `tests/tools/bridge_mamprune_ab.py` compares original and extracted code with controlled state for stale cooldown, expired recovery, retry exhaustion, retained recovery and unaffected live slot. `tests/tools/bridge_mamprune_runtime_lab.py` checks removal of a manually logged synthetic slot while preserving another slot in isolated before/after application sessions; evidence is under `tests/bridge_refactor_evidence/mamprune/`. The application lab does not establish the elapsed-time or retry-expiry recovery path, and does not transmit over RF.

The existing `test_mam_tx_payload_policy` source-level regression test reconstructs the moved MAM functions in their original order and reads moved settings, QSO-limit and decode-click code from their new files. The full test passes after the extraction.

The twelve existing tests that reference `DecodiumBridge.cpp` were built and run after the split. Four initially failed because they still inspected the old locations of TX dispatch, CQ persistence, FT2 partner guards and worked-status enrichment. Those checks now inspect the corresponding extracted files and all twelve tests pass across the initial run and targeted rerun.

On 2026-11-10 a further source-reference audit found stale locations in the liquid-glass, rigctl dashboard, MAM and RTTY tests. Their checks now inspect the corresponding extracted modules; all four passed on targeted rerun, as did `test_ft2_qso_sim`. The first MAM rerun still missed the extracted autolog constant, which was corrected and rerun separately. These are local macOS results and do not replace real radio tests.
# MAM decode ingestion extraction

`mamIngestDecode` now resides in `DecodiumBridgeMamIngest.cpp`. `tests/tools/bridge_mamingest_ab.py` compares the complete original and extracted bodies after normalizing the logging and partner-inference wrappers. Isolated before/after application labs exercise a pending plain report via `bridge_mamrecovery_runtime_lab.py` and an R-report advancing an existing slot to TX4 via `bridge_mamingest_runtime_lab.py`; matching observations are saved in `tests/bridge_refactor_evidence/mamingest/`. The legacy source-level regression `test_mam_tx_payload_policy` also passes. The app labs use synthetic decodes, CAT and monitoring disabled, and BlackHole output; they do not establish RF behaviour or cover every decode branch.
# MAM slot logging extraction

`mamLogSlot` now resides in `DecodiumBridgeMamLog.cpp`. `tests/tools/bridge_mamlog_ab.py` compares the original and extracted methods using controlled state, checking exchange fields, one-time logging, empty calls and cooldown updates. The isolated before/after application lab `tests/tools/bridge_mamlog_runtime_lab.py` creates a temporary test logbook, logs a synthetic MAM slot, reads back its ADIF call and verifies another slot remains; matching output is under `tests/bridge_refactor_evidence/mamlog/`. CAT and monitoring are disabled and the output is BlackHole; no RF QSO is claimed.
# MAM dispatch and elastic-cycle extraction

MAM dispatch and Fox elastic timer functions now reside in `DecodiumBridgeMamDispatch.cpp`. `tests/tools/bridge_mamdispatch_ab.py` compares the extracted bodies to the original, normalizing only the shared helper wrappers. `tests/tools/bridge_foxelastic_ab.py` exercises elastic timer opening, waiting, in-flight deferral, dispatch and manual hold with controlled dependencies. The isolated app A/B lab `bridge_mamlate_runtime_lab.py` exercises actual FT2 MAM dispatch, virtual audio, retries and late recovery with BlackHole input/output and CAT disabled; matching results are in `tests/bridge_refactor_evidence/mamdispatch/`.

An app test revealed that macOS PCM completion used `finishModulatorIdlePlayback`, which did not notify the elastic cycle. That completion path now calls `foxElasticOnTxFinished` after a normal transmission, excluding tune and the earlier FT2-Link branch. `bridge_foxelastic_fix_runtime_lab.py` compares the pre-fix and fixed apps with FT2 elastic enabled: the former has no elastic-window log after virtual TX, and the latter does. Results are in `tests/bridge_refactor_evidence/foxelastic_fix/`. This test uses BlackHole input/output and CAT disabled; other platforms and RF were not tested.

`bridge_foxelastic_dispatch_runtime_lab.py` continues through a second virtual transmission. The baseline does not log an elastic dispatch, while the fixed app logs `FoxElastic: go after` before the second transmission; its before/after observations are stored alongside the opening-window evidence.
# MAM state and message extraction

The MAM mode and payload gates, multi-stream toggle and slot message builder now reside in `DecodiumBridgeMamState.cpp`. `tests/tools/bridge_mamstate_ab.py` verifies unchanged method bodies apart from the logging wrapper. Isolated before/after app labs exercise FT2 MAM activation and cleanup (`bridge_mamstate_runtime_lab.py`), a virtual TX/retry/late-report sequence (`bridge_mamlate_runtime_lab.py`), and FT8/FT4 virtual MAM transmissions (`bridge_mamstate_modes_runtime_lab.py`); matching results are stored in `tests/bridge_refactor_evidence/mamstate/`. The app tests use BlackHole and no CAT, and do not prove RF. `test_mam_tx_payload_policy` passes with its source checks updated for the new location.
# MAM waveform generation extraction

Multi-stream FT8/FT4/FT2 waveform generation now resides in `DecodiumBridgeMamWave.cpp`. `tests/tools/bridge_mamwave_ab.py` verifies the original implementation is unchanged after moving default arguments to the header. In isolated before/after application sessions, `tests/tools/bridge_mamwave_runtime_lab.py` generates two-stream WAV files for each mode, verifies invalid inputs fail and compares file sizes and SHA-256 hashes. The matching observations are stored under `tests/bridge_refactor_evidence/mamwave/`. CAT, monitoring and TX were disabled; WAV generation is not an RF test.

On 2026-11-10 the source comparison and current-app WAV generation were repeated for all three modes; sizes, hashes and result fields match the earlier baseline records. New current results are `FT8-current-repeat.json`, `FT4-current-repeat.json` and `FT2-current-repeat.json` in that directory; the earlier baseline was not rerun in this repeat.
# WAV output extraction

The float and int16 mono WAV writers now reside in `DecodiumBridgeWavIO.cpp`, and the MAM WAV export method resides with its waveform generator. `bridge_wavio_ab.py` checks the extracted source; `bridge_wavio_runtime_ab.py` compares actual bytes for clipped and boundary samples, invalid input and nested directories. Isolated before/after application labs generate two-stream FT8, FT4 and FT2 WAV files with matching sizes and SHA-256 hashes; observations are under `tests/bridge_refactor_evidence/wavio/`. The integer writer is verified in the controlled harness; the application lab exercises the float writer. No RF is involved.
# Single-mode TX waveform extraction

`buildTxWaveformForMessage` now resides in `DecodiumBridgeTxWave.cpp`, retaining shared FST4/Q65 timing through wrappers. `bridge_txwave_ab.py` compares the complete original and extracted implementations and exercises all mode families and error selection with controlled substitutes. `test_bridge_txwave_real_ab` additionally runs the original and extracted code with the real encoders and waveform generators, comparing every floating-point sample for FT2, FT4, FT8, JT65, JT4, JT9, MSK144, WSPR, FST4-15, FST4W-120 and Q65-15A; it checks the timing-helper fixture against the current source. `bridge_txwave_runtime_lab.py` compares real FT8, FT4 and FT2 app transmission sessions before and after. Evidence is in `tests/bridge_refactor_evidence/txwave/`. The other modes are not tested through the app UI, and the app lab uses BlackHole with CAT disabled rather than RF.

On 2026-11-10 the real waveform A/B test passed again. The accompanying TX dispatch source-contract test initially failed because monitoring and JTTY preflight had since moved; it now reads the extracted monitoring module and verifies `startTx` calls the preflight containing the JTTY block. Both CTests pass: `bridge_refactor_evidence/txwave-txguard-ctest.txt`.
# FST4/Q65 timing extraction

The FST4/Q65 period, samples-per-symbol and submode helpers now reside in `DecodiumBridgeTxTimingMath.cpp`. `test_bridge_txwave_real_ab` builds the original helper snapshot and the extracted production helpers separately, compares supported period/submode labels and compares full real waveform samples for all supported mode families. This tests generation in-process, not a radio transmission. Detailed CTest output is stored in `tests/bridge_refactor_evidence/txtimingmath/`.
# TX timing policy extraction

The payload estimates, start budgets and FT2/D3 timing boundaries now reside in `DecodiumBridgeTxPolicy.cpp`. `test_bridge_txpolicy_ab` compares the original and extracted functions across modes, periods, immediate-click options and FT2 conservative timing, using the same controlled lead-in dependency. `test_bridge_txwave_real_ab` continues to compare real waveform samples, and isolated before/after FT2 app sessions exercise a virtual audio transmission with matching observations in `tests/bridge_refactor_evidence/txpolicy/`. The platform-specific preprocessor branches are preserved; this run is on macOS and does not demonstrate RF transmission.
# TX lead-in and decode grace extraction

The TX lead-in and auto-decode grace calculations now reside in `DecodiumBridgeTxPolicy.cpp`. `test_bridge_txpolicy_ab` compares their original and extracted implementations as well as the surrounding policy, using the real shared FST4/Q65 timing helpers rather than a substituted lead-in. The `Q_OS_LINUX` branch is preserved; this execution is on macOS. Isolated before/after FT2 virtual-transmission observations are in `tests/bridge_refactor_evidence/txlead/`; they do not establish RF behaviour.
# TX audio preparation extraction

`ensureTxAudioPrepared` now resides in `DecodiumBridgeTxPrepare.cpp`; existing device, format and PCM helpers are reached through forwarding functions. `bridge_txprepare_ab.py` compares the full original and moved implementation after normalizing those forwards. Isolated before/after application tests cover FT2 single-stream TX, FT2 MAM retry/recovery, and two simultaneous FT4 streams; matching observations are in `tests/bridge_refactor_evidence/txprepare/`. `bridge_txprepare_special_runtime_lab.py` also verifies audio preparation and TX start for CW, a synthetic FT2-Link waveform and station telemetry before and after; it halts immediately after start and does not verify full playback or decode. `bridge_txprepare_cache_runtime_lab.py` observes two FT2 transmissions and confirms an actual cache hit in both versions. `bridge_txprepare_errors_ab.py` compares waveform failure, a missing device, PCM failure, fallback, cache reuse, mode invalidation and an empty FT2-Link plan using controlled audio dependencies. Existing MAM policy and real waveform tests pass. The app labs use BlackHole and CAT disabled; the simulated device-error cases do not demonstrate a physical device failure.
# TX PCM conversion extraction

Channel selection and float-to-PCM conversion now reside in `DecodiumBridgeTxPcm.cpp`. `test_bridge_txpcm_ab` compares the original and extracted code byte-for-byte over 192 combinations of sample rate, channel count, sample format and channel selection, including clipping and invalid inputs. Isolated before/after FT2 app transmission observations are saved in `tests/bridge_refactor_evidence/txpcm/`; the app test uses BlackHole with CAT disabled and is not an RF test.

File-reference cross-check on 2026-11-10: a literal search of Python and C++ test sources named 409 of 410 `DecodiumBridge*.cpp` files. The remaining name, `DecodiumBridgeTxPcm.cpp`, is supplied as a source to `test_bridge_txpcm_ab` in `tests/CMakeLists.txt`; that target was rebuilt and its CTest passed on this Mac. Literal filename references alone do not prove execution or A/B coverage of any other file, and this check does not count tests still needed.

The same CTest now also checks independently specified stereo Int16 samples, clipping and right-channel selection at 48 kHz, rather than relying solely on equality between the two implementations. The expanded test passed on macOS; it does not exercise an output device.

On 2026-11-10 all four configured `test_bridge_*` CTests were rebuilt and rerun: TX waveform, policy, PCM and format passed (4/4). These are focused tests; the separately documented application and audio loopback evidence is needed for runtime behavior.
# TX audio format selection extraction

`DecodiumBridgeTxFormat.cpp` now owns sample-format descriptions and output-format selection, with the existing platform-specific candidate ordering preserved. `test_bridge_txformat_ab` compares the original and extracted logic over formats, the actual output devices reported by Qt and an empty-device fallback, without starting playback. `bridge_txformat_runtime_lab.py` additionally compares the selected BlackHole format and cache reuse during two FT2 virtual transmissions in isolated before/after app sessions. Evidence is in `tests/bridge_refactor_evidence/txformat/`. The run is on macOS and does not prove Linux/Windows device behavior or RF.

# TX audio state and buffering extraction

Audio state/error names, TX buffer sizing and precompute retry timing now reside in `DecodiumBridgeTxFormat.cpp`. `bridge_txbuffers_ab.py` compares the original and moved implementations, including state/error values, buffer sizing and timing around slot boundaries with a controlled clock. The existing audio format and PCM tests continue to pass. Isolated before/after FT2 application sessions both observed two transmissions, the same BlackHole audio format and a cache hit; results are in `tests/bridge_refactor_evidence/txbuffers/`. These are virtual-audio observations on macOS with CAT disabled, not RF or cross-platform runtime tests.

# DXCC and CALL3 update extraction

`DecodiumBridgeLocalUpdate.cpp` now contains the CTY and CALL3 update methods, their payload validation and the asynchronous DXCC reload. `bridge_localupdate_ab.py` checks the moved source against its original snapshot and compares invalid/valid payload handling and sequential HTTP requests against a local TCP server. Before/after app sessions with external networking denied both reached the same CTY and CALL3 error states with CAT and transmission disabled; observations are in `tests/bridge_refactor_evidence/localupdate/`. This does not demonstrate a successful download, save or reload from a real update service.

`bridge_localupdate_success_lab.py` additionally builds two temporary instrumented application variants from the original method snapshot and the moved methods, changing only the update URLs to a local fixture server. Each isolated app exercises a failed first CTY URL, the successful fallback, CALL3 download, atomic file save, DXCC reload and CALL3 cache load; both saved files match the fixture SHA-256 and both variants produce identical counts and statuses. The original source, QML and production executable are restored after the test. Results are in `tests/bridge_refactor_evidence/localupdate/success-ab.txt`. The original snapshot is compiled in the extracted translation unit for this test; it is not the historical application binary. The fixture uses local HTTP, so real HTTPS services and certificates are not tested.

# SSTV QSO commit extraction

`commitSstvNewQso` now resides in `DecodiumBridgeSstvQso.cpp`; the extracted body matches the snapshot byte for byte. Both QML and legacy targets build and the SSTV QSO log and QML tests pass. `bridge_sstvqso_runtime_lab.py` temporarily exposes the real commit method via a test-only branch in the existing QML entry point and builds the original and extracted placements in turn. Two isolated application sessions each wrote one SSTV ADIF record, returned the same association identifier and rejected the duplicate; details are in `tests/bridge_refactor_evidence/sstvqso/runtime-ab.txt`. Source, QML and the standard QML executable were restored after the run. This tests the commit operation, not the Gallery preflight/association flow or external logging services; network and CAT were disabled.

# Manual synchronous TX scheduler extraction

The deferred manual synchronous TX decision, retry, cancellation and scheduler rearming methods now reside in `DecodiumBridgeSyncSchedule.cpp`. `bridge_syncschedule_ab.py` compares the entire moved source after normalizing the unchanged helper forwards and executes 140 mode/backend/slot combinations plus guard and receive-rendezvous cases against controlled dependencies. Before/after FT2 application sessions both requested manual TX, prepared audio and completed the same CQ on BlackHole with CAT disabled; see `tests/bridge_refactor_evidence/syncschedule/`. This virtual-audio check does not prove RF, every mode's live slot timing, or the delayed timer branches in the application.

`bridge_syncschedule_timer_ab.py` also exercises the original and moved methods with real Qt timers and controlled bridge dependencies: retry, generation and serial cancellation, mode/hold/transmission guards, and scheduler rearming. These controlled timer tests complement, but do not replace, live timing tests for all radio modes.

# Special operation and legacy synchronisation extraction

Fox/Hound and special-operation selection, transition deferral and legacy synchronisation now reside in `DecodiumBridgeSpecialOperation.cpp`. `bridge_specialop_ab.py` checks all moved source with the logging forward normalized and exercises state transitions with controlled legacy dependencies. `bridge_specialop_deferred_ab.py` checks TX, tuning, pending final 73 and log-prompt deferral, plus application of the queued transition. Before/after isolated application sessions both changed activity through 0, 1, Fox, Hound and 0 with identical states and no transmission; results are in `tests/bridge_refactor_evidence/specialop/`. The session runs on macOS with CAT and external networking disabled; it does not verify RF or every legacy backend condition.

The public-setter app A/B test now additionally supplies activity values -1 and 99, verifies clamping to 0 and 9, and returns to 0; both placements match with CAT and TX disabled. Results: `bridge_refactor_evidence/specialop-wrappers-boundaries-ab.txt`.

Public-setter follow-up: `bridge_specialop_wrappers_ab.py` runs the actual app
with `setFoxMode`, `setHoundMode` and `setNcontest` in each placement. It checks
Fox/Hound enable and disable, disabling the inactive mode, and contest/none;
all seven states match and CAT and TX remain off. See
`bridge_refactor_evidence/specialop-wrappers-ab.txt`. No RF transition was
performed. The full `run_bridge_refactor_ab.py` suite also passed locally on
2026-11-09; its comparisons and harnesses are not a substitute for per-path
runtime evidence.

# TX precompute and output-device resolution extraction

`DecodiumBridgeTxPrecompute.cpp` contains the moved output-device resolution, precompute scheduling and preparation methods. `bridge_txprecompute_ab.py` compares the entire source after normalizing the unchanged helper forwards and exercises the preparation guards, retry scheduling, remote PCM selection, failure path and duplicate timer requests. `bridge_txprecompute_explicit_lab.py` temporarily adds a test-only entry point, builds the original snapshot and moved methods in the extracted translation unit, and invokes precompute twice in isolated applications. Both sessions observe 120,960 samples for the same FT2 message and a cache hit, without CAT or transmission; source, QML and executable are restored afterwards. Results are under `tests/bridge_refactor_evidence/txprecompute/`. This checks sample count and cache reuse, not equality of all audio samples, actual RF, or Linux device selection.

# FT2-Link satellite half-duplex extraction

Satellite half-duplex configuration, state, CAT/PTT validation and RX restoration now reside in `DecodiumBridgeSatelliteHalfDuplex.cpp`. `bridge_satellitehd_ab.py` compares the entire moved source with the original snapshot and runs both with a fake CAT rig and real Qt timers, covering invalid settings, disconnected CAT, split/PTT guards, refusal, cancellation, unverified VFO, successful preparation and RX restoration. The existing satellite policy test passes. Isolated before/after application sessions show identical disabled, invalid and valid configuration states with CAT disconnected; evidence is in `tests/bridge_refactor_evidence/satellitehd/`. No real rig, PTT or RF was exercised.

# TX audio telemetry extraction

TX audio telemetry reset, periodic probing and summary now reside in `DecodiumBridgeTxAudioTelemetry.cpp`. The complete source is compared with the original after normalizing the logging forward. `bridge_txaudiotelemetry_ab.py` compares reset and summary with and without a fake sink; `bridge_txaudiotelemetry_timer_ab.py` uses real Qt timers and a simulated sink to exercise stall/underrun detection and tuning, TX and serial guards. `bridge_txaudiotelemetry_explicit_lab.py` temporarily exposes reset/summary in isolated before/after application builds and observes the same summary and reset state, with CAT and transmission disabled. Evidence is in `tests/bridge_refactor_evidence/txaudiotelemetry/`. A BlackHole transmission on this Mac used the legacy audio path and did not produce this telemetry summary, so the periodic callback is demonstrated with a simulated sink, not with an actual device or RF.

# TX playback completion extraction

`noteTxPlaybackFinished` now resides in `DecodiumBridgeTxFinish.cpp`. `bridge_txfinish_ab.py` compares its entire source after normalizing the logging forward and exercises error handling, final 73, TX4 retry cap, timer scheduling and cancellation with controlled dependencies and Qt timers. `bridge_txfinish_runtime_lab.py` builds the original and extracted placements with a temporary observation marker; isolated FT2 app sessions both prepare the same CQ and invoke playback completion on BlackHole without CAT. Results are in `tests/bridge_refactor_evidence/txfinish/`. The actual signoff and retry branches were tested with controlled substitutes, not over RF; the test restores the source and executable afterwards.

# TX slot boundary and buffer drain extraction

The slot-boundary stop and PCM-drain delay methods now reside in `DecodiumBridgeTxFinish.cpp`. `bridge_txboundary_ab.py` compares both moved methods with the original and exercises real Qt timers, serial/mode/tuning/TX guards, a memory buffer, duplicate scheduling and hard deadlines. Instrumented before/after FT2 sessions both invoke the drain check during a BlackHole CQ, with identical observations and CAT disabled. That legacy FT2 path did not invoke the boundary-stop method; its timer behaviour is therefore demonstrated with controlled dependencies rather than real audio or RF. Evidence is in `tests/bridge_refactor_evidence/txboundary/`; temporary source and executable instrumentation was restored.

# AutoCQ recent partner history extraction

Worked/abandoned partner cache mutation, clearing and completed-partner cooldown now reside in `DecodiumBridgeAutoCqHistory.cpp`. `bridge_autocqhistory_ab.py` checks the original and moved source after normalizing the logging/cache-prune forwards and compares cache/queue scenarios with controlled callsign and band helpers. `bridge_autocqhistory_runtime_lab.py` temporarily exposes the methods in isolated before/after applications and verifies worked/abandoned duplicate recognition, clearing, cooldown and caller removal using the real bridge; CAT and TX are off. Evidence is in `tests/bridge_refactor_evidence/autocqhistory/`. The test restores the normal source, QML and executable; it does not exercise an RF QSO.

# AutoCQ courtesy signoff extraction

The courtesy 73 start and release methods now reside in `DecodiumBridgeCourtesySignoff.cpp`. `bridge_courtesy_ab.py` compares the moved methods with the original after normalizing logging and the existing shared window constant and exercises QSO, queue, TX and repeat-limit guards, successful release and timer scheduling using controlled dependencies. `bridge_courtesy_runtime_lab.py` temporarily exposes the real methods in isolated before/after apps and verifies that the guard refuses a courtesy reply while transmission is marked active; neither CAT nor TX is started. Evidence is in `tests/bridge_refactor_evidence/courtesy/`. The positive courtesy exchange is covered with substitutes, not with RF or two live stations; source, QML and the executable are restored after testing.

# MAM directed caller ingestion extraction

Directed decode ingestion, active-partner lookup and caller removal now reside in `DecodiumBridgeMamCaller.cpp`. `bridge_mamcaller_ab.py` compares the original and moved source after normalizing unchanged logging/partner forwards, and tests directed calls, duplicate and hold guards, final 73 handling and base-call queue removal with controlled dependencies. `bridge_mamcaller_runtime_lab.py` temporarily exposes the real methods in isolated before/after applications: a synthetic directed call enters the actual queue, a 73 removes it, and worked/cooldown state is set identically. CAT and TX are disabled; no RF decode is claimed. The test restores source, QML and executable; evidence is in `tests/bridge_refactor_evidence/mamcaller/`.

# Manual TX hold extraction

Manual TX hold, resume-on-reply arming and hold release now reside in `DecodiumBridgeManualHold.cpp`. `bridge_manualhold_ab.py` compares the full original and moved methods and exercises mode/partner eligibility, queue clearing, asynchronous FT2 handling and preservation or clearing of deferred TX state with controlled dependencies. `bridge_manualhold_runtime_lab.py` temporarily exposes the real methods in isolated before/after app builds; both arm the same synthetic partner, clear the queue and release the hold with CAT and transmission off. Evidence is under `tests/bridge_refactor_evidence/manualhold/`; source, QML and the normal executable are restored after the test. This is not an RF resume-on-reply test.

# Deferred AutoSeq after TX extraction

The post-TX deferred AutoSeq application and FT2 one-shot methods now reside in `DecodiumBridgeAutoSeqAfterTx.cpp`. `bridge_deferredseq_ab.py` compares the original and moved source and tests stale mode/partner/message rejection, FT2 slot waiting, TX5 and one-shot legacy interactions using controlled dependencies. During extraction, an existing file name was accidentally reused; `DecodiumBridgeDeferredAutoSeq.cpp` was restored from its original comparison fixtures, its existing deferred and rearm A/B tests both pass, and both targets relink successfully.

`bridge_deferredseq_runtime_lab.py` temporarily exposes the real methods in isolated original/moved app builds. Both synthetic sessions apply TX2, record that they are awaiting the partner and do not transmit with CAT disabled. The recorded TX-enabled flag is true in both after legacy synchronisation, despite the one-shot method logging that it disarmed TX; the test directly seeds internal state, so this result is not by itself proof of a real operating regression. It remains a limitation requiring a realistic legacy-state reproduction before drawing conclusions. Evidence is in `tests/bridge_refactor_evidence/deferredseq/`; temporary instrumentation and the normal executable are restored.

Follow-up on the synthetic legacy observation: the diagnostic log shows `syncLegacyBackendState` rearming the legacy TX-enabled flag while AutoCQ is active. The partner-wait predicate nevertheless reports true in both builds. The isolated lab now makes one explicit `checkAndStartPeriodicTx()` call with monitoring state set, CAT disabled and BlackHole selected; neither build starts TX in that call. See `legacy-gate-ab.txt`. Since the lab directly seeds internal state and checks one synchronous call, it does not establish sustained behaviour over successive slots or real RF. No production behaviour was changed in this investigation.

# AutoCQ partner lock and caller queue extraction

Partner lock/update/restore and internal caller enqueue now reside in `DecodiumBridgeAutoCqLockQueue.cpp`. `bridge_autocqlock_ab.py` compares the original and moved source with the logging forward normalized and tests restoration plus active, recently worked, duplicate and full-queue guards with controlled dependencies. `bridge_autocqlock_runtime_lab.py` exercises the real methods in isolated before/after apps: both retain the partner, accept one new caller, reject active/duplicate callers and block restoration during manual hold, with CAT and transmission disabled. Evidence is in `tests/bridge_refactor_evidence/autocqlock/`; the successful restoration branch is covered by the controlled test, not an RF QSO. Temporary instrumentation and executable are restored.

# QSO state transition extraction

`canAdvance` and `advanceQsoState` now reside in `DecodiumBridgeQsoTransition.cpp`. `bridge_qsotransition_ab.py` compares the original and moved source after normalizing the logging forward, checks all 49 state pairs and exercises the actual shared sequencer remap rules, Quick QSO, selection rejection and census paths with controlled bridge dependencies. `bridge_qsotransition_runtime_lab.py` exercises the real methods in isolated before/after applications: normal TX1, Quick QSO remap to TX2 and Quick QSO TX3 signoff result in identical states with CAT and transmission disabled. Evidence is in `tests/bridge_refactor_evidence/qsotransition/`; temporary test instrumentation is restored. These checks do not demonstrate an RF exchange.

# FT8 automatic TX decode gate extraction

The FT8 automatic decode rendezvous, context, deadlines and audio-start validation now reside in `DecodiumBridgeFt8AutoGate.cpp`. `bridge_ft8bridgegate_ab.py` compares the moved source after normalizing unchanged helper forwards and exercises mode/backend/manual/remote guards and readiness/deadline decisions using the real gate class. `bridge_ft8bridgegate_runtime_lab.py` exercises the real methods in isolated before/after app builds: the bridge-owned path defers while the legacy-owned path excludes the gate, with CAT and TX disabled. Evidence is in `tests/bridge_refactor_evidence/ft8bridgegate/`. The existing FT8 gate wiring test was updated to find both this module and the previously extracted sync scheduler; this also fixes a stale source-location assertion exposed by running that test. No RF or live decoder latency claim is made.

# Decode and TX rendezvous extraction

The time-synchronised decode wait and deferred automatic TX scheduling methods now reside in `DecodiumBridgeDecodeTxRendezvous.cpp`. `bridge_decodetxwait_ab.py` compares the moved source with its original, normalising helper forwards, and exercises guard conditions, pending serial filtering, CPU grace, deadlines and cancellation using controlled dependencies and Qt timers. `bridge_decodetxwait_runtime_lab.py` exercises the real methods in isolated original and extracted app builds: both recognise a pending decode, exclude an early decode and bypass the bridge wait when the legacy backend owns scheduling. CAT and transmission remain disabled. Evidence is in `tests/bridge_refactor_evidence/decodetxwait/`; the lab restores its temporary instrumentation and executable. These checks do not demonstrate RF operation or live decoder latency.

# DX Cluster commands extraction

The DX Cluster connect, disconnect and AutoSpot commands now reside in `DecodiumBridgeDxClusterCommands.cpp`. `bridge_dxcluster_commands_ab.py` compares the full original and moved methods and checks null cluster, default and explicit endpoints, callsign propagation, connection, disconnection and AutoSpot with controlled dependencies. `bridge_dxcluster_commands_runtime_lab.py` runs the original and extracted methods through QML in isolated app sessions against a local TCP peer: both connect, set AutoSpot, disconnect and clear AutoSpot with identical results. CAT and monitoring are disabled and network access is confined to loopback. Evidence is in `tests/bridge_refactor_evidence/dxclustercommands/`. The test does not claim interoperability with an external DX Cluster service.

# FT2 asynchronous smart scheduler extraction

`scheduleSmartFt2AsyncTx` now resides in `DecodiumBridgeSmartFt2Scheduler.cpp`. `bridge_smartft2scheduler_ab.py` compares the entire original and extracted method and exercises guards, timer execution, late-slot rescheduling, generation and serial cancellation, manual hold and request coalescing with controlled dependencies and Qt timers. `bridge_smartft2scheduler_runtime_lab.py` temporarily exposes the real scheduler in isolated before/after app sessions: both schedule, coalesce and cancel a request, with CAT and transmission disabled. Evidence is in `tests/bridge_refactor_evidence/smartft2scheduler/`; temporary instrumentation is restored. The positive timer branch uses substitutes and is not a live RF transmission test.

# FT2 audio gating extraction

`onAudioLevelForFt2Gate` and `ft2AsyncGateRequirements` now reside in `DecodiumBridgeFt2AudioGates.cpp`. `bridge_ft2audiogates_ab.py` compares the complete methods and exercises the quiet threshold and saturation, mode and async guards, and default, clear and busy channel thresholds. `bridge_ft2audiogates_runtime_lab.py` temporarily exposes the real methods in isolated before/after app sessions: both return the same clear and busy results with CAT and TX disabled. Evidence is in `tests/bridge_refactor_evidence/ft2audiogates/`. Audio levels are synthetic; this is not a live radio test.

# RX and TX audio frequency extraction

`setRxFrequency` and `setTxFrequency` now reside in `DecodiumBridgeAudioFrequencies.cpp`. `bridge_audiofrequencies_ab.py` compares the full methods and exercises bounds, unchanged values, guard deadlines, settings writes, legacy backend calls and cache/split/precompute hooks using controlled dependencies. `bridge_audiofrequencies_runtime_lab.py` exercises the QML properties in isolated original and extracted app builds, confirming clamping and subsequent values with CAT and monitoring disabled. Evidence is in `tests/bridge_refactor_evidence/audiofrequencies/`; these tests do not establish actual CAT or RF behaviour.

# CAT and spot sharing configuration extraction

CAT, native CAT and spot sharing configuration methods now reside in `DecodiumBridgeSharingConfig.cpp`. `bridge_sharingconfig_ab.py` compares the original and extracted methods and exercises port validation, settings, permissions, routing change and CAT reconnect decisions, and virtual-port forwarding with controlled dependencies. `bridge_sharingconfig_runtime_lab.py` runs both app builds in isolated profiles and verifies defaulted port settings with all sharing disabled and CAT and monitoring off. Evidence is in `tests/bridge_refactor_evidence/sharingconfig/`. Active sharing, a real virtual port and a physical radio reconnect are not exercised by the runtime lab.

# Amplifier configuration extraction

Amplifier discovery and configuration now reside in `DecodiumBridgeAmplifierConfig.cpp`; the existing CAT/PTT port-conflict helper remains shared with startup. `bridge_amplifierconfig_ab.py` compares the original and extracted methods and exercises discovery, CAT/PTT port conflicts, configuration and manufacturer selection with controlled dependencies. `bridge_amplifierconfig_runtime_lab.py` verifies manufacturer and disabled configuration through isolated original and extracted app sessions with CAT and monitoring off. Evidence is in `tests/bridge_refactor_evidence/amplifierconfig/`; no physical amplifier discovery or serial exchange is claimed.

# Remote RX audio settings extraction

Remote RX volume and device methods now reside in `DecodiumBridgeRemoteRxAudio.cpp`. `bridge_remoterxaudio_ab.py` compares the complete original and extracted methods and checks defaults, bounds, gain, settings, notifications and monitor restart with controlled dependencies. `bridge_remoterxaudio_runtime_lab.py` checks the QML properties in isolated original and extracted app sessions with CAT and monitoring disabled. Evidence is in `tests/bridge_refactor_evidence/remoterxaudio/`; the monitor restart uses a substitute, and no remote audio playback is claimed.

# Delayed legacy audio start extraction

The delayed legacy bridge audio start arm, consume and cancel methods now reside in `DecodiumBridgeLegacyAudioStart.cpp`. `bridge_legacyaudiostart_ab.py` compares the complete original and moved methods and exercises valid, stale, shutdown, disabled bridge, TX authorization and cancellation paths with controlled dependencies. `bridge_legacyaudiostart_runtime_lab.py` exercises arming, stale serial rejection and cancellation in isolated original and extracted app sessions, with CAT and TX disabled. Evidence is in `tests/bridge_refactor_evidence/legacyaudiostart/`. The positive actual audio-start path and physical PTT are not exercised.

# Second instance extraction

`launchSecondInstance` now resides in `DecodiumBridgeSecondInstance.cpp`. `bridge_secondinstance_ab.py` compares the complete original and moved method and checks name validation, profile setup, shared CAT success and failure, and process launch failure with controlled substitutes. `bridge_secondinstance_runtime_lab.py` checks invalid-name rejection in isolated original and extracted app sessions with CAT and monitoring off. Evidence is in `tests/bridge_refactor_evidence/secondinstance/`. A successful real second-instance launch and shared CAT connection are not exercised by this lab.

# DecoPort hub refresh extraction

`refreshDecoPortHub` now resides in `DecodiumBridgeDecoPortHubRefresh.cpp`. `bridge_decoporthubrefresh_ab.py` compares the complete original and moved method and exercises background completion, single-flight coalescing, defaults and receiver destruction using a controlled worker. `bridge_decoporthubrefresh_runtime_lab.py` invokes the actual refresh in isolated original and extracted apps and checks the returned TCP and UDP rows with CAT and monitoring disabled. Evidence is in `tests/bridge_refactor_evidence/decoporthubrefresh/`. The scan does not open serial ports; ownership details can vary with the environment.

# Bridge self-check extraction

`runSelfCheck` now resides in `DecodiumBridgeSelfCheck.cpp`. `bridge_selfcheck_ab.py` compares the complete original and moved method and runs the actual diagnostic implementation with controlled normal, outdated, unsynchronised and clock-offset inputs. `bridge_selfcheck_runtime_lab.py` checks that the real method returns a valid list to QML in isolated original and extracted app sessions with CAT and monitoring disabled. Evidence is in `tests/bridge_refactor_evidence/selfcheck/`. The runtime check validates the result shape, while the controlled A/B test compares diagnostic content.

# Auto Call completion extraction

`noteAutoCallQsoCompleted` now resides with the other Auto Call methods in `DecodiumBridgeAutoCall.cpp`. `bridge_autocallcomplete_ab.py` compares the original and moved method and exercises disabled and empty-candidate guards, count updates, candidate clearing and the QSO limit notification. `bridge_autocallcomplete_runtime_lab.py` invokes the real method with synthetic state in isolated original and extracted app builds and verifies one completion is counted once with CAT and TX disabled. Evidence is in `tests/bridge_refactor_evidence/autocallcomplete/`; no on-air QSO is claimed.

# Remote link selection extraction

Remote link selection and the DecoLink source switch now reside in `DecodiumBridgeRemoteLinkSelection.cpp`. `bridge_remotelinkselection_ab.py` compares the full original and moved methods and exercises lazy selection, switching, disable and failed activation with controlled links. `bridge_remotelinkselection_runtime_lab.py` checks the disabled state in isolated original and extracted app builds with CAT and monitoring off. Evidence is in `tests/bridge_refactor_evidence/remotelinkselection/`; a connected remote radio and live audio handoff are not exercised.

# Rig telemetry extraction

CAT state accessors and `updateRigTelemetry` now reside in `DecodiumBridgeRigTelemetry.cpp`. `bridge_rigtelemetry_ab.py` compares the full original and moved methods and exercises state access, change deduplication and SWR/power limit hook calls with controlled dependencies. `bridge_rigtelemetry_runtime_lab.py` injects and restores synthetic values in isolated original and extracted app sessions with CAT and TX disabled. Evidence is in `tests/bridge_refactor_evidence/rigtelemetry/`; actual physical radio telemetry and TX protection are not exercised by the runtime test.

# Tune watchdog extraction

The Tune watchdog arm and disarm methods now reside in `DecodiumBridgeTuneWatchdog.cpp`. `bridge_tunewatchdog_ab.py` compares the complete original and moved methods and checks disabled settings, time limits, idle and active timeout callbacks, single-shot behaviour and disarming with a controlled `stopTune`. `bridge_tunewatchdog_runtime_lab.py` arms and disarms the real timer in isolated original and extracted apps with CAT and TX disabled. Evidence is in `tests/bridge_refactor_evidence/tunewatchdog/`; the app test does not key or stop a physical transmitter.

# Remote frequency extraction

Remote dial and band change methods now reside in `DecodiumBridgeRemoteFrequency.cpp`. `bridge_remotefrequency_ab.py` compares the original and extracted methods and checks invalid inputs, monitoring accumulation reset, band normalization and mode propagation with controlled dependencies. `bridge_remotefrequency_runtime_lab.py` verifies invalid inputs leave the frequency unchanged in isolated original and extracted app sessions with CAT and TX off. Evidence is in `tests/bridge_refactor_evidence/remotefrequency/`; a valid frequency change on a connected radio is not exercised.

# UDP client restart extraction

UDP client shutdown and restart scheduling now reside in `DecodiumBridgeUdpRestart.cpp`. `bridge_udprestart_ab.py` compares the original and moved methods and exercises shutdown of three controlled clients, deferred restart coalescing and repeated restart. `bridge_udprestart_runtime_lab.py` checks shutdown and the pending restart flag in isolated original and extracted app sessions with CAT and TX disabled. Evidence is in `tests/bridge_refactor_evidence/udprestart/`; the lab does not verify a full UDP exchange with an external application.

# Corrected UTC extraction

Corrected UTC epoch and time-of-day methods now reside in `DecodiumBridgeCorrectedTime.cpp`. `bridge_correctedtime_ab.py` compares the full original and moved methods and tests NTP gating, rounding, non-finite offsets and midnight wrapping against a controlled clock. `bridge_correctedtime_runtime_lab.py` tests a temporary synthetic offset in isolated original and extracted app sessions with CAT and TX disabled. Evidence is in `tests/bridge_refactor_evidence/correctedtime/`; this does not test an external NTP server or live slot timing.

# DecoPort RX audio input extraction

`onDecoPortRxAudio` now resides with remote RX settings in `DecodiumBridgeRemoteRxAudio.cpp`. `bridge_decoportaudioinput_ab.py` compares the full original and moved method and exercises source, TX, Tune and CW guards, sample forwarding and accounting with a controlled sink. `bridge_decoportaudioinput_runtime_lab.py` checks blocked cases in isolated original and extracted app sessions with CAT and TX off. Evidence is in `tests/bridge_refactor_evidence/decoportaudioinput/`; the positive forwarding path uses a substitute and no live remote audio is claimed.

# DecoPort credentials extraction

DecoPort key lookup, pending password migration, setting and clearing now reside in `DecodiumBridgeDecoPortCredentials.cpp`. `bridge_decoportcredentials_ab.py` compares the complete original and moved methods and tests migration, empty rejection, profile and root settings, endpoint propagation and clearing using controlled dependencies. `bridge_decoportcredentials_runtime_lab.py` uses a synthetic password in isolated original and extracted app sessions to verify set, lookup and clear with CAT and monitoring off. Evidence is in `tests/bridge_refactor_evidence/decoportcredentials/`; an authenticated remote connection is not exercised.

# Remote radio state extraction

`onDecoPortRemoteState` now resides in `DecodiumBridgeRemoteRadioState.cpp`. `bridge_remoteradiostate_ab.py` compares the full original and moved method and checks connection guards, telemetry masks, frequency tolerance, band update and SSTV stream selection using controlled dependencies with SSTV disabled and enabled. `bridge_remoteradiostate_runtime_lab.py` checks the disabled-remote path in isolated original and extracted app sessions with CAT and TX off. The SSTV buffer, ingress and receive-job tests also pass. Evidence is in `tests/bridge_refactor_evidence/remoteradiostate/`; active selection uses a substitute and live remote radio synchronization is not exercised.

# Legacy audio policy extraction

Legacy bridge audio eligibility, representative message and in-flight checks now reside in `DecodiumBridgeLegacyAudioPolicy.cpp`. `bridge_legacyaudiopolicy_ab.py` compares the full original and moved methods and tests mode/backend guards, message selection and playback windows using controlled dependencies. `bridge_legacyaudiopolicy_runtime_lab.py` checks the CW exclusion and synthetic in-flight state in isolated original and extracted app sessions with CAT and TX off. Evidence is in `tests/bridge_refactor_evidence/legacyaudiopolicy/`; no actual transmission or playback is claimed.

# DecoPort gateway controls extraction

Gateway start/stop and rig driver state now reside in `DecodiumBridgeDecoPortGateway.cpp`. `bridge_decoportgateway_ab.py` compares the complete original and moved methods and checks missing, closed and open rig states and gateway start failure/success and stop with controlled dependencies. `bridge_decoportgateway_runtime_lab.py` checks the stopped, disconnected state in isolated original and extracted apps. `bridge_decoportgateway_nokey_runtime_lab.py` also confirms that both real app builds refuse startup without a key; its temporary test instrumentation suppresses physical rig discovery and is restored afterwards. Evidence is in `tests/bridge_refactor_evidence/decoportgateway/`; successful gateway startup and radio access are not exercised by these runtime tests.

# RTL-SDR status extraction

RTL-SDR capability, status and RF-view accessors now reside in `DecodiumBridgeRtlSdrStatus.cpp`. `bridge_rtlsdrstatus_ab.py` compares the original and moved methods and exercises device state, running and RF-view guards, sample rate and frequencies with controlled dependencies. `bridge_rtlsdrstatus_runtime_lab.py` checks QML status values in isolated original and extracted apps with the receiver stopped. Evidence is in `tests/bridge_refactor_evidence/rtlsdrstatus/`; an active physical receiver is not exercised.

# RTL-SDR configuration extraction

Direct sampling availability and general receiver configuration checks now reside in `DecodiumBridgeRtlSdrConfig.cpp`. `bridge_rtlsdrconfig_ab.py` compares the full methods and checks index bounds, Blog V3/V4 identity, enabled state and demodulator guards with controlled dependencies. `bridge_rtlsdrconfig_runtime_lab.py` checks the actual direct-sampling method against a synthetic device list in isolated original and extracted apps. Evidence is in `tests/bridge_refactor_evidence/rtlsdrconfig/`; no physical receiver capture is claimed.

# RTL-SDR discovery extraction

`refreshRtlSdrDevices` now resides in `DecodiumBridgeRtlSdrDiscovery.cpp`. `bridge_rtlsdrdiscovery_ab.py` compares the original and moved method and exercises request coalescing, empty, device and error results with a controlled asynchronous enumerator. `bridge_rtlsdrdiscovery_runtime_lab.py` temporarily substitutes the USB enumeration result in isolated original and extracted app sessions, checks the reported device and status, and restores the implementation afterwards. Evidence is in `tests/bridge_refactor_evidence/rtlsdrdiscovery/`; physical USB discovery is not exercised.

# Rig discovery extraction

`detectConnectedRigs` now resides in `DecodiumBridgeRigDiscovery.cpp`. `bridge_rigdiscovery_ab.py` compares the full method and verifies empty and populated results and diagnostic reporting with a controlled detector. `bridge_rigdiscovery_runtime_lab.py` invokes the actual passive detector in isolated original and extracted app sessions and validates the result structure with CAT and monitoring disabled. Evidence is in `tests/bridge_refactor_evidence/rigdiscovery/`; detection does not establish a CAT connection or identify every possible radio.

# UTC period timer extraction

`armPeriodTimerForCurrentMode` now resides in `DecodiumBridgePeriodTimerArm.cpp`. `bridge_periodtimerarm_ab.py` compares the entire method after normalising existing helper forwards and exercises immediate modes, aligned timer starts and monitoring/session/mode cancellation using Qt timers and controlled dependencies. `bridge_periodtimerarm_runtime_lab.py` checks the real immediate WSPR arm/reset in isolated original and extracted app sessions with CAT and TX disabled. Evidence is in `tests/bridge_refactor_evidence/periodtimerarm/`; live radio timing is not exercised.

# JT/FST4 decode fallback extraction

`maybeDispatchBridgeOwnedJtDecodeFallback` now resides in `DecodiumBridgeJtDecodeFallback.cpp`. `bridge_jtdecodefallback_ab.py` compares the full original and moved method with helper forwards normalized, and checks mode, monitoring, TX, timing, audio buffer and duplicate guards with controlled dependencies. `bridge_jtdecodefallback_runtime_lab.py` checks the mode and monitoring guards in isolated original and extracted app sessions with CAT and TX off. Evidence is in `tests/bridge_refactor_evidence/jtdecodefallback/`; a live decoder dispatch is not exercised.

# QSO reset extraction

`resetQsoStateForNextSequence` now resides in `DecodiumBridgeQsoReset.cpp`. `bridge_qsoreset_ab.py` compares the complete method and checks automatic TX preservation, deferred state cleanup and notifications with controlled dependencies. `bridge_qsoreset_runtime_lab.py` exercises the actual reset on synthetic QSO state in isolated original and extracted apps with CAT and TX off. Evidence is in `tests/bridge_refactor_evidence/qsoreset/`; no on-air QSO is claimed.

# QSO finish extraction

`finishAutoSequenceQsoFromCompletedSignoff` now resides in `DecodiumBridgeQsoFinish.cpp`. `bridge_qsofinish_ab.py` compares the full method and tests courtesy handling, logging, queued callers, AutoCQ and stale timer guards with controlled dependencies. `bridge_qsofinish_runtime_lab.py` exercises an unlogged synthetic completion in isolated original and extracted apps with CAT and TX off. Evidence is in `tests/bridge_refactor_evidence/qsofinish/`; no on-air QSO or real log commit is claimed.

# RTTY rig mode extraction

RTTY CAT context and rig mode application now reside in `DecodiumBridgeRttyRigMode.cpp`. `bridge_rttyrigmode_ab.py` compares the complete original and moved methods with helper forwards normalized, and checks CAT routing, already-set mode and QSY deferral with controlled backends. `bridge_rttyrigmode_runtime_lab.py` checks the disconnected path in isolated original and extracted apps with CAT and TX off. Evidence is in `tests/bridge_refactor_evidence/rttyrigmode/`; no physical radio mode change is claimed.

# TX recording extraction

`saveTxRecordingAsync` now resides in `DecodiumBridgeTxRecording.cpp`. `bridge_txrecording_ab.py` compares the complete method and writes actual synthetic PCM WAV files, checking identical bytes, format and invalid-input rejection. `bridge_txrecording_runtime_lab.py` invokes the real asynchronous writer in isolated original and extracted app sessions and compares their temporary WAV bytes with CAT and TX off. Evidence is in `tests/bridge_refactor_evidence/txrecording/`; no live transmission recording is claimed.

# Hound TX preparation extraction

`prepareHoundTxSelectionForStart` now resides in `DecodiumBridgeHoundTxPrepare.cpp`. `bridge_houndtxprepare_ab.py` compares the full method and tests TX selection, invalid or empty payload, regeneration and rejection with controlled dependencies. `bridge_houndtxprepare_runtime_lab.py` checks the missing-Fox rejection in isolated original and extracted app sessions with CAT and TX off. Evidence is in `tests/bridge_refactor_evidence/houndtxprepare/`; no on-air Fox/Hound exchange is claimed.

# CAT backend switch extraction

`setCatBackend` now resides in `DecodiumBridgeCatBackendSwitch.cpp`. `bridge_catbackendswitch_ab.py` compares the full original and moved method and checks normalization, SSTV TX guard, manager disconnect and legacy routing with controlled dependencies. `bridge_catbackendswitch_runtime_lab.py` switches between TCI and the normalized Hamlib fallback in isolated original and extracted app sessions with CAT and TX off. Evidence is in `tests/bridge_refactor_evidence/catbackendswitch/`; switching a live physical CAT connection is not exercised.

# QSY command extraction

`qsyTo` now resides in `DecodiumBridgeQsyCommand.cpp`. `bridge_qsycommand_ab.py` compares the full method and checks operation ordering, mode guards and the frequency-preservation flag with controlled dependencies. `bridge_qsycommand_runtime_lab.py` checks a synthetic frequency and mode change in isolated original and extracted app sessions with CAT and TX off. Evidence is in `tests/bridge_refactor_evidence/qsycommand/`; no physical rig frequency change is claimed.

# VHF/UHF setting extraction

`setVhfUhfFeatures` now resides in `DecodiumBridgeVhfUhfSetting.cpp`. `bridge_vhfuhfsetting_ab.py` compares the full method and checks persistence, legacy synchronization, deduplication and notifications with controlled dependencies. `bridge_vhfuhfsetting_runtime_lab.py` toggles the setting in isolated original and extracted app sessions with CAT and TX off. Evidence is in `tests/bridge_refactor_evidence/vhfuhfsetting/`; live legacy operation is not claimed.

`refreshAudioDevices` and `restoreAudioDefaults` now reside in `DecodiumBridgeAudioRecovery.cpp`. `bridge_audio_device_recovery_ab.py` compares the complete methods and tests refresh arguments, TX/tune guards, missing defaults, input-only, output-only and both-device recovery with controlled dependencies. `bridge_audio_device_recovery_runtime_lab.py` invokes both methods in isolated original and extracted app sessions; both selected the same current macOS defaults, with CAT, monitoring and TX disabled. Evidence is in `tests/bridge_refactor_evidence/audiodevicerecovery/`. The guarded TX/tune and missing-device cases use substitutes; actual physical device loss and other operating systems were not exercised.

FT2-Link SMTP credential status, save, clear and test configuration now reside in `DecodiumBridgeFt2SmtpCredentials.cpp`. `bridge_ft2smtpcredentials_ab.py` compares the exact method bodies and exercises lookup, empty credentials, unavailable and failing backends, successful save/removal and test-recipient selection against a synthetic secure backend. `bridge_ft2smtpcredentials_runtime_lab.py` compares actual app validation of an empty password and missing SMTP host in isolated original/extracted sessions, without writing credentials or sending mail. Evidence is in `tests/bridge_refactor_evidence/ft2smtpcredentials/`. Real OS keychain storage and successful SMTP communication were not exercised.

Audio device identity persistence and legacy INI synchronisation now reside in `DecodiumBridgeAudioIdentity.cpp`. `bridge_audioidentity_ab.py` compares the full original methods and tests clearing stale IDs, unchanged devices, case-insensitive names, changed IDs, legacy sync and TX-cache invalidation with controlled devices and collaborators. `bridge_audioidentity_runtime_lab.py` compares two BlackHole input/output combinations in isolated original and extracted app builds, including their saved identifiers; CAT, monitoring and TX remain disabled. Evidence is in `tests/bridge_refactor_evidence/audioidentity/`. The controlled legacy callback and cache invalidation are not physical-device or RF tests.

Audio input/output runtime change handling now resides in `DecodiumBridgeAudioRuntimeChange.cpp`. `bridge_audioruntimechange_ab.py` compares the full original and moved methods and tests legacy ownership, monitoring disabled, TX deferral, legacy fallback, RX restart and output cache/precompute decisions using controlled collaborators. `bridge_audioruntimechange_runtime_lab.py` compares two virtual BlackHole input/output selections in isolated original and extracted app sessions; CAT, monitoring and TX are disabled. Evidence is in `tests/bridge_refactor_evidence/audioruntimechange/`. Active capture restart, sink changes and TX guards are exercised using substitutes, not a live radio or physical device.

TX dispatch authorisation and deferred-request invalidation now reside in `DecodiumBridgeTxDispatchGuard.cpp`. `bridge_txdispatchguard_ab.py` compares the exact methods and tests manual and stale requests, hold/startup and TX6 guards, AutoCQ/Multi-Answer authorisation, and invalidation with controlled collaborators. `bridge_txdispatchguard_runtime_lab.py` exercises stale TX6 blocking, explicit manual authorisation and generation invalidation in isolated original/extracted apps with CAT and transmission disabled. The source-wiring CTest `test_tx_dispatch_guard` also passes. Evidence is in `tests/bridge_refactor_evidence/txdispatchguard/`. No physical PTT or RF transmission was tested.

Startup mode detection and application from rig frequency now reside in `DecodiumBridgeStartupMode.cpp`. `bridge_startupmode_ab.py` compares the exact methods and tests invalid/unknown frequencies, expiry, pending and authoritative modes, TX deferral and frequency-preservation state using controlled time and band manager. `bridge_startupmode_runtime_lab.py` exercises actual band detection, deferred and applied mode in isolated original and extracted sessions with CAT disabled; the TX flag is synthetic and no physical PTT or rig operation occurs. Evidence is in `tests/bridge_refactor_evidence/startupmode/`.

Frequency update now resides in `DecodiumBridgeFrequencyUpdate.cpp`. `bridge_frequencyupdate_ab.py` compares the complete method and exercises small and large changes, legacy dial synchronisation, monitor recovery, RTL-SDR follow and remote echo suppression with controlled collaborators. `bridge_frequencyupdate_runtime_lab.py` changes the QML frequency property twice in isolated original and extracted app sessions; CAT, monitoring and TX are disabled. Evidence is in `tests/bridge_refactor_evidence/frequencyupdate/`. The legacy, remote and SDR paths use substitutes; no physical radio was retuned.

Configured CAT rig mode handling now resides in `DecodiumBridgeCatRigMode.cpp`. `bridge_catrigmode_ab.py` compares the complete method and exercises mode aliases, network and QSY guards, remote mode mapping, post-PTT reassertion and RTTY restoration using simulated backends and immediate timer callbacks. `bridge_catrigmode_runtime_lab.py` exercises the disconnected CAT path in isolated original and extracted app sessions. Evidence is in `tests/bridge_refactor_evidence/catrigmode/`; real CAT, radio commands, PTT timing and RF were not exercised.

CAT QSY request and post-QSY settle handling now reside in `DecodiumBridgeCatQsy.cpp`. `bridge_catqsy_ab.py` compares the complete methods and tests invalid offset rejection, disconnected updates, retry/settle, stale callbacks and RTTY resynchronisation with a simulated rig and manually triggered timer. `bridge_catqsy_runtime_lab.py` compares two disconnected QSY requests in isolated original and extracted app sessions with CAT and TX disabled. Evidence is in `tests/bridge_refactor_evidence/catqsy/`. Real radio CAT commands and real-time timer behaviour are not demonstrated.

Active CAT split frequency synchronisation now resides in `DecodiumBridgeCatSplitSync.cpp`. `bridge_catsplitsync_ab.py` compares the complete method and tests Doppler and satellite ownership, legacy/network/QSY guards, split disabled and calibrated TX frequency, including the zero sentinel, using controlled collaborators. `bridge_catsplitsync_runtime_lab.py` compares the disconnected CAT guard in isolated original and extracted app sessions. Evidence is in `tests/bridge_refactor_evidence/catsplitsync/`. Physical split VFO and CAT commands were not exercised.

CAT safe reset and reconnection selection now reside in `DecodiumBridgeCatRecovery.cpp`. `bridge_catrecovery_ab.py` compares the full methods and tests TX/tune guards, legacy ownership/fallback and release/reconnect order for each supported backend with simulated collaborators. `bridge_catrecovery_runtime_lab.py` exercises the TX and tune guards in isolated original and extracted app sessions; the flags are synthetic and no CAT port is opened. Evidence is in `tests/bridge_refactor_evidence/catrecovery/`. Physical serial release and reconnection are not demonstrated.

RX audio pause and resume around TX now reside in `DecodiumBridgeRxAudioTx.cpp`. `bridge_rxaudiotx_ab.py` compares complete method bodies and exercises native and simulated Windows paths, monitor-off behaviour, legacy tap and TCI recovery with controlled collaborators. `bridge_rxaudiotx_runtime_lab.py` compares monitor-off guards in isolated original and extracted app sessions. A two-session virtual FT8 test decoded the message after extraction; its first run detected audio at 13.56 s in the slot, so it was rejected for timing. A repeat decoded with first audio at 0.43 s. Evidence is in `tests/bridge_refactor_evidence/rxaudiotx/`. These are virtual-audio results, not RF, and the Windows path and delayed watchdog callbacks were not tested on Windows or live hardware.

CW and FT2-Link direct audio request handling now resides in `DecodiumBridgeDirectAudioTx.cpp`. `bridge_directaudiotx_ab.py` compares the full methods and exercises empty/busy input, CW speed bounds and QSY, silent FT2-Link input, satellite and normal start failure/success using controlled collaborators. `bridge_directaudiotx_runtime_lab.py` compares invalid/busy guards in isolated original and extracted apps. In the extracted build, `bridge_txprepare_special_runtime_lab.py` observed audio preparation and TX start for both CW and FT2-Link over BlackHole, with CAT disabled and an immediate halt; no full playback, RF or live satellite operation was verified. Evidence is in `tests/bridge_refactor_evidence/directaudiotx/`.

DecoPort hub row construction now resides in `DecodiumBridgeDecoPortHubRows.cpp`. `bridge_decoporthubrows_ab.py` compares the exact code and checks serial ownership, virtual ports, configured CAT and TCP/UDP states with controlled owners. `bridge_decoporthubrows_runtime_lab.py` compares the populated port list in isolated original and extracted app sessions with CAT disabled. Evidence is in `tests/bridge_refactor_evidence/decoporthubrows/`. The runtime test does not establish actual cross-platform process ownership detection or open serial ports.

DecoLink saved login, connection validation, audio profile and password removal now reside in `DecodiumBridgeDecolinkLogin.cpp`. `bridge_decolinklogin_ab.py` compares the complete methods and covers defaults, validation, profile bounds, saved-password reuse and removal using synthetic settings, secure store and link. `bridge_decolinklogin_runtime_lab.py` compares empty-credential handling and invalid profile defaults in isolated original and extracted app sessions; no real credential is stored or network connection made. Evidence is in `tests/bridge_refactor_evidence/decolinklogin/`. Actual OS keychain and remote DecoLink service are not tested.

DecoPort remote-source enable/disable and local CAT restoration now reside in `DecodiumBridgeDecoPortUseRemote.cpp`. `bridge_decoportuse_ab.py` compares the complete method and tests disconnected refusal, legacy RX handoff, remote mode selection and restoration of local CAT with a simulated link. `bridge_decoportuse_runtime_lab.py` compares disconnected selection in isolated original and extracted sessions with CAT and monitoring disabled. Evidence is in `tests/bridge_refactor_evidence/decoportuse/`. A live remote radio, connected-stream handoff and CAT/PTT operation were not exercised.

Lazy DecoPort discovery/link and DecoLink objects now reside in `DecodiumBridgeRemoteLinkObjects.cpp`. `bridge_remotelinkobjects_ab.py` compares the full methods and verifies object reuse, disconnection ownership and SSTV stream guards with simulated links and callbacks. `bridge_remotelinkobjects_runtime_lab.py` verifies object presence and identity in isolated original and extracted app sessions with CAT, monitoring and TX disabled. Evidence is in `tests/bridge_refactor_evidence/remotelinkobjects/`. Actual network connection, disconnection and remote SSTV streams were not exercised.

Auto Call candidate selection now resides in `DecodiumBridgeAutoCallSelect.cpp`. `bridge_autocallselect_ab.py` compares the full method and exercises latest/strongest/furthest priority, filters, worked stations, failure to arm and stale deferred callbacks with controlled collaborators. `bridge_autocallselect_runtime_lab.py` compares busy and non-CQ guards in isolated original/extracted sessions with CAT and TX disabled. The FT8 auto-TX gate CTest and an existing before/after manual-hold runtime lab pass after updating their source insertion anchor. Evidence is in `tests/bridge_refactor_evidence/autocallselect/`. Positive candidate selection and deferred retry were simulated, not verified on-air.

Auto Call WAV follow-up: `bridge_autocall_wav_runtime_ab.py` enables candidate selection in isolated before/after applications, feeds a synthetic FT8 CQ WAV and observes selection of K1ABC with TX enabled. Both placements passed and HALT left TX off; embedded rig control and CAT were disabled, immediate TX dispatch suppressed and output directed to BlackHole. This verifies the decoded-file-to-selection path, not an on-air QSO or deferred retry. Evidence: `bridge_refactor_evidence/autocall-wav-ab.txt`.

FT4 decode request and early-dispatch handling now reside in `DecodiumBridgeFt4Early.cpp`. `bridge_ft4early_ab.py` compares complete methods and exercises slot thresholds, TX/legacy guards, CPU pressure, backlog and queued request construction with a controlled worker. `bridge_ft4early_runtime_lab.py` compares monitor-off and early-slot guards in original and extracted app sessions, forcing the native backend with CAT and TX disabled. `test_tx_pipeline` also passes. Evidence is in `tests/bridge_refactor_evidence/ft4early/`. The worker and its queued callback are simulated in the focused A/B test; the runtime probe does not produce a live FT4 decode.

FT4 early-decode audio follow-up: `bridge_ft4early_audio_ab.py` injects a generated FT4 WAV into the real audio buffer, calls the actual early dispatcher and checks the worker output and decode list in both original and extracted placements. Both decoded and displayed `CQ K1ABC FN42`; CAT and TX were disabled. The probe forces the native backend and publication path on macOS, uses a synthetic file rather than live audio, and invokes the early dispatcher at a simulated slot time. Results: `bridge_refactor_evidence/ft4-early-audio-ab.txt`.

HALT handling now resides in `DecodiumBridgeHalt.cpp`. `bridge_halt_ab.py` compares the complete methods and exercises request cancellation, MAM cleanup, legacy abort and native stop with simulated collaborators. `bridge_halt_runtime_lab.py` compares HALT while idle in isolated original and extracted app sessions; the TX-dispatch and MAM CTests pass after source-path updates. A separate active CW/BlackHole A/B (`bridge_halt_active_ab.py`) found the same unresolved result in both versions: two seconds after HALT, `txEnabled` was false but the QML `transmitting` property was true, despite a logged playback completion. CAT was disabled. This is not evidence about physical PTT or RF; active HALT state is not proven correct and requires separate investigation. Evidence is in `tests/bridge_refactor_evidence/halt/`.

Active-HALT CW follow-up: when a legacy TX backend was selected, bridge-owned CW bypassed native `stopTx` cleanup; the CW-active flag could then make legacy-state synchronisation reassert `transmitting`. HALT now sends bridge-owned CW through native stop first, including output and PTT release, before stopping the legacy backend. In `bridge_halt_active_ab.py` the original retained TX state after two seconds while the corrected app was stopped; `bridge_halt_active_audio_lab.py` measured audible BlackHole samples before HALT and silence afterwards, with CAT disabled. Idle HALT A/B and the TX-dispatch, FT8-gate and MAM CTests pass. Evidence is in `tests/bridge_refactor_evidence/halt/cw-fix-*.txt`. This does not measure a physical relay or RF/PTT release.

Direct-audio HALT follow-up: a longer FT2-Link waveform exposed the same legacy-selected TX-state problem as CW; a short waveform had ended before observation. Bridge-owned CW, FT2-Link and telemetry now use the native stop path before legacy shutdown. Original/fixed active A/B sessions confirm the change for CW and FT2-Link. BlackHole captures measured nonzero audio before HALT and silence afterward for both, and an isolated telemetry start/HALT returned TX inactive. The TX-dispatch, FT8-gate and MAM tests pass. Evidence is in `tests/bridge_refactor_evidence/halt/direct-fix-*.txt`. CAT was disabled; physical PTT, RF, satellite VFO restoration and other OS runtimes are not demonstrated.

Telemetry HALT A/B follow-up: `bridge_halt_active_ab.py --kind telemetry` now also compares the original HALT fixture with the corrected method during an actual synthetic telemetry audio start in isolated legacy-selected app sessions. Both prepare audio and observe TX; two seconds after HALT the original still reports TX active, whereas the corrected method reports TX inactive. CAT is disabled and output is BlackHole; this checks software TX state, not captured silence or physical PTT. Evidence: `halt/direct-fix-telemetry-ab.txt`.

Telemetry HALT audio follow-up: `bridge_halt_active_audio_lab.py --kind telemetry` captures BlackHole during the corrected app's telemetry start and HALT. RMS was 0.636 before HALT and 0.0 afterwards, and TX was inactive. This is a corrected-version virtual-audio check, complementing the state A/B above; it does not capture the original version or measure physical RF/PTT. Evidence: `halt/direct-fix-telemetry-audio.txt`.

DecoPort remote TX admission, audio framing/pacing and stop now reside in `DecodiumBridgeDecoPortTxFrames.cpp`. `bridge_decoporttxframes_ab.py` compares the complete methods and exercises link admission, blocked TX, PCM clipping, frame sizes and PTT set/release with a simulated link and timer. `bridge_decoporttxframes_runtime_lab.py` compares disconnected admission in original and extracted app sessions with CAT and TX disabled. Evidence is in `tests/bridge_refactor_evidence/decoporttxframes/`. No remote radio, real-time network pacing or physical PTT was tested.

DecoPort automatic radio-driver selection now resides in `DecodiumBridgeDecoPortDriver.cpp`. `bridge_decoportdriver_ab.py` compares the complete method and checks absent ports, CAT conflict, missing model, unrecognised device and simulated successful open. `bridge_decoportdriver_runtime_lab.py` compares the no-device branch in isolated original/extracted apps by temporarily substituting an empty detector so no physical scan occurs. The no-key gateway A/B lab also passes after moving its safety override. Evidence is in `tests/bridge_refactor_evidence/decoportdriver/`. No real radio or serial-port open was tested.

Manual NTP synchronisation is also located in `DecodiumBridgeTimeSync.cpp`. `tests/tools/bridge_ntp_manual_ab.py` compares the original and extracted implementations with a simulated client: missing client, disabled synchronisation, public servers, and a custom server. `tests/tools/bridge_ntp_manual_runtime_lab.py` compares the actual application before and after extraction with isolated profiles, local-only networking, CAT and monitoring disabled; it verifies both the disabled warning and the manual request for `127.0.0.1`. The lab does not establish synchronisation with a real NTP server or exercise radio hardware.

`decoPortGatewayObject` and its CAT, rig and meter hooks are in `DecodiumBridgeDecoPortObject.cpp`. `tests/tools/bridge_decoportobject_ab.py` compares identical original/extracted bodies using synthetic drivers and CAT meters, covering lazy creation, credentials, routing, busy TX/tune guards and meter validity. `tests/tools/bridge_decoportobject_runtime_lab.py` compares actual isolated app instances and verifies gateway creation refuses to start without an authentication key; the rig detector is disabled and CAT/monitor are off. Real CAT, radio PTT, RF and live meter readings are not exercised by these tests.

RTL-SDR capture configuration, start, stop, retune, debounce and settings application now reside in `DecodiumBridgeRtlSdrCapture.cpp`. `tests/tools/bridge_rtlsdrcapture_runtime_lab.py` compares the unchanged original and extracted code inside isolated app instances, temporarily replacing the input's start/retune/stop methods with an in-memory receiver. It checks unchanged configuration, in-place retune, rate change and restart, stop, debounce and settings deferred during synthetic TX. The discovery lab was also rerun with its insertion point updated. These are simulated receiver tests; they do not demonstrate operation with a physical RTL-SDR or RF input.

FT2-Link SMTP submission and its address and DATA formatting helpers now live in `DecodiumBridgeFt2SmtpSend.cpp`. `tests/tools/bridge_ft2smtpsend_runtime_lab.py` compares the original and extracted implementations in isolated app instances against a synthetic loopback SMTP server. It checks missing-host and recipient validation, sender/recipient normalization, SMTP commands, dot stuffing, and the Sent notification. It sends no external email and does not test authentication, TLS, a real mail service or OS keychain access. Runtime test insertion anchors for CAT switching, SMTP credentials and DX cluster were updated.

The DecoPort remote-audio monitor is in `DecodiumBridgeDecoPortMonitor.cpp`. `tests/tools/bridge_decoportmonitor_runtime_lab.py` runs original and extracted implementations with isolated settings and CAT disabled, checking activation/deactivation against the Mac's default output and refusal of an intentionally missing device, with no audio samples or TX. The remote-audio settings lab was updated to preserve the later extraction of its audio input method and rerun. This does not verify audible playback, a real remote stream, or other operating systems.

The CPU/GPU process usage samplers and platform counter adapters now reside in `DecodiumBridgeProcessUsage.cpp`; constructor access is provided by the same functions with external linkage. `tests/tools/bridge_processusage_runtime_lab.py` compares the original and extracted sampling methods in isolated app sessions, checking initialization, a counter reset and bounded CPU/GPU values with CAT and TX disabled. The app comparison also forces the severe CPU-pressure threshold with an isolated synthetic sample. Linux DRM GPU usage calculations pass their dedicated data-driven test; Linux/Windows hardware counters are not verified. Platform-specific branches are preserved in the extracted source; only macOS runtime was exercised.

Q65 Doppler/CAT split integration and split-mode legacy persistence are in `DecodiumBridgeQ65Split.cpp`. `tests/tools/bridge_q65split_runtime_lab.py` compares the original and extracted methods in isolated app sessions with CAT disconnected, checking alias normalization, persistence for emulated/rig/off split and Q65 TX-frequency fallback. The dedicated Q65 Doppler test also passed. No real CAT frequency command, radio transmission or RF Doppler correction was exercised.

Standalone WSJT-X UDP client initialization and its signal connections are now in `DecodiumBridgeUdpInit.cpp`. `tests/tools/bridge_udpinit_runtime_lab.py` compares original and extracted code in isolated app sessions and checks creation of primary/secondary/tertiary clients, reuse, disabling secondary/tertiary and shutdown, with CAT/TX off and network limited to loopback. A/B checks also emit the Clear and Halt TX signals directly and verify their handlers. The packet-level A/B lab (`bridge_udpinit_wire_runtime_lab.py`) also sends Clear and Halt TX datagrams on loopback and checks both signals and clear handlers. For this isolated test only, the UDP bind is restricted to loopback to work with its sandbox; the production bind is restored after the test. An external logger and normal production bind were not tested. Test insertion points for SMTP, CAT backend and DX cluster labs were updated after the move.

Audio device refresh now resides in `DecodiumBridgeAudioRefresh.cpp`. `tests/tools/bridge_audiorefresh_runtime_lab.py` compares original and extracted behavior in isolated app instances: saved missing input/output identities remain unchanged, and empty selections resolve to available default devices. CAT, TX and monitoring are disabled. This does not establish audible playback or recording, nor exercise Linux Pulse/PipeWire. Test insertion anchors for device recovery and HALT were updated.

Shared DecoPort/RTTY PTT control is in `DecodiumBridgeDecoPortPtt.cpp`. `tests/tools/bridge_decoportptt_runtime_lab.py` compares original and extracted code in isolated instances: it checks RTTY, missing-CAT, active-TX and Tune guards, logical keying without hardware, and release after the actual three-second watchdog. CAT is disabled and no physical PTT or radio is used; actual rig routing remains unverified here. Remote-audio test insertion anchors were updated.

`decoPortPlayTxAudio` now resides in `DecodiumBridgeDecoPortTxAudio.cpp`. The A/B app lab `tests/tools/bridge_decoporttxaudio_runtime_lab.py` substitutes the output enqueue method temporarily and compares exact 12 kHz and interpolated 48 kHz PCM, source/TX/Tune/empty guards and watchdog renewal. No samples reach an audio device, CAT or RF; output-device selection, real playback and physical PTT are not proven by this lab. Remote-audio lab insertion points were updated.

DX callsign getter/setter now reside in `DecodiumBridgeDxCall.cpp`. The A/B app lab `tests/tools/bridge_dxcall_runtime_lab.py` compares a partner change, QSO/deferred-state resets, decoded-row matching, no-op selection and unresolved placeholder handling in isolated instances with CAT and TX disabled. It does not validate real over-the-air QSO sequencing or all partner-memory cases.

The post-QML startup service scheduler now resides in `DecodiumBridgeStartupServices.cpp`. `tests/tools/bridge_startupservices_runtime_lab.py` compares original and extracted legacy app launches with CAT and monitor disabled and local-only network; it checks that services start exactly once even after repeated requests and that the app reaches a ready legacy backend. It does not measure a startup-speed change, exercise live CAT auto-connect or suspend/resume recovery, or verify remote services.

Legacy PTT transition state, confirmation, timeout and release scheduling now reside in `DecodiumBridgeLegacyPttTransition.cpp`. The A/B app lab `tests/tools/bridge_legacyptttransition_runtime_lab.py` covers pending/confirmed/cleared state, command-dispatch confirmation, backend feedback filtering and actual feedback timeout with CAT off and no radio. The PTT policy unit test also passed. Physical PTT on/off, CAT feedback and the follow-up RX command are not exercised by this lab.

The app lab additionally checks that clearing a pending transition prevents its queued confirmation action, and that a synthetic audio-start failure clears the requested, pending and confirmed states. Original and extracted placements produced the same successful results with CAT and transmission disabled; see `bridge_refactor_evidence/legacy-ptt-transition-expanded-ab.txt`. These are simulated transition conditions, not physical radio or audio-device failure tests.
The lab was repaired for relocated helper declarations and rerun on 2026-11-09;
both placements passed the simulated state and timeout cases. Results:
`bridge_refactor_evidence/legacy-ptt-transition-repeat-ab.txt`.

Legacy backend construction and signal wiring now reside in `DecodiumBridgeLegacyInit.cpp`. The existing async-PTT audio delay constant was shared through `BridgeConfigConstants.h` to avoid diverging copies. `tests/tools/bridge_legacyinit_runtime_lab.py` compares original/extracted legacy app instances, verifies backend availability and reuse, warning and settings signal forwarding, and clearing of synthetic PTT state on a legacy PTT-off signal with CAT and TX disabled. This does not exercise live radio PTT or all legacy signal paths. The legacy PTT transition lab and full A/B suite were rerun.

The app lab now forces embedded legacy rig control disabled during initialization and explicitly checks `rigControlEnabled()` is false. After updating its baseline helper declarations, both placements passed backend reuse, synthetic signal forwarding and PTT-state cleanup with CAT and TX off: `bridge_refactor_evidence/legacy-init-rig-disabled-ab.txt`. Rig-control configuration is intentionally instrumented for isolation in this test.

`clearTxMessages` now resides with the other TX-message methods in `DecodiumBridgeTxMessages.cpp`. `tests/tools/bridge_cleartxmessages_runtime_lab.py` compares original and extracted app behavior for inactive synthetic QSO state, message/report clearing, pending sequence reset and selection of TX6 or TX1. CAT and TX are disabled; The additional `bridge_cleartx_active_ab.py` lab exercises active CW on BlackHole with CAT disabled and verifies nonzero PCM before reset and silence afterward in both versions; physical RF/PTT is not tested.

Legacy state synchronization now resides in `DecodiumBridgeLegacyState.cpp`, using shared wrappers for existing mode, device and level helpers. `tests/tools/bridge_legacystatesync_runtime_lab.py` compares original/extracted app instances on macOS with legacy RX, checking audio-frequency synchronization, re-entry protection, TX period, alternate option, RX/TX levels and Bridge-owned TX-message preservation with CAT/TX off. This focused test does not cover every synchronized field or a live radio.

The A/B lab now forces embedded rig control off and checks it is disabled; its baseline placement was updated with the moved helper declarations. Both placements passed the synthetic state checks again, recorded in `bridge_refactor_evidence/legacy-state-rig-disabled-ab.txt`. The rig-control policy is instrumented for isolation.

The AutoCQ enable/disable transition is in `DecodiumBridgeAutoCqRepeat.cpp`. `tests/tools/bridge_autocqrepeat_runtime_lab.py` compares original/extracted app behavior for enabling AutoCQ, requesting TX arming, disabling it, queue/deferred-state clearing and repeated disable. The test intercepts TX arming, disables CAT and monitoring, and selects BlackHole; it does not prove actual automatic transmission, final-73 logging or RF behavior.

`setMode` now resides in `DecodiumBridgeMode.cpp`. The A/B app labs `bridge_modeswitch_runtime_ab.py` and `bridge_modecleanup_runtime_lab.py` compare FT8, FT4, FT2 and Q65 periods, invalid-mode handling and synthetic decode-list cleanup with CAT/TX disabled. The `bridge_modeswitch_decode_ab.py` lab also decodes two FT8 WAV files around an FT8-to-FT4-to-FT8 switch and verifies list clearing and decoder recovery in both versions. Its initial failure was traced to an active blacklist; filters are bypassed only in the isolated test profile. It does not test live audio, CAT mode switching, RF or every mode transition.

RX monitor start/stop now reside in `DecodiumBridgeRxControl.cpp`. The A/B lab `tests/tools/bridge_rxcontrol_audio_ab.py` starts and stops legacy RX in isolated app instances, feeds a synthetic tone through BlackHole, and validates nonzero 12 kHz PCM in the recorded WAV; CAT and TX remain off. It verifies virtual-audio capture rather than physical radio reception or every RX mode. The active TX-clear lab insertion point was updated and rerun.

On 2026-11-10 the lab's old source insertion failed to compile because helper declarations had moved. The lab now includes the extracted module's existing declarations and forwarding macros in the baseline placement, and the complete A/B virtual-audio run passed again: `bridge_refactor_evidence/legacy-rxcontrol-repeat-ab.txt`.

`DecodiumBridgeAudioCaptureStart.cpp` contains the unchanged `startAudioCapture` method, with forwarding functions for its pre-existing file-local helpers. `tests/tools/bridge_audiocapture_start_ab.py` builds the saved pre-extraction method and the extracted method in turn and runs the same legacy RX recording probe through BlackHole 2ch with CAT and TX disabled. Both runs started and stopped RX and recorded non-silent 12 kHz mono PCM. This verifies RX recording across the refactor, but does not establish that this legacy probe traversed every capture-start branch; in particular, TCI, RTL-SDR, remote audio, watchdog recovery and actual radio input are not covered by this runtime comparison.

The app A/B lab was repaired on 2026-11-10 to include the moved helper declarations in the baseline placement, then repeated successfully. Results: `bridge_refactor_evidence/legacy-audiocapture-repeat-ab.txt`. As above, the recording probe does not by itself establish execution of every branch of `startAudioCapture`.

Dedicated capture follow-up: `bridge_audiocapture_sstv_ab.py` instruments the same method entry in the original and extracted builds. In both, an isolated SSTV receiver opened BlackHole 2ch, started its worker and processed audio chunks while a synthetic tone was sent to the virtual device, with CAT disabled. This confirms that the moved method was invoked; the test does not establish decoded tone content, TCI, RTL-SDR, remote input, watchdog recovery or RF. Two full-image Robot C12 attempts against the baseline did not pass: one completed with MAE 8.83 and p95 65 against thresholds 5 and 15, and a second exited before completion. Those outcomes are not a successful image-quality comparison.

Dedicated capture signal-level follow-up: during a virtual 1500 Hz tone the SSTV diagnostics reported nonzero signal RMS in both instrumented builds (before 0.21945, after 0.21960), with the capture method called and worker processing chunks. This confirms signal presence in the local virtual-audio test; it does not independently establish the received frequency or the previously failed image-quality thresholds.

`DecodiumBridgeAudioSink.cpp` now holds the unchanged `ensureAudioSink` implementation. The saved-body A/B comparisons rebuild the original and extracted versions and exercise legacy RX recording (`bridge_audiosink_recording_ab.py`) and instrumented dedicated SSTV capture (`bridge_audiosink_sstv_ab.py`). Both recordings contained non-silent 12 kHz mono PCM; both SSTV runs called the moved method and showed nonzero audio RMS from BlackHole 2ch (0.21970 in each run), with CAT disabled. The legacy recording probe does not by itself prove that it used this sink. CW, RTTY/JTTY, FT2-Link, SSB and DecoPort gateway taps, watchdog and physical RX are not established by these tests.

`bridge_audiosink_taps_ab.py` also exercises the actual sink's in-process signal connections in both original and extracted app builds: each delivered 240 synthetic samples to CW, RTTY and JTTY respectively, and the CW/RTTY/JTTY TX/tune gates prevented a duplicate delivery. CAT was disabled. The FT2-Link queued callback delivered no batch in this synthetic probe in either build; this is an unresolved test outcome, not evidence of working FT2-Link or of a product defect. SSB and DecoPort still need independent coverage. The injected signals are not demodulation or physical radio tests.

FT2-Link follow-up: the initial synthetic probe was confounded by queued samples from prior modes and legacy state refresh changing monitoring. With that queue drained and monitoring explicitly maintained during the synthetic interval, the same A/B application probe delivered exactly one 240-sample FT2-Link batch in both builds. This covers the sink's queued handoff under simulated active monitoring, not FT2-Link demodulation, actual monitor-state stability, radio hardware, SSB or DecoPort.

`bridge_audiosink_ssb_ab.py` compares original and extracted sink wiring with a test-only probe at `ssbMonitorAudio`: in each build local SSB delivers exactly 240 synthetic samples to the monitor method, while the same samples are blocked for remote source and another mode. The test leaves CAT/TX disabled; it verifies invocation and routing, not audio output device playback or AGC behaviour. DecoPort gateway routing remains to be tested.

`bridge_audiosink_gateway_ab.py` exercises gateway routing before and after extraction: both versions pass 240 samples with 12 kHz mono format only for local monitoring, and suppress empty data, stopped monitor, TX, Tune and remote-source cases. The isolated test temporarily replaces gateway `running()` with true and intercepts `pushRxAudio` before networking; original files are restored and the app rebuilt. It verifies the sink-to-gateway call and gates, not actual gateway startup, packet transmission, remote client reception or physical radio audio.

`DecodiumBridgeTimeSyncDispatch.cpp` contains the unchanged time-synchronised decode dispatch method. `bridge_timesyncdispatch_runtime_ab.py` compares original/extracted app builds with CAT/TX disabled: an obsolete session leaves pending data untouched and the legacy-owned RX path clears the pending snapshot in both. This probe does not cover native window alignment, deadline borrowing, deferred dispatch, actual live signal decoding or TX interruption. The pre-existing virtual WAV and RX tests remain separate evidence; they should not be treated as direct proof of these untested branches.

`bridge_timesyncdispatch_windows_ab.py` compares the exact saved and extracted method in a Qt event-loop harness with controlled sample counts and clock. Both builds select a 10-sample completed window and retain two current-slot samples, wait for and consume four missing samples after the boundary, preserve borrowed samples when a timeout leaves an incomplete window, and clear a pending window during simulated TX. The harness replaces the decoder and uses scaled synthetic windows; it is not an actual FT8 decode or physical audio/RF test.

`bridge_timesyncdispatch_decode_ab.py` additionally runs two isolated application builds through the moved window-dispatch method with 180,000 samples from a known 12 kHz FT8 WAV. Each decodes `CQ K1ABC FN42` with CAT and TX disabled. To exercise native window assembly on this legacy-RX environment, the test temporarily bypasses only the legacy-ownership early return in both copies; the original sources are restored and rebuilt. This demonstrates the native method-to-decoder path for that recorded signal, not the normal legacy path, live RF or every mode/window condition.

`DecodiumBridgeFt8Queue.cpp` holds the unchanged FT8 worker-request method. `bridge_ft8queue_decode_ab.py` compares original and extracted app builds using the known FT8 WAV signal, with CAT/TX disabled; both decode `CQ K1ABC FN42`. The test temporarily bypasses the preceding legacy-ownership dispatch guard to exercise the native queue on this Mac and restores/rebuilds original files. It does not establish RF reception, all request parameter combinations, CPU-pressure policies or the normal legacy decoder path.

`bridge_ft8queue_params_ab.py` instruments request construction in isolated original/extracted applications and intercepts the worker handoff. Both produce the same requests for normal RX, simulated TX and low-CPU operation: depth 4/2/2, AP 1/0/0, bounded thread count 1, symbol clamp 50, budget 1000 ms, and reduced 300–2700 Hz range in the low-CPU case. CAT remains disabled and TX is only a synthetic member state. This checks request construction, not decoding under actual load or radio transmission.

`DecodiumBridgeFt8Early.cpp` now contains the unchanged early FT8 preview scheduler. `bridge_ft8early_decode_ab.py` compares original and extracted applications with CAT/TX disabled, using the known FT8 WAV; both produce `CQ K1ABC FN42` via the early decoder. The lab temporarily bypasses only the legacy-TX ownership guard in both builds to reach this native preview on the Mac and restores/rebuilds the sources. The test supplies a complete stored WAV to the early request rather than a live partial slot; low-CPU, startup, backlog and timing gates need separate coverage.

`bridge_ft8early_gates_ab.py` compares actual isolated app builds at the 10,900 ms and 14,250 ms FT8 preview thresholds and with 119,999 versus 120,000 samples. Both versions keep the first preview unmarked before the time/sample threshold, mark it when reached, mark the second threshold without issuing a second preview, skip under simulated low-CPU mode and block while simulated TX is active. The test bypasses the legacy-TX ownership guard in both builds and uses synthetic samples/member state with CAT disabled; it does not measure live slot timing, verify CPU pressure detection or exercise physical TX.

`DecodiumBridgeFeedDecoder.cpp` contains the unchanged audio-to-decoder routing method for all supported modes, with its prior file-local helpers forwarded. `bridge_feeddecoder_ft8_ab.py` compares original and extracted app builds using a recorded FT8 signal: both decode `CQ K1ABC FN42` with CAT/TX disabled. The preceding legacy ownership guard is temporarily bypassed in both test builds to exercise the native path, then all files are restored and rebuilt. FT2, FT4, Q65, MSK144, WSPR, JT, FST4 and fallback dispatches, all guard combinations, live RF and normal legacy decoding are not established by this specific FT8 proof.

The FT8 lab was updated for moved helper declarations and rerun successfully in both placements on 2026-11-10; `bridge_refactor_evidence/feeddecoder-ft8-repeat-ab.txt`. The same temporary legacy ownership bypass applies.

`bridge_feeddecoder_ft4_ab.py` generates a clean FT4 WAV using the project encoder and compares isolated original/extracted builds through the native dispatch branch. In both, the FT4 worker returned `CQ K1ABC FN42` with CAT/TX disabled; the row did not appear in the observed application list within the test timeout in either build. The lab temporarily bypasses the legacy-ownership guard and injects synthetic state, so this observation alone does not establish a production UI defect or a successful end-to-end display. Other mode branches and live RF remain unverified.

On 2026-11-10 the lab was updated for the moved helper declarations and rerun: both placements decoded and displayed `CQ K1ABC FN42`, as recorded in `bridge_refactor_evidence/feeddecoder-ft4-repeat-ab.txt`. The test still bypasses the legacy-ownership guard and injects state; this later pass does not explain the earlier display timeouts or establish live RF behaviour.

The corresponding FT2 feed-decoder app lab was likewise updated for moved helper declarations and rerun on 2026-11-10. Both placements decoded and displayed `CQ K1ABC FN42`: `bridge_refactor_evidence/feeddecoder-ft2-repeat-ab.txt`. It uses synthetic input and test-only native publication/time-sync instrumentation, not live RF.

FT4 display follow-up: the earlier native-worker test was run with legacy TX ownership still active, so `onFt8DecodeReady` deliberately deferred non-FT8 list publication to the legacy mirror. With the native ownership and native publication branches both selected temporarily in the isolated test, `bridge_feeddecoder_ft4_ab.py` now observes `CQ K1ABC FN42` in the decode list in both original and extracted builds. All temporary instrumentation/branch selections are restored and the application rebuilt. This establishes the selected native path for one generated signal, not the normal legacy mirror or RF reception.

`bridge_feeddecoder_ft2_ab.py` generates an FT2 WAV with the project encoder and compares the original/extracted application routing: both the FT2 worker and the decode list produced `CQ K1ABC FN42`, with CAT and TX disabled. The test injects a forced snapshot and temporarily selects native list publication while the Mac backend remains legacy, then restores and rebuilds all files. It does not prove live radio RX, legacy mirror publication or FT2-Link streaming.

`bridge_feeddecoder_q65_ab.py` generates a Q65-15A WAV with the project simulator and compares original/extracted app routing. Both native workers and decode lists returned `CQ K1ABC FN42` with CAT/TX disabled. The lab injects a forced audio snapshot and temporarily selects native list publication while the backend is legacy; source files are restored and rebuilt. This does not prove RF reception, other Q65 submodes or the legacy mirror.

On 2026-11-10 the lab was updated for moved helper declarations and rerun successfully in both placements: `bridge_refactor_evidence/feeddecoder-q65-repeat-ab.txt`. The same synthetic-input and native-publication limitations apply.

`bridge_feeddecoder_msk144_ab.py` generates an MSK144 WAV with the project simulator and compares original/extracted app routing. Both native workers and decode lists produced `CQ K1ABC FN42` with CAT/TX disabled. The test injects a forced snapshot and temporarily selects native publication on this legacy-backend Mac; sources are restored and rebuilt. It does not establish RF reception or legacy mirror operation.

On 2026-11-10 the lab was updated for moved helper declarations and repeated successfully in both placements: `bridge_refactor_evidence/feeddecoder-msk144-repeat-ab.txt`. The synthetic snapshot and native-publication instrumentation remain in use.

FST4-15 investigation: `bridge_feeddecoder_fst4_ab.py` generated a project-simulator signal and exercised both original and extracted native routing. Neither worker returned the expected `CQ K1ABC FN42`; both reported unrelated rows, even in the stronger-signal attempt. The test therefore remains failing and must not be counted as a functional proof. CAT/TX were disabled, and native snapshot/publication paths were forced for this legacy-backend environment. Root cause is not yet established.

After updating its baseline helper declarations, the same FST4-15 lab was rerun on 2026-11-10 and still failed in both placements: `bridge_refactor_evidence/feeddecoder-fst4-15-failed-repeat-ab.txt`. It did not decode or display the expected message. This repeat does not distinguish an invalid test fixture/configuration from decoder behaviour.

Simulator investigation: at DT=0 the generated FST4-15 waveform has nonzero energy in the final second even though the reported transmission lasts 9.6 seconds; the simulator's negative circular shift wraps part of the signal to the end. The lab was changed to generate DT=-0.5 (zero shift) and explicitly prepend 0.5 seconds of silence, avoiding that wrap. The expected decode still failed for both placements: `bridge_refactor_evidence/feeddecoder-fst4-15-nonwrapped-failed-ab.txt`. The wrap is a fixture defect but is not sufficient to explain this decoding failure; no simulator or decoder production code was changed.

With the nonwrapped fixture and simulator noise at -10 dB, both placements decoded and displayed `CQ K1ABC FN42` in two consecutive A/B runs: `feeddecoder-fst4-15-noisy-ab.txt` and `feeddecoder-fst4-15-noisy-repeat-ab.txt` under `bridge_refactor_evidence`. This establishes this controlled FST4-15 routing scenario, not the precise reason the noise-free fixture fails or general performance. The simulator-generated noise is not independent validation; the forced native snapshot/publication instrumentation remains active.

FST4-15 investigation follow-up: the first simulator setup included fading/noise, so the A/B lab was repeated with 0 Doppler, 0 multipath delay, a clean signal and narrowed receive range; neither version decoded the intended message. The existing independent project Qt test `fst4_native_emit_bridge_writes_decoded_output` also SKIPs for an explicitly documented unstable synthetic FST4-15 fixture (run on the current build). This is evidence of an existing test limitation, not proof that the simulator and decoder have the same root cause, nor proof of on-air behaviour. That clean-signal configuration remained failing; the later nonwrapped noisy-fixture passes are documented above and do not resolve the clean-signal failure.

JT65 investigation: `bridge_feeddecoder_jt65_ab.py` generates a 48 kHz JT65 waveform with the project TX stack, samples it into a synthetic 60-second 12 kHz window, and compares the original/extracted native dispatch. Both workers returned no decoded rows and neither displayed the expected `CQ K1ABC FN42`; the test is explicitly failing. CAT/TX were disabled, and native snapshot/publication paths were forced. Correctness of the constructed fixture and the production decoder have not been distinguished; neither RF nor normal legacy mirror has been tested.

JT65 follow-up: the application A/B lab was repeated with deterministic background noise added to the synthesized JT65 window, instead of a noiseless signal. Both original and extracted workers again returned no rows for the expected message. The decoder consumes only the first 52 seconds of the supplied 60-second buffer; that and the synthetic generator/offset need independent fixture verification. The test remains failing, and this observation does not establish the cause or live-RF behaviour.

JT65 independent-fixture follow-up: `bridge_feeddecoder_jt65_reference_ab.py` uses the WSJT-X sample `130610_2343.wav` from https://downloads.sourceforge.net/project/wsjt/samples/JT9%2BJT65/130610_2343.wav, SHA-256 `5f1ac56197afc3373746edea12869ea35e2e6f3798980825c3af8103996bbea4`. The WSJT-X 2.0 tutorial identifies `CQ DL7ACA JO40` in that file. Both original/extracted native workers returned no rows under the isolated forced-snapshot test, so the expected decode remains failing. This narrows dependence on the project-generated waveform but does not establish whether app test configuration or decoder behaviour caused the result, nor does it verify the normal legacy path.

On 2026-11-10 the reference lab was updated for moved helper declarations and repeated with an explicit 200–3000 Hz search range and depth 3. Neither placement returned a decoded row; `bridge_refactor_evidence/jt65-reference-wide-failed-ab.txt`. The test still injects a forced snapshot and selects native publication, so the cause remains unproven.

The working-tree diff in `Detector/JT65Decoder.cpp` was audited on 2026-11-10: its contents were byte-equivalent to HEAD after normalising line endings, with no logic changes. Original CRLF line endings were restored exactly, removing an unrelated full-file whitespace diff. This does not explain the failed JT65 decode.

`bridge_feeddecoder_jt9_reference_ab.py` exercises the original/extracted native audio dispatcher with WSJT-X `130610_2343.wav` (SHA-256 `5f1ac56197afc3373746edea12869ea35e2e6f3798980825c3af8103996bbea4`): both decode and display `WB8QPG IZ0MIT -11` at 3196 Hz, and produce matching additional JT9 rows. CAT/TX were disabled; the lab injects a forced snapshot and selects native publication. This does not establish normal legacy mirror or live RF behaviour. The guide reference is https://www.yara.ca/Comms%20Trailer/Comms%20Trailer/WSJT-X%202.0%20User%20Guide.htm.

The lab was updated for moved helper declarations and rerun successfully on 2026-11-10; `bridge_refactor_evidence/feeddecoder-jt9-repeat-ab.txt`. The forced-snapshot and native-publication limitations remain.

`bridge_feeddecoder_jt4_ab.py` compares original/extracted native audio routing using a project-generated JT4A waveform with `CQ K1ABC FN42`. Both worker and decode list produced the expected message with CAT/TX disabled. A previous `CQ TESTA JN70` fixture reached the worker but did not appear in the list; using an actual callsign made the end-to-end lab pass, without proving exactly which filter rejected the former. Native snapshot/publication are selected temporarily; normal legacy mirror and live RF remain untested.

`bridge_feeddecoder_wspr_reference_ab.py` compares original/extracted native audio routing using WSJT-X `150426_0918.wav`, SHA-256 `0f1e41e6a94fefd3df9efd8234dd6342dfd7e6675700f70e79e69eec955537da`. The standalone `wsprd` decoded eight rows; both isolated app builds also decoded and displayed `ND6P DM04 30` with CAT/TX disabled. The lab injects the recorded snapshot and temporarily selects native publication on the legacy-backend Mac, then restores and rebuilds files. This does not establish live RF or legacy mirror behaviour. Sample: https://downloads.sourceforge.net/project/wsjt/samples/WSPR/150426_0918.wav.

FST4-60 independent-fixture follow-up: `bridge_feeddecoder_fst4_reference_ab.py` exercises original and extracted native routing using the WSJT-X sample `210115_0058.wav` (SHA-256 `0dd5f25cfba45835bcde54ee60fbff7500c95c4f7b9b25f98a57fe319013987f`) with the guide settings: 60-second period, deep decode, and 1000–1400 Hz range. Both workers decoded and the app displayed `CQ K9KFR EN71`; both raw result lists also contained numerous other candidate rows that were not independently verified. CAT/TX were disabled and native snapshot/publication were forced in this legacy-backend environment. This is evidence for FST4-60 routing; the separate FST4-15 clean-fixture failure, normal legacy reception and RF behaviour remain unverified. Sample and instructions: https://wsjtx.github.io/wsjtx/guide-full.html.

After updating moved helper declarations, the FST4-60 reference A/B lab passed again on 2026-11-10; `bridge_refactor_evidence/feeddecoder-fst4-reference-repeat-ab.txt`. The separate FST4-15 clean-fixture failure remains unresolved; the noisy fixture passed as described above.

JT65 tolerance investigation: `bridge_jt65_tolerance_probe.py` temporarily applies the same `ntol=150` instrument to both original and extracted dispatch, constrains the receiver to 700–1100 Hz and selects 975 Hz, then tests the same WSJT-X reference recording. Both workers returned empty rows and neither displayed `CQ DL7ACA JO40`; the probe deliberately fails. This experiment does not reproduce unmodified defaults and does not establish the underlying cause. The instrument and application sources are restored by the probe. CAT/TX were disabled.

JT65 synchronisation diagnostic: temporary logging in `JT65Decoder.cpp`, combined with the tolerance probe on the WSJT-X reference sample, showed the intended 700–1100 Hz dispatch found 12 and 15 candidates on its first two passes in both builds; neither produced the expected decoded row. Other earlier dispatches observed during startup used 0–50 Hz and had no candidates; these are separate from the forced reference dispatch. This establishes candidate acquisition in that test, not the identity or validity of any candidate, and leaves the later failure undiagnosed. The decoder instrumentation was restored and the application rebuilt.

JT65 candidate diagnostic: with temporary identical logging in the tolerance probe, both builds processed a candidate at 974.12 Hz (near the documented 975 Hz reference) on two passes with sync 8.30; `decode65a_compute` returned `nft=0` and a blank message in both. A second candidate at 985.84 Hz returned an unverified string and was not emitted as a row. The first candidate proximity does not by itself prove its identity, but directs further investigation to decoding after acquisition. Diagnostic changes were restored and the app rebuilt.

JT65 deep-decode investigation: `bridge_jt65_deep_probe.py` repeats the reference fixture with both temporary tolerance 150 Hz and explicit decode depth 3 applied identically to both original and extracted dispatch. Both workers returned no rows and neither displayed the expected message; this probe deliberately fails. Test source modifications were restored and the app rebuilt. Depth alone therefore did not resolve the observed failure under these test conditions.

UI action extraction: `DecodiumBridgeUiActions.cpp` contains clipboard writing and the setup/time-settings request methods; a forwarding logger preserves the existing messages. `bridge_ui_actions_ab.py` compares the original bodies with the extracted versions and exercises the Qt offscreen clipboard, tab values -1 and 4, both requested signals and log calls. `bridge_ui_actions_runtime_ab.py` runs both builds in isolated application sessions and verifies QML observes setup tab 4 followed by the time-settings signal; it invokes clipboard writing but does not independently read clipboard contents within the application. CAT/TX/monitor are disabled and network is loopback-only. This does not prove actual human interaction with the displayed dialogs.

Offline service extraction: `DecodiumBridgeOffline.cpp` contains the unchanged status getter and resume action. `bridge_offline_ab.py` compares v1.0.737 and extracted bodies and tests absent service, refused resume and successful resume with a substitute service. `bridge_offline_runtime_ab.py` runs both builds in isolated app sessions, toggles the actual map service offline, invokes resume and observes `true` to `false` in both. CAT, monitor and TX are disabled, and network is loopback-only; real remote service availability is not established. The full existing Bridge A/B suite passed immediately before this extraction, and these focused checks passed afterward.

Legacy decode-list preparation: the existing `bridge_legacyaugment_runtime_lab.py` was rerun against the current build. An isolated test profile with legacy backend active appended an FT8 row to its own ALL.TXT and observed `CQ K1ABC FN42` in `decodeList`, with CAT/TX disabled and network confined to loopback. This checks a synthetic ALL.TXT-to-list path, not every branch of `syncLegacyBackendDecodeList` or RF reception. `bridge_legacydelta_realmodel_ab.py` also passed against the real Qt model for ordering, period separators, snapshot guard, timer and invalidation in the original/extracted delta helpers. The 760-line sync method itself remains in the main Bridge pending extraction and A/B coverage.

Legacy decode-list synchronisation extraction: `DecodiumBridgeLegacySync.cpp` contains the unchanged pre-extraction method body preserved in `legacy_sync_before.inc`, with forwarding helpers for existing file-local dependencies. `bridge_legacy_sync_runtime_ab.py` builds both variants and uses isolated legacy-backed app sessions to add a synthetic FT8 row to ALL.TXT and observe it in the decode list; both passed with CAT/TX disabled and loopback-only network. The legacy delta A/B and real Qt model tests passed, and `test_mam_tx_payload_policy` passed after reconstructing the moved sync and earlier Auto CQ source in its source-level checks. The app proof covers an ALL.TXT receive path, not every filter, MAM, mode switch or live RF branch in this large method; further branch-specific proof remains needed.

Legacy sync duplicate-filter A/B: `bridge_legacy_sync_dedup_ab.py` builds the pre-extraction and extracted implementations and runs `bridge_displaydedup_runtime_lab.py` in isolated legacy-backed sessions. Two synthetic ALL.TXT FT8 rows with the same CQ and -20/-10 dB both appear in the raw list; after enabling filters both variants display one model row at -10 dB. CAT/TX are disabled and network is loopback-only. This does not exercise actual RF or all sync filter and mode-change branches.

Legacy sync mode guards A/B: `bridge_legacy_sync_guards_ab.py` builds original/extracted methods and invokes the real sync function in isolated app sessions with the legacy backend present. With RTTY selected and again with SSTV workspace active, preloaded sentinel decode lists stayed unchanged; the RTTY-to-FT8 mode change also completed in both builds. CAT and TX were disabled. This covers guard entry and preservation of the synthetic list; it does not inject pending legacy backend rows and therefore does not prove their clearing, nor a new FT8 decode after return.

Legacy queued-row and mode recovery A/B: `bridge_legacy_sync_queued_ab.py` temporarily adds a test-only helper to append a synthetic row to the real legacy band widget. In isolated sessions both versions selected RTTY, inserted one row, called the sync method and observed zero legacy rows afterward; after returning to FT8 and injecting a fresh synthetic legacy row, both observed `CQ W1XYZ FN31` in the bridge decode list. CAT/TX were disabled; this is widget-input testing, not a received FT8 waveform or RF. Instrumented application files are restored and rebuilt in `finally`.

Legacy sync CQ-only probe remains unresolved: `bridge_legacy_sync_cq_filter_ab.py` injects synthetic CQ and directed rows into the legacy band widget with CQ-only enabled. Both original and extracted builds returned the same empty displayed list, while diagnostic direct parsing returned the directed row but not the CQ; disabling ghost filtering and reversing insertion order did not change this. The probe intentionally fails, so it is not proof of CQ-only behavior or a demonstrated regression; the reason the CQ is absent from the input parsing remains unestablished. CAT/TX were disabled and temporary instrumentation restored.

Legacy CQ-only probe resolution: instrumentation identified the earlier rejection as an enabled user blacklist matching `K1ABC`, before the CQ-only filter. `bridge_legacy_sync_cq_filter_ab.py` now temporarily substitutes an empty user-filter snapshot only for the sandboxed test, retaining CQ-only. Both original and extracted apps parsed a synthetic CQ and a directed row; each displayed the CQ and excluded the directed row. The blacklist itself was not changed. CAT/TX were off. The test changes and rebuilds the filter-settings source temporarily and restores it in `finally`; it does not demonstrate RF or all filter combinations.

Legacy sync blacklist A/B: `bridge_legacy_sync_blacklist_ab.py` temporarily supplies a synthetic blacklist containing `K1ABC` in both isolated app variants and injects two CQ rows into the real legacy widget. Both discard the blocked CQ and display the allowed `CQ W1XYZ FN31`; CAT/TX are disabled and network is loopback-only. The user blacklist is not modified. Test-only instrumentation is restored and the app rebuilt. The full `run_bridge_refactor_ab.py --base v1.0.737` suite also passed after legacy-sync extraction.

Period timer extraction: `DecodiumBridgePeriodTimer.cpp` contains the original current method preserved in `period_timer_before.inc`, with its file-local log forwarded. `bridge_period_timer_runtime_ab.py` exercises SSTV suppression, a simulated FT8 slot transition and backward-clock reset using isolated application sessions; both variants agree with CAT/TX disabled. Existing `bridge_periodtimerarm_ab.py` and `bridge_timesyncdispatch_windows_ab.py` passed. These focused checks do not establish real RF, every watchdog path, or reception of FT8 audio through the moved timer; these remain to be verified.

Period timer FT8 audio A/B: `bridge_period_timer_ft8_audio_ab.py` injects the project FT8 WAV PCM into the bridge buffer, sets a completed slot and invokes the actual original or extracted period timer. Both isolated app builds dispatch the worker and display `CQ K1ABC FN42`; CAT/TX are disabled. To exercise the native path on this legacy macOS configuration, the time-sync legacy-ownership guard is temporarily bypassed equally in both builds, then restored. This demonstrates the timer-to-decoder path for the injected waveform, not normal legacy ownership, real audio-device capture, or RF.

Period timer count watchdog A/B: `bridge_period_timer_watchdog_ab.py` starts isolated app builds with CAT disabled, sets the TX-enabled flag directly without keying or playing TX audio, configures a one-period count watchdog in memory, and calls the original/extracted period timer. Both clear the flag, report the watchdog timeout and remain non-transmitting. This tests the count-based expiration path without physical PTT, RF, actual transmission or QSO-log-on-close behavior.

FT2 async timer extraction: `DecodiumBridgeFt2AsyncTimer.cpp` contains the pre-extraction method preserved in `ft2_async_timer_before.inc`; its log uses an existing forwarding helper. `bridge_ft2_async_timer_audio_ab.py` supplies an FT2 test waveform to the async ring and both variants decode and display `CQ K1ABC FN42` in isolated apps. The test temporarily forces native list publication in this legacy environment and also applies its existing time-sync guard bypass; CAT/TX are disabled. `bridge_ft2_async_timer_guards_ab.py` checks stopping outside FT2, short-buffer and busy-worker guards in both builds. These tests do not cover RF, audio hardware, every CPU/deadline setting, or normal legacy publication.

FT2 async callback extraction: `DecodiumBridgeFt2AsyncReady.cpp` contains the pre-extraction callback preserved in `ft2_async_ready_before.inc`, with existing helpers forwarded. `bridge_ft2_async_ready_audio_ab.py` drives the real async worker from an FT2 PCM fixture; both app variants decode and display `CQ K1ABC FN42`, with the test forcing native publication on the legacy Mac. `bridge_ft2_async_ready_guards_ab.py` verifies mode-change and SSTV late-result suppression and duplicate suppression using a correctly formatted synthetic worker row. CAT/TX are disabled. Other auto-sequencing, MAM, reporting and hardware/RF paths require further evidence.

Spectrum timer extraction: `DecodiumBridgeSpectrumTimer.cpp` preserves the pre-extraction method in `spectrum_timer_before.inc` with the existing logger forwarded. `bridge_spectrum_timer_cpu_ab.py` runs isolated apps and supplies a synthetic 1000 Hz PCM ring; after explicitly disabling GPU availability in the test, both variants produce 956 CPU FFT bins with a peak at approximately 999 Hz. CAT/TX are disabled. Two preliminary test configurations produced no CPU frame because GPU availability remained enabled; those attempts are not passing evidence. The successful test does not verify GPU rendering, audio device capture, remote waterfall or RF. Runtime A/B scripts using the old main-file insertion anchor now use `onUtcTimer`.

Repeated on 2026-11-10 with identical 956-bin output and peak frequency in both placements: `bridge_refactor_evidence/spectrum-cpu-repeat-ab.txt`.

Spectrum GPU handoff A/B: `bridge_spectrum_timer_gpu_handoff_ab.py` enables GPU availability in the isolated test, removes registered visual items and observes the fallback `panadapterPcmFrameReady` signal. Both original/extracted timers emit exactly one 4096-sample frame with expected 1000 Hz sine sample and 200–3000 Hz bounds. CAT/TX are disabled. This tests the PCM signal handoff when no direct PanadapterItem is registered; it does not test the direct-item path, actual GPU FFT or rendering.

Repeated on 2026-11-10 with identical results in both placements: `bridge_refactor_evidence/spectrum-gpu-handoff-repeat-ab.txt`.

Spectrum hidden-item A/B: `bridge_spectrum_timer_hidden_ab.py` registers an actual hidden `PanadapterItem` in isolated apps. Both original and extracted timers detect no frame consumer, emit no PCM frame, leave the computation serial unchanged and reset the last-frame timestamp. CAT/TX are disabled. This does not prove rendering or the direct handoff to a visible GPU item.

On 2026-11-10 the hidden-item and visible-item fallback A/B labs passed again. The hidden item received no frame; the visible item was recognised as a consumer and advanced the computation serial with GPU availability falling back to false. Results: `spectrum-hidden-repeat-ab.txt` and `spectrum-visible-repeat-ab.txt` under `bridge_refactor_evidence`. The offscreen run does not establish actual GPU rendering.

Spectrum visible-item unsupported-GPU A/B: `bridge_spectrum_timer_visible_fallback_ab.py` registers an actual visible `PanadapterItem` in an offscreen software-rendered `QQuickWindow`. Both original and extracted timers attempt the direct item path, advance the frame serial and mark GPU FFT unavailable when the item rejects unsupported GPU compute. CAT/TX are disabled. This establishes rejection and fallback selection, not an actual CPU frame after rejection or GPU rendering.

Spectrum visible-item fallback frame A/B: `bridge_spectrum_timer_fallback_frame_ab.py` keeps a real visible item in an offscreen software window, observes unsupported GPU compute rejection, then directly invokes a second timer tick with its probe timestamp set to suppress an immediate GPU retry and frame timestamp set beyond throttle. Both original/extracted methods emit a CPU FFT frame of 956 bins with a 999 Hz peak for the same 1000 Hz synthetic ring signal. CAT/TX disabled. This demonstrates selected fallback plus a subsequent CPU frame under controlled timing; it does not prove native timing between real ticks, actual GPU rendering, or hardware/RF.

FT2 async timer adaptive throttle A/B: `bridge_ft2_async_timer_adaptive_ab.py` sets adaptive mode, zero CPU pressure and an already-recent dispatch in isolated apps. Both original and extracted timer methods skip a new attempt while idle and enqueue one after a partner is set. The synthetic 45000-sample buffer is not used to establish a successful decode in this test. CAT/TX disabled; no RF.

FT2 async narrow/full range A/B: `bridge_ft2_async_timer_ranges_ab.py` instruments the timer in both builds and submits four controlled synthetic windows with an active partner at 1650 Hz. Both build requests for 1500–1800 Hz three times and 200–4000 Hz on the fourth pass, retaining 1650 Hz as the target. The test resets the pending flag between calls to isolate cadence; it is not proof of actual sequential worker completion, received decode, hardware or RF. CAT/TX disabled and instrumentation restored.

FT2 async sequential narrow/full A/B: `bridge_ft2_async_timer_sequential_ab.py` supplies a generated FT2 waveform and triggers each new attempt only after `asyncDecodeReady`; it does not reset the pending flag between callbacks. With the legacy state, period and async periodic timers stopped for this isolated test, both original and extracted methods completed four worker jobs, decoded `CQ K1ABC FN42` in all four, and requested 850–1150 Hz for three passes followed by 200–4000 Hz. A prior attempt without that timer isolation stopped after one job, for an unproven reason. The lab instruments frequency requests temporarily, disables CAT/TX and restores all modified production files. This is not live device capture or RF.

FT2 synchronous callback extraction: `DecodiumBridgeFt2SyncReady.cpp` preserves the pre-extraction callback in `ft2_sync_ready_before.inc`. `bridge_ft2_sync_ready_audio_ab.py` uses an FT2 WAV with forced native publication and observes the worker decode and display `CQ K1ABC FN42` in both isolated apps. `bridge_ft2_sync_ready_cache_ab.py` directly exercises normal hash-cache seeding, exclusion of a correctly parsed AP-7 rescue and suppression during SSTV in both variants. `test_ft8_auto_tx_decode_gate` passed after adapting its source boundary. CAT/TX were disabled; audio-device capture, RF and all callback branches are not established.

TX-enable extraction: `DecodiumBridgeTxEnable.cpp` contains the pre-extraction enable, manual request and message-disable methods preserved in `tx_enable_before.inc`, forwarding only the original log helper. `bridge_tx_enable_runtime_ab.py` compares both methods in isolated apps with CAT and monitoring off and immediate TX dispatch explicitly suppressed: startup guard, simulated enable/disable, invalid index and message-2 disable/re-enable all agree. `test_tx_dispatch_guard` and `test_mam_tx_payload_policy` passed after updating their source reconstruction for previously extracted files. The updated AutoCQ runtime A/B also passed, with enable=true stubbed to avoid keying. These tests do not establish physical PTT, actual TX audio or all QSO/AutoCQ branches.

FT8/common decode callback extraction: `DecodiumBridgeFt8Ready.cpp` contains the pre-extraction body in `ft8_ready_before.inc`, forwards its existing file-local helpers including asynchronous text batching, and keeps the 5 Hz dedup tolerance. `bridge_ft8_ready_audio_ab.py` compares both variants with an FT8 PCM fixture and both display `CQ K1ABC FN42`; `bridge_ft8_ready_ft4_audio_ab.py` compares both with an FT4 fixture and forces native publication and time-sync guard equally on the legacy Mac. `bridge_ft8_ready_guards_ab.py` checks stale session, stale mode and SSTV suppression. The FT8 gate regression passed and existing FT2 sync and FT4 dispatch labs were rerun after their temporary native-publication instrumentation moved to the new source. CAT/TX disabled; these tests do not cover all callback branches, auto-TX/RF, MAM, reporting, or every supported mode.

Q65 common callback A/B: `bridge_ft8_ready_q65_audio_ab.py` compares the original and extracted callback with the same project-generated Q65-15A waveform. Both native workers decoded `CQ K1ABC FN42` and published it to the decode list. The isolated legacy Mac test temporarily selects the native time-sync and publication paths for both variants, disables CAT and transmission, and restores the sources and production build afterward. This proves the exercised synthetic-audio path, not RF or normal legacy-mirror behaviour.

FT2 common callback A/B: `bridge_ft8_ready_ft2_audio_ab.py` compares the original and extracted common callback using the same generated FT2 waveform. Both native workers decoded `CQ K1ABC FN42` and displayed it. The test selects the native time-sync and publication paths equally on the legacy Mac, disables CAT and transmission, and restores sources and the production build. This is synthetic audio evidence, not RF or a normal legacy-mirror test.

MSK144 common callback A/B: `bridge_ft8_ready_msk144_audio_ab.py` compares original and extracted callback using the same project-generated MSK144 waveform. Both native workers decoded and displayed `CQ K1ABC FN42`. Native time-sync and publication paths were selected equally for the isolated legacy Mac test, CAT and transmission were disabled, and sources and production build restored. This establishes the exercised synthetic-audio path, not RF reception or normal legacy-mirror behaviour.

JT4 common callback A/B: `bridge_ft8_ready_jt4_audio_ab.py` compares the original and extracted callback using a synthetic JT4 waveform generated by the application. Both native workers decoded and displayed `CQ K1ABC FN42`. The isolated legacy Mac test selects the native time-sync and publication paths equally, disables CAT and transmission, and restores the sources and production build. It does not establish RF reception or normal legacy-mirror behaviour.

JT9 common callback A/B: `bridge_ft8_ready_jt9_reference_ab.py` compares original and extracted callback with the WSJT-X reference recording `130610_2343.wav` (SHA-256 `5f1ac56197afc3373746edea12869ea35e2e6f3798980825c3af8103996bbea4`). Both native workers decoded `WB8QPG IZ0MIT -11` and published it to the decode list. The isolated legacy Mac test selected native time-sync and publication paths equally, disabled CAT and transmission, and restored sources and production build. This is a reference recording test, not live RF or normal legacy-mirror evidence.

FST4-60 common callback A/B: `bridge_ft8_ready_fst4_reference_ab.py` compares original and extracted callback with WSJT-X recording `210115_0058.wav` (SHA-256 `0dd5f25cfba45835bcde54ee60fbff7500c95c4f7b9b25f98a57fe319013987f`). Both native workers decoded and displayed `CQ K9KFR EN71`. The isolated legacy Mac test selects native time-sync and publication paths equally, disables CAT and transmission, and restores sources and the production build. This does not establish live RF, normal legacy-mirror or FST4-15 behaviour.

General regression rerun after the period timer, FT2 callbacks, spectrum timer, TX enable and common decode callback extractions: `python3 tests/run_bridge_refactor_ab.py --base v1.0.737` exited 0 on macOS. This establishes the cases implemented in that suite; it does not resolve the separately documented JT65 and clean-fixture FST4-15 native decoding failures or prove every runtime branch.

Common decode callback frequency dedup A/B: `bridge_ft8_ready_dedup_ab.py` injects matching FT8 decode rows at 1500, 1503 and 1510 Hz into isolated application builds. Both original and extracted callbacks retain 1500 and 1510 Hz and suppress 1503 Hz under the five-hertz dedup rule. CAT and TX remain disabled; sources and production build are restored. This is a controlled decode-row test, not RF reception.

Common decode callback CQ-only A/B: `bridge_ft8_ready_cq_filter_ab.py` injects a CQ and a directed FT8 row with CQ-only enabled in isolated application builds. Both versions display the CQ, hide the directed row and register the directed row in the in-memory persistence dedup keys with the persistence worker active. This does not verify the eventual SQLite write. CAT and TX are disabled and sources and production build restored.

CQ-only persistence follow-up: `bridge_ft8_ready_cq_filter_ab.py` now opens each isolated SQLite database after the application exits and confirms both `CQ W1XYZ FN31` and the hidden `W1XYZ K2ABC -10` are saved. Both original and extracted callback versions passed, with CAT and TX disabled. This supersedes the earlier limitation concerning verification of the SQLite write for these two rows.

Worked calls accessors: `workedCallsigns` and `workedCount` moved unchanged from the main bridge to `DecodiumBridgeWorkedHistory.cpp`. The isolated `bridge_workedhistory_runtime_lab.py --variant a` was run against both placements, including an explicit count of one after synthetic ADIF import, lookup, search, edit and deletion. Both passed with CAT disabled and loopback-only network. Production build restored.

Common callback legacy ownership A/B: `bridge_ft8_ready_legacy_mirror_ab.py` checks an FT4 synthetic callback with actual legacy TX ownership active. Both versions clean the callback serial without adding the row directly to the native list, leaving legacy mirror ownership intact. CAT and TX were disabled and sources and build restored. This is an ownership/guard test and does not itself prove that the legacy mirror later displays the row.

FT4 legacy mirror follow-up remains failing: `bridge_ft8_ready_legacy_publish_ab.py` injects the same synthetic FT4 row into the callback and legacy widget. Both original and extracted callbacks behave alike and the legacy parser recognises the row, but neither isolated session publishes it after an immediate sync. The test fails intentionally; the reason (including possible test timing/state) is unproven, and this is not evidence of a refactor regression or of successful normal legacy FT4 display. CAT/TX were disabled; instrumentation and build restored.

FT4 legacy mirror full-snapshot follow-up: `bridge_ft8_ready_legacy_publish_ab.py` repeats the A/B test while explicitly invalidating the cached band revision before sync. Both original and extracted versions then publish the synthetic FT4 row from the legacy widget. `bridge_ft8_ready_legacy_immediate_ab.py` preserves the earlier failing immediate-sync scenario without forced invalidation; its timing or revision behaviour is still unresolved. CAT and TX disabled; instrumentation and build restored.

FT4 legacy revision diagnostic: `bridge_ft4_legacy_revision_diagnostic_ab.py` updates the immediate-sync probe for later helper extractions and reads the widget and Bridge revisions around insertion. In this run both placements published the row without forced invalidation: widget revision changed from 3 to 4, the cached revision was 3 before sync and became 4 afterwards. Results are in `ft4-legacy-revision-diagnostic-ab.txt`. The earlier failure was not reproduced; added instrumentation and timing may differ, so this does not explain the earlier outcome or prove it cannot recur. The row is synthetic; CAT and TX were disabled. Temporary changes were restored and the app rebuilt.

FT4 legacy original-script repair: both `bridge_ft8_ready_legacy_immediate_ab.py` and `bridge_ft8_ready_legacy_publish_ab.py` now declare the forwarding helpers moved by later extractions. Both repaired scripts completed in original and extracted placements and published the synthetic row (`ft4-legacy-script-repair-ab.txt`). The first does not force revision invalidation; the second does. These are two passing controlled runs, not an explanation of the earlier intermittent failure or a live RF test. CAT/TX were disabled, temporary instrumentation was restored and the app rebuilt.

The same missing helper declarations affected 13 other `bridge_ft8_ready_*_ab.py` scripts; their insertion scaffolding was updated and all 13 parsed successfully. The repaired dedup and FT4 synthetic-audio scripts passed both placements (`ft8-ready-helper-repair-samples-ab.txt`). The FT4 check decoded and displayed a synthetic WAV; the dedup check retained 1500 and 1510 Hz. The repaired guards, CQ-only and my-call-only A/B scripts also passed both placements (`ft8-ready-filter-repair-ab.txt`), covering session/mode guards and the respective filtering decisions. All eight remaining repaired scripts then passed both placements (`ft8-ready-helper-repair-remaining-ab.txt`): FT8 audio, FST4 reference, FT2 audio, JT4 audio, JT9 reference, legacy mirror ownership, MSK144 audio and Q65 audio. The audio/reference tests use local fixtures and the legacy mirror case checks ownership and serial cleanup, not publication of a live RF decode. Temporary app changes were restored and CAT/TX remained disabled.

The anchor inventory now also flags missing forwarding macros in scripts that
reinsert the saved FT8-ready fixture at the UTC timer marker. Synthetic missing,
complete and unrelated examples passed; the current inventory reports zero
such scripts. This narrow static heuristic is not a general compile or runtime
test and does not cover other fixture dependencies.

FT4 legacy immediate-sync resolution: the earlier lab helper called `QTextEdit::append` directly, bypassing the `DisplayText::lineAppended` signal that fills the real incremental delta queue. `bridge_ft8_ready_legacy_immediate_ab.py` now emits that signal after the synthetic insertion and does not invalidate the band revision; both original and extracted callbacks result in the row being published. This resolves the earlier failure as a test-instrumentation error for this scenario, not a demonstrated product regression. It remains a controlled widget/callback test rather than a live RF decode. CAT/TX disabled; sources and build restored.

My-call-only callback A/B: `bridge_ft8_ready_mycall_filter_ab.py` injects synthetic FT8 CQ and `9H1SR W1XYZ -10` rows with my-call-only enabled. Both original and extracted versions show the directed row, hide the CQ and persist both rows to the isolated SQLite database. CAT and TX disabled; sources and production build restored.

Mode period extraction: `updatePeriodTicksMax` and `availableModes` moved unchanged to `DecodiumBridgeMode.cpp`; `bridge_mode_period_ab.py` compares original and extracted implementations in isolated app sessions. Both return the same 16 modes and period ticks for FT8, FT4, FT2, Q65-30A, FST4-60 and WSPR, with CAT/TX disabled. Twenty-seven existing labs using the previous function as an insertion anchor were redirected to `onUtcTimer`; the FT4 native audio routing A/B lab was rerun successfully.

Mode harness follow-up: updated three `setMode` A/B harnesses to retain the newly appended period functions. `bridge_modeswitch_runtime_ab.py` passes on both original and moved `setMode`, observing FT8, FT4, FT2 and Q65 periods plus rejection of an invalid mode, with CAT/TX disabled.

Mode switching audio regression after mode-period extraction: `bridge_modeswitch_decode_ab.py` passed in both before and after `setMode` placements with the newly extracted mode-period functions retained. Both isolated sessions report old results cleared and decoding recovered from WAV, with CAT and TX disabled.

Multi Answer mode extraction: `setMultiAnswerMode` moved to `DecodiumBridgeMamState.cpp`, forwarding the existing log helper. `bridge_multianswer_mode_ab.py` confirms equivalent enable/disable and queue cleanup in isolated app sessions, CAT/TX disabled. The MAM state source comparison and 27-case `test_mam_tx_payload_policy` passed; `bridge_tx_enable_runtime_ab.py` also passed after its insertion anchor was updated. Auto-call exclusion and every live MAM transmission branch are not separately proven by this new test.

Multi Answer A/B branch follow-up: `bridge_multianswer_mode_ab.py` additionally checks that enabling MAM disables an active Auto Call and clears a manual hold, and that disabling MAM while Auto CQ remains active preserves the caller queue. Both original and extracted variants passed in isolated sessions with CAT/TX disabled. States are set in the test; this does not prove a live QSO or RF transmission.

Multi Answer slot lifecycle A/B: `bridge_multianswer_mode_slot_ab.py` runs the isolated MAM app lab with the original and extracted Multi Answer setter. Both create one synthetic FT2 MAM slot and clear it when multi-stream is disabled, with CAT, monitoring and transmission inactive. This covers the app slot lifecycle, not RF or an on-air QSO.

Manual TX dispatch extraction: `sendTx` and `reportJttyPanelTransmitResult` moved to `DecodiumBridgeSendTx.cpp`, with existing file-local helpers forwarded and both CMake targets updated. `bridge_sendtx_jtty_ab.py` verifies valid JTTY signal forwarding and rejection of empty/invalid slots with the real controller disconnected, CAT/TX disabled; `bridge_sendtx_jtty_result_ab.py` compares success, explicit error and fallback notifications. `test_tx_dispatch_guard` (9) and `test_ft8_auto_tx_decode_gate` (14) passed with source reconstruction updated. FT/JT manual scheduling, legacy delegation and actual audio/RF still need distinct A/B proof.

Manual FT8 deferred dispatch A/B: `bridge_sendtx_ft8_deferred_ab.py` places the app in a controlled opposite-parity slot and invokes TX1. Both original and extracted `sendTx` select TX1, arm the explicit request and defer it without becoming transmitting; the test invalidates the scheduled request immediately, with CAT disabled. This tests admission and deferral only, not later playback, legacy delegation or RF.

Manual FT8 invalid/disabled slot A/B: `bridge_sendtx_ft8_guards_ab.py` verifies both original and extracted dispatch reject an invalid TX index and a user-disabled TX2 without arming a manual request, deferring or transmitting. CAT disabled and profiles isolated. Stale partner payload handling is not covered by this test.

The lab baseline placement was updated for moved helper declarations and rerun on 2026-11-10; both placements passed, with evidence in `bridge_refactor_evidence/sendtx-guards-repeat-ab.txt`.

Manual FT8 stale partner A/B: `bridge_sendtx_stale_partner_ab.py` selects a synthetic RR73 for W1XYZ while K1ABC is active. Original and extracted dispatch both report the block and leave no TX request armed or transmitting; CAT disabled. This covers stale signoff rejection, not the separate repair/retry paths or RF.

Manual FT8 stale partner repair A/B: `bridge_sendtx_repair_partner_ab.py` starts with a TX1 message addressed to W1XYZ while K1ABC is active, in an opposite-parity slot. Both original and extracted dispatch regenerate the message for K1ABC, arm it and defer it; the test invalidates the request immediately with CAT disabled and no TX. This tests the repair/admission path, not later playback or RF.

Manual legacy dispatch A/B: `bridge_sendtx_legacy_delegate_ab.py` forces the CAT-suppression branch decision only inside both test copies of `sendTx`, while CAT remains disabled, and temporarily replaces `armCurrentTx` with a logging stub. Both versions reach the stub and clear manual authority without TX. This proves controlled delegation routing, not the real backend arm, PTT or RF. Test instrumentation and production build are restored.

Manual FT8 virtual loopback A/B: `bridge_sendtx_ft8_loopback_ab.py` runs two isolated app sessions on BlackHole with CAT disabled and loopback-only network for each source placement. Both received `CQ 9H1SR JM75`; captured peak 0.402 and initial audio at 0.38/0.46 s into the slot. Compact reports are in `tests/bridge_refactor_evidence/sendtx/`; temporary profiles and artifacts for this run were removed. This is virtual audio, not RF or physical PTT.

TX partner guards extraction: `forceRecentRogerReportSignoffIfNeeded` and `repairOrRejectStalePartnerTxMessage` moved to `DecodiumBridgeTxPartnerGuard.cpp` with existing helpers forwarded and both CMake targets updated. The isolated A/B labs `bridge_partner_guard_reject_ab.py`, `bridge_partner_guard_repair_ab.py` and `bridge_partner_guard_roger_ab.py` pass for stale RR73 rejection, TX1 repair and a recent R-report advancing to TX4 respectively. CAT/TX disabled; these controlled scenarios do not cover every retry branch or RF.

The rejection app lab additionally checks acceptance without modification of an active-partner payload, an empty payload, the Hound exception and a TX6 CQ payload. Both placements passed with matching results and no CAT/TX; see `bridge_refactor_evidence/partner-guard-admission-ab.txt`. These are direct synthetic guard calls rather than transmitted messages.

TX partner guards A/B follow-up: `bridge_partner_guard_old_roger_ab.py` confirms a 60-second-old R-report does not advance FT8 TX2 to TX4; `bridge_partner_guard_retry_ab.py` confirms a stale signoff can restore the previous TX2 step for the active partner. Both original and extracted versions match in isolated sessions, CAT disabled, immediate dispatch suppressed for the retry, and no transmission.

TX partner guard normal FT8 loopback A/B: `bridge_partner_guard_ft8_loopback_ab.py` runs isolated Decodium TX/RX over BlackHole with original and extracted guards. Both receive `CQ 9H1SR JM75`, with CAT disabled and loopback-only network. This proves normal CQ path virtual audio after extraction, not a partner-specific live QSO or RF. Compact reports are in `tests/bridge_refactor_evidence/tx_partner_guard/`; temporary profiles/artifacts cleaned.

Repeated on 2026-11-10: both placements again decoded the same CQ and exited successfully. The per-run timing and active-window measurements differ slightly, as expected for separate real-time runs; see the refreshed JSON reports in the same evidence directory.

On 2026-11-09 the repair and rejection A/B scripts were updated to insert their
fixture at the current source location, with the existing helper declarations;
their former insertion anchor had been moved by subsequent extractions. Both
were rerun successfully. The app repairs a stale TX1 partner message or blocks
a mismatched RR73 before dispatch in both placements, with CAT and TX off.
Results: `bridge_refactor_evidence/partner-guard-repair-reject-ab.txt`.
The same stale insertion anchor was fixed in the retry, recent/old R-report and
FT8 loopback scripts. Their 2026-11-09 A/B reruns passed: the three controlled
cases agree, and both two-session BlackHole runs decoded `CQ 9H1SR JM75` with
CAT disabled. Evidence: `bridge_refactor_evidence/partner-guard-followup-ab.txt`.
Virtual audio does not prove an RF QSO.

Fallback QSO log dialog extraction: `showLogQsoPromptDialog` and its local satellite-code helper moved unchanged to `DecodiumBridgeLogPromptDialog.cpp`; both CMake targets include it. `bridge_log_prompt_dialog_ab.py` opens the actual QWidget dialog in isolated app sessions, checks the synthetic call is displayed, a second request reuses the same dialog, and rejection closes and clears it; both variants pass with CAT/TX disabled. The prompt lifecycle A/B harness also passed. Accepting the dialog and saving its edited fields are not covered by the new runtime test.

Fallback QSO dialog Add A/B: `bridge_log_prompt_dialog_add_ab.py` creates an isolated test logbook, opens the real fallback dialog for a synthetic K1ABC QSO, edits the comment and clicks Add. Both original and extracted versions close the dialog and return exactly one saved K1ABC record with the edited comment through logbook search. CAT/TX disabled, test profile removed. Other editable fields and cluster/satellite paths are not established by this case.

The separate `bridge_log_prompt_cluster_offline_ab.py` test verifies the cluster spot control is disabled offline and still saves exactly one synthetic QSO with its edited comment. Both placements passed with matching results, CAT and TX disabled; `bridge_refactor_evidence/log-prompt-cluster-offline-ab.txt`. It does not test sending a cluster spot.

Fallback QSO dialog satellite/time A/B and UTC bug fix: `bridge_log_prompt_dialog_satellite_ab.py` saves synthetic LABSAT/LABMODE plus edited UTC start/end and comment in an isolated ADIF record with both dialog placements. Before the parser fix, explicit ` UTC` times were silently replaced by fallback times because replacing the ISO T also modified UTC; stripping the suffix first fixes this. The targeted log prompt harness passes UTC, ISO Z, time-only, reversed-time clamping and invalid-input fallback. The baseline comparator accepts only the deliberate ordering fix; CAT/TX disabled. Cluster transmission is not covered.

General suite after UTC parsing fix and latest extractions: `python3 tests/run_bridge_refactor_ab.py --base v1.0.737` completed with exit 0. Updated worked-history source reconstruction and retained an explicit baseline comparison; the prompt comparison allows only the documented UTC parsing order fix and tests common unchanged behaviours on both variants. Targeted dialog/harness tests separately verify the corrected UTC behaviour.

Fallback QSO dialog disconnected-cluster A/B: `bridge_log_prompt_cluster_offline_ab.py` confirms both dialog placements disable and uncheck the cluster-spot option when disconnected while still saving a synthetic QSO. CAT/TX disabled; no network spot or connected-cluster flow is tested.

Amplifier port conflict helper extraction: moved the unchanged CAT/PTT serial-port conflict helper into `DecodiumBridgeAmplifierConfig.cpp`; startup now calls its existing exported wrapper. `bridge_amplifier_port_ab.py` compares real helper bodies with simulated empty, matching CAT/PTT, free and null cases. `bridge_amplifier_port_runtime_ab.py` verifies both placements reject enabling the amplifier on fake CAT/PTT ports in isolated app sessions; CAT and TX disabled. Existing amplifier configuration A/B unit and app labs passed. No physical serial port or amplifier was opened.

Periodic TX scheduler extraction: `checkAndStartPeriodicTx` moved unchanged to `DecodiumBridgePeriodicTx.cpp`, with existing helper forwards and both CMake targets updated. `bridge_periodic_tx_guards_ab.py` passes for reentrancy, CW/JTTY, disabled monitoring and manual hold, CAT/TX disabled. TX dispatch, FT8 gate and MAM policy source regression tests pass after reconstruction updates; 55 existing labs had insertion anchors moved and the manual-hold lab passed. A BlackHole FT8 loopback succeeded for both placements on repeat, but its manual immediate-TX trigger does not prove the periodic scheduler was reached. A first original-placement run decoded the message but recorded initial activity at slot offset 13.106 s and failed timing; evidence retained in `periodic_tx/before-timing-failure.json`, cause unknown. An earlier direct dispatch attempt failed to reach its intercepted start; the immediately following entry records the test correction and successful direct A/B, and later entries document scheduler-to-audio tests.

Periodic scheduler dispatch A/B follow-up: `bridge_periodic_tx_dispatch_ab.py` aligns the FT2 test invocation to the beginning of a slot after startup, and intercepts `startTx` before any audio or PTT. Both original and extracted schedulers reach the intercepted start and release the reentrancy guard, CAT disabled. The initial failure was explained by the active FT2 late-start protection; no production guard was bypassed. This is controlled dispatch evidence, not a full scheduler-to-audio/RF test; the previously recorded out-of-window audio event remains unexplained.

Periodic scheduler direct FT8 audio A/B: `bridge_periodic_tx_direct_audio_ab.py` invokes `checkAndStartPeriodicTx` in the isolated TX session at an aligned slot, with test-selected state and a diagnostic marker at its `startTx` call. Both original and extracted scheduler reached that marker and the second Decodium session decoded `CQ 9H1SR JM75` over BlackHole; first audio offsets were 0.397/0.408 s. CAT disabled, loopback-only network, the laboratory immediate-TX timing bypass remains active, and profiles/artifacts were cleaned while compact reports were retained. This covers the selected FT8 CQ path with controlled state, not physical PTT/RF or every scheduler mode. The separate earlier 13.106 s activity observation remains unexplained.

Periodic parity A/B and test correction: `bridge_periodic_tx_parity_ab.py` verifies both scheduler placements reach the wrong-slot guard and suppress dispatch, and also suppress dispatch while transmitting. The labs now use the actual convention `txPeriod=1` for even slots. Earlier FT8 deferred/partner-repair labs and the direct scheduler audio lab had assigned the opposite parity from what their descriptions intended; all four were corrected and rerun successfully. The corrected direct audio test decoded the CQ for both variants at 0.399/0.437 s into the slot with CAT disabled. The isolated earlier 13.106 s activity remains unexplained.

Periodic FT8 late-slot A/B: `bridge_periodic_tx_late_ab.py` invokes both scheduler versions in the selected FT8 slot after 12 s, records entry to the late guard, verifies a retry is scheduled and that no intercepted TX starts, then invalidates the test request. Both match with CAT disabled. This verifies scheduling of a retry, not its eventual transmission.

Periodic FT8 late retry cancellation A/B: `bridge_periodic_tx_cancel_ab.py` schedules a late-slot retry, calls the real halt operation, verifies the retry flag and generation invalidate, and waits 22 seconds across the next eligible slot. Neither original nor extracted scheduler calls the intercepted TX start. CAT disabled; startTx is stubbed and no physical PTT/RF is involved.

Sync retry timer fix: in the direct real-time retry lab, the original default timer reached its callback before the target slot; the periodic parity guard rejected the callback on two observed baseline attempts. `scheduleSyncTxAtNextValidSlot` now requests `Qt::PreciseTimer`, preventing early expiry. With this change `bridge_periodic_tx_retry_ab.py` reaches the intercepted start in both scheduler placements; `bridge_periodic_tx_cancel_ab.py` still confirms HALT cancels the retry across the next slot. The source A/B comparator allows only this intentional timer change, and sync scheduling unit and real-timer tests pass. CAT disabled and startTx intercepted; no RF proof. The separate 13.106 s audio observation remains unexplained.

Late FT8 retry full virtual audio A/B: `bridge_periodic_tx_late_audio_ab.py` invokes the scheduler after 12 s in the selected slot, verifies the late-guard marker precedes the actual dispatch marker, and decodes the ensuing `CQ 9H1SR JM75` in a second isolated Decodium session. Both original and extracted scheduler versions passed with BlackHole, CAT disabled and loopback-only network; first audio offsets in the successful rerun were 0.449/0.432 s. Test uses controlled initial state and the standard laboratory immediate-TX setting; it does not prove RF or physical PTT. Compact results retained and test artifacts cleaned.

Periodic MAM dispatch A/B: `bridge_periodic_tx_mam_ab.py` activates synthetic FT2 MAM state and intercepts `mamDispatchPeriod` and `startTx`. Both scheduler placements call MAM once with elastic=true, consume the elastic flag and do not call the generic start; CAT and TX remain disabled. The actual MAM waveform and RF are not exercised by this isolated routing test.

Periodic MAM FT8 app A/B: `bridge_periodic_tx_mam_audio_ab.py` runs the real MAM application lab with a diagnostic marker confirming scheduler-to-MAM dispatch. Both original and extracted scheduler placements enter and finish virtual TX on BlackHole and report the same W1AW stream at 1500 Hz, with CAT disabled. The lab observes application TX state and stream metadata; it does not capture/decode the audio, verify RF, or cover multiple simultaneous streams.

Periodic MAM FT8 virtual audio capture A/B: `bridge_periodic_tx_mam_capture_ab.py` captures BlackHole while both scheduler placements dispatch the W1AW stream. The observed audio peaks were 0.882 in both cases with 127/128 active 100 ms windows; stream metadata matched and CAT was disabled. This verifies audio presence, not decoding, precise waveform duration, multiple streams or RF. Profiles are removed by the lab.

Periodic MAM FT8 two-app A/B: `bridge_periodic_tx_mam_receive_ab.py` uses isolated Decodium TX/RX sessions over BlackHole, with scheduler dispatch marker and CAT disabled. Both original and extracted schedulers transmitted one 1500 Hz MAM stream and the receiver saved `W1AW 9H1SR JM75`; captured peaks 0.882 and 128 active 100 ms windows in each. Test profiles removed; this is virtual audio, not RF or multi-stream proof.

Periodic MAM FT8 two-stream two-app A/B: `bridge_periodic_tx_mam_two_receive_ab.py` sends W1AW at 1000 Hz and K1ABC at 2000 Hz simultaneously over BlackHole, then checks both messages in the isolated receiver database. Both scheduler placements passed with matching decoded messages, CAT disabled and RX filters bypassed only in the test profile. Earlier attempts with receiver filters active decoded W1AW but not K1ABC in both placements, although both frequency bands were present; the specific filter causing suppression was not independently identified. This is virtual audio, not RF.

Display frequency extraction: `bridge_display_frequency_ab.py` checks RX, tuning, split and satellite branches against the original body. `bridge_display_frequency_runtime_ab.py` compares both placements in isolated Decodium processes with simulated state changes, CAT and monitoring disabled; both returned 14074000 Hz in RX, tuning and TX state without split. No hardware PTT or RF was used; split and satellite branches are covered by the isolated C++ harness, not a real radio.

Fox OTP extraction: `bridge_verify_otp_ab.py` compares the original and extracted callbacks using a synthetic Qt verifier (empty inputs, default and explicit audio frequency, success and error). `bridge_verify_otp_runtime_ab.py` compares both implementations inside isolated Decodium processes against a local HTTP server, including success, HTTP failure, and empty-input suppression; timestamps in returned error URLs are normalised for comparison. The public OTP service, TLS and real credentials were not exercised. CAT and monitor were disabled.

Auto-sequencer extraction: `bridge_auto_sequence_runtime_ab.py` compares original and extracted function in isolated Decodium instances. With RX filters bypassed for the test, a directed synthetic FT8 report advances from TX1/progress 2 to TX3/progress 4; empty input, uncertain RR73 and own-message echo do not change state. CAT and monitoring were disabled; TX was enabled by the sequencer but no transmission or RF occurred. This covers selected branches only, not all 1299 lines.

The same isolated auto-sequencer A/B also exercised synthetic FT2 and FT4 directed reports: both implementations selected TX3/progress 4 in both modes, with CAT and monitoring off and no actual transmission.

The auto-sequencer A/B additionally repeated an identical FT8 report and injected a directed report from a different callsign during an active exchange; both implementations retained the same partner and TX3/progress 4. The test uses synthetic input and no RF.

The sequencer runtime A/B also checked a directed FT8 report while manual TX hold was active; both versions kept TX disabled and the QSO state unchanged. This was simulated with CAT and monitor disabled.

The sequencer runtime A/B additionally covered an FT8 grid (TX1/progress 2 to TX2/progress 3) and an R-report (TX2/progress 3 to TX3/progress 4), yielding identical states in both placements. CAT and monitor remained off; no RF was transmitted.

Sequencer A/B with synthetic FT8 unrelated traffic retained the active QSO state. An RRR scenario produced TX2/progress 3 in both versions despite initial TX3/progress 4; this fixture reuses state across cases, so the result only establishes parity and needs a fresh isolated QSO lifecycle before judging correctness. CAT and monitoring were off.

RRR follow-up: supplying a synthetic previous directed report to the active partner makes both versions select TX5/progress 5. The earlier TX2 result was consistent with the existing safety barrier because the fixture lacked evidence of a report sent to that partner. This remains a constructed state, not a complete over-the-air QSO.

Log QSO extraction: `bridge_log_qso_runtime_ab.py` compares old and new placements in isolated Decodium instances through the actual QSO dialog; each creates a temporary logbook and saves exactly one K1ABC record with the same synthetic comment. CAT and monitor are disabled and no TX occurs. This covers the active logbook path; explicit legacy writer delegation remains to be verified.

Log QSO legacy delegation: `bridge_log_qso_legacy_delegate_ab.py` checks both placements in isolated app sessions with legacy ownership and writer setting enabled; both reached the legacy writer exactly once. The writer entry point was instrumented to return immediately, so this proves delegation only, not actual legacy file persistence. CAT and monitor were off.

Legacy writer real-file experiment `bridge_log_qso_legacy_real_ab.py`: the pre-extraction placement selected legacy ownership, but no K9ZZZ record was found in the isolated embedded-ft2/wsjtx_log.adi, even with a wait; test stopped before the extracted variant. This is an unresolved test setup/path or persistence issue, not proof of a regression. Temporary profiles were cleaned.

Legacy writer real-file follow-up: the previous experiment searched K9ZZZ while the bridge sent K1ABC. Once the synthetic call was aligned, `bridge_log_qso_legacy_real_ab.py` exercised the real legacy LogQSO acceptance and found one K9ZZZ entry in each isolated embedded-ft2/wsjtx_log.adi, before and after extraction. CAT and monitor were disabled. Diagnostic hooks only observed execution and were restored; the real writer was not replaced.

On 2026-11-10 the real-file A/B test was updated for moved helper declarations and to force and assert embedded rig control disabled. Both placements again wrote one synthetic record in their isolated legacy ADIF: `bridge_refactor_evidence/legacy-log-real-rig-disabled-ab.txt`. The forced rig setting is test instrumentation; actual operator logbooks are not used.

Shutdown extraction: `bridge_shutdown_runtime_ab.py` compares original and extracted shutdown in isolated Decodium profiles with CAT and monitor disabled; each closes without TX and a second process reads back the synthetic setting. This does not test disconnecting a real CAT device or active RF transmission.

Shutdown active-state A/B: `bridge_shutdown_active_ab.py` sets the in-memory transmitting flag in isolated CAT-disabled app sessions before shutdown. Both variants returned with TX false and reloaded a synthetic saved setting. No audio, CAT PTT, or RF transmission was performed; this does not prove hardware de-keying.

Modulator idle finish extraction: `bridge_modulator_idle_runtime_ab.py` compares old and new code inside isolated app sessions with a simulated active TX flag, CAT and monitor disabled; both ended with TX false. No actual audio or PTT was active. Delayed PCM, legacy PTT and FT2-Link paths need further functional coverage.

Modulator finish FT2-Link A/B: `bridge_modulator_idle_ft2_ab.py` first simulates an active playback hold and confirms TX remains active with deferred release; it then clears the hold and simulates FT2-Link completion, checking TX and FT2-Link flags clear and the acknowledgement guard is set. Both placements match, with CAT and monitor off. No physical PTT or real pending PCM was exercised.

Modulator finish legacy A/B: `bridge_modulator_idle_legacy_ab.py` simulates bridge-owned legacy TX and instruments the legacy PTT-off request entry; both implementations call it once and end with TX false, with CAT disconnected. The request executes normally but cannot operate a radio in this setup; physical PTT de-keying is not tested.

Modulator pending PCM A/B: `bridge_modulator_idle_pcm_ab.py` uses a real in-memory QBuffer containing 4096 bytes in isolated app sessions. Both versions defer while data remains, then clear TX state and release the buffer when its read position reaches the end. The buffer was advanced by the test; no audio device or RF was used.

Modulator finish two-app FT8 A/B: `bridge_modulator_finish_loopback_ab.py` used two isolated Decodium sessions over BlackHole with CAT disabled. Before and after, the receiver decoded CQ 9H1SR JM75, audio was present for 127 100ms windows and an instrumented marker confirmed finishModulatorIdlePlayback was reached. Evidence is in bridge_refactor_evidence/modulator_finish; this is virtual audio, not RF.

After the CW ownership correction changed this completion method, the two-app FT8 A/B was repeated: both placements reached the modulator completion marker and decoded `CQ 9H1SR JM75`, with 128 active 100 ms capture windows and peak 0.402. The first detected audio was at approximately 0.395/0.379 s into the slot. CAT remained disabled; this does not establish RF or physical PTT behavior.

Natural FT2-Link modulator A/B: `bridge_modulator_ft2_audio_ab.py` transmits a synthetic four-second waveform on BlackHole 2ch without HALT, confirms `finishModulatorIdlePlayback` is reached in both placements and observes TX disabled after completion. Independent capture recorded peak 0.22498 and zero peak from 0.8 to 2 seconds after completion for both. CAT was disabled; this is virtual audio and does not verify RF or PTT hardware.

CW variant of the same app probe: both before and after placements reached the modulator-finish marker and produced captured CW audio, but at the two-second post-completion observation both reported `transmitting=true` while the captured tail was silent. The test therefore failed in both placements. CAT was disabled and the process exited; no physical PTT or RF was exercised. The next entry documents the identified CW ownership cause and the correction.

CW natural-completion correction: `finishModulatorIdlePlayback` left `m_cwTxActive` set, and legacy state synchronization treats that flag as an active bridge-owned transmission. The method now clears the flag and pending CW text before publishing TX completion. In the virtual-audio A/B, the pre-fix placement showed silent audio but TX active after completion; the corrected production method ended TX and had a silent captured tail. FT2-Link natural-completion A/B passed with both placements, and the TX dispatch, FT8 gate and MAM policy CTests passed. Both macOS application targets built, and the full refactor A/B suite passed with this intentional source difference accounted for. Compact results: `bridge_refactor_evidence/cw-natural-modulator-fix-ab.txt`. CAT was disabled; physical PTT and RF remain untested.

The `--two-cw` follow-up runs two different CW requests in the same app session. The pre-fix state prevented the test from progressing after the first request, while the corrected version observed both TX start/end cycles and a silent final tail. The capture peak is measured for the combined session, so it does not independently quantify the second message's waveform; CAT remained disabled.

The two-request test now separately measures the second request's BlackHole capture window: the corrected version observed a 0.89996 peak during that window and a zero final tail, with both TX cycles complete. This verifies audio presence in the second window, not decoding or exact CW timing.

The second request is `EE`; an additional 10 ms RMS-envelope check found exactly two tone bursts separated by at least 20 ms in the corrected app. This coarse waveform check does not establish exact Morse timing or on-air reception.

Pending-PCM guard follow-up: `bridge_modulator_cw_pending_ab.py` injects CW ownership and an unread buffer into isolated apps. Both versions preserve CW state while completion is deferred; once the buffer is consumed, the original leaves CW ownership set and the corrected method clears it. Both clear TX and buffer, with CAT disabled. This uses a manually advanced in-memory buffer, not a physical audio device.

After updating their insertion wrappers for the CW correction, the existing modulator idle, pending-PCM and legacy PTT-request A/B labs all passed in both placements. Their individual simulation and hardware limitations remain as described above.

Legacy bridge audio two-app A/B: `bridge_legacy_bridge_audio_loopback_ab.py` explicitly initialises the legacy backend, simulates PTT confirmation with CAT disabled, and sends FT8 over BlackHole to a second Decodium instance. Before and after extraction the start method succeeded and the receiver decoded CQ 9H1SR JM75. Active audio windows were 129 and 128 respectively. PTT confirmation was synthetic; no radio or physical PTT was exercised. An earlier attempt failed because the isolated backend had not been initialised; its temporary files were removed.

Legacy bridge audio active-stop A/B: `bridge_legacy_bridge_audio_stop_ab.py` provides a simulated active TX with an in-memory PCM buffer and active hold flags. Both code placements clear TX and legacy-audio flags, release the buffer and clear hold/deadline state, with CAT and monitor disabled. This does not test physical PTT.

The test now also calls stop a second time and confirms that the cleared state remains unchanged. After restoring moved helper declarations to the baseline test placement, both cases passed; `bridge_refactor_evidence/legacy-audio-stop-idempotent-ab.txt`.

Legacy preflight loopback investigation: pre-extraction FT8 decoded successfully but the lab assertion detected m_catConnected=true after legacy synchronisation, including with embedded rig control explicitly disabled; diagnostics reported radio name None. Therefore no after-extraction RF/audio comparison was attempted and real CAT isolation remains to be understood. Two generated test profiles and temporary artifacts were removed; limited diagnostics are retained in bridge_refactor_evidence/legacy_preflight.

Legacy preflight A/B follow-up: `bridge_legacy_preflight_loopback_ab.py` disables embedded rig control after backend initialisation and explicitly clears the bridge CAT-state flag in the isolated lab before running preflight. Both original and extracted implementations passed preflight, started virtual FT8 and produced a CQ decoded by a second instance over BlackHole (128/129 active 100ms windows). PTT confirmation and CAT state were set in the lab; no physical PTT proof. Earlier m_catConnected=true is explained by legacy initialisation reapplying rig-control configuration; the meaning of the None rig online state outside this isolated setup remains unverified.

Legacy CAT state investigation: `TransceiverFactory` maps None to the Hamlib Dummy backend, and legacyCatConnected reports Configuration::is_transceiver_online rather than proof of a physical radio. Dummy may still support serial PTT, so None alone is not a safety isolation. The preflight two-app A/B was repeated with the test-only legacy init rig-enable policy forced false and explicitly checked as disabled; both placements decoded CQ 9H1SR JM75 via BlackHole, with synthetic PTT confirmation. Test hooks were restored afterwards.

Complete TX playback extraction: `bridge_complete_tx_runtime_ab.py` compares normal and error completion with simulated TX state in isolated instances; both placements end TX, with CAT disabled. MAM source regression test was updated to include the extracted QSO logger and passes. Actual audio and remaining completion branches need additional proof.

Complete TX playback audio investigation: the standard macOS FT8 two-app loopback decoded CQ successfully but did not invoke completeTxPlayback; it used the separate mac-pcm/modulator completion path. The A/B harness correctly rejected the case as coverage for completeTxPlayback, and the second variant was not run. Compact negative evidence is retained in bridge_refactor_evidence/complete_tx.

Complete TX PCM A/B: `bridge_complete_tx_pcm_ab.py` simulates bridge-owned legacy playback with 4096 bytes in a QBuffer. Both placements defer completion while PCM remains and release the buffer and TX state once drained; CAT is disabled. An initial fixture without a sink and without the legacy-ownership flag did not satisfy the buffer-release expectation, so it was corrected to model the intended legacy path. No physical audio/PTT was used.

Real virtual sink error follow-up: `bridge_complete_tx_sink_ab.py` starts a QAudioSink on BlackHole 2ch and invokes the actual completion method with an injected error and synthetic TX/legacy ownership. Both source placements cleared the sink pointer, buffer and TX state and restored the persisted test setting after restart. CAT was disabled. This checks cleanup of a started virtual sink, not natural playback completion, audio content, physical PTT or RF.

With `--drained`, the same test waits for BlackHole's QAudioSink to consume the synthetic PCM buffer before explicitly invoking non-error completion. Both placements cleared the sink, buffer and synthetic TX state. The test invokes completion explicitly rather than proving the normal device callback fired; no physical PTT or RF is used.

Natural completion investigation: `bridge_complete_tx_audio_ab.py` attempted a real FT2-Link virtual-audio transmission without HALT and required an instrumentation marker in `completeTxPlayback`. The first (before-placement) run observed the transmission and subsequent silence but did not see the marker, so the test failed and did not run the after placement. This attempt does not establish which other completion path handled the TX and is not evidence for `completeTxPlayback`; the production sources and build were restored.

The natural FT2-Link probe was repeated with a second temporary marker in `finishModulatorIdlePlayback`. The marker appeared and `completeTxPlayback` did not, identifying the actual completion path for this specific legacy Mac run. This still does not cover automatic entry into `completeTxPlayback`; the temporary instrumentation was restored after the run.

Complete TX special-mode A/B: `bridge_complete_tx_special_ab.py` simulates FT2-Link, CW and station-telemetry completion in isolated app sessions. Both placements clear TX and corresponding mode state; FT2-Link sets its ACK guard and telemetry is checked on the error path. CAT and monitor were off; no real audio or RF was used.

Complete TX SSTV/legacy A/B: `bridge_complete_tx_sstv_legacy_ab.py` simulates SSTV ownership and verifies completion leaves TX state intact, then simulates legacy bridge completion and observes one PTT-off request after the TX flag has cleared. Both placements match, with CAT and monitor disabled; no physical PTT was exercised.

TX boundary lab after completion extraction: `bridge_txboundary_ab.py` passes slot-stop, timer, drain and deadline cases; `bridge_txboundary_runtime_lab.py` compares the existing boundary/drain helpers in actual isolated FT2 playback to BlackHole. Both variants prepared CQ 9H1SR JM75, observed the PCM drain check and stopped TX with CAT false; the slot-boundary hook was not observed in this run.

TX output selection extraction: `bridge_tx_output_device_loopback_ab.py` confirms both method placements reached output selection and produced a CQ 9H1SR JM75 decoded by a second isolated Decodium session over BlackHole. CAT disabled; no RF.

TX output fallback A/B: `bridge_tx_output_fallback_ab.py` exercises updateSoundOutputDevice in isolated app sessions with BlackHole 2ch and a deliberately missing device. Both placements resolved BlackHole correctly, marked the missing device as not found and returned a non-null fallback; no playback or CAT occurred in this specific check.

Stop TX extraction: `bridge_stop_tx_runtime_ab.py` simulates active FT2-Link TX in isolated CAT-disabled instances; both placements clear TX and FT2-Link state. No real audio, PTT, SSTV, satellite or legacy delegate was exercised in this test.

Stop TX routing A/B: `bridge_stop_tx_routes_ab.py` simulates SSTV ownership and legacy digital TX in isolated app sessions, instrumenting and intercepting the respective SSTV cancel and legacy stop entry points. Both placements invoke each expected entry exactly once; embedded rig control is forced off. This checks routing, not completion of the intercepted operations or physical PTT.

On 2026-11-10 the test was updated for the moved SSTV cancellation method and helper declarations, then passed again in both placements; `bridge_refactor_evidence/stop-tx-routes-repeat-ab.txt`. Its cancellation and stop handlers are still intercepted, and do not prove physical PTT release.

Stop TX virtual-audio A/B: `bridge_stop_tx_audio_ab.py` plays synthetic FT2-Link audio to BlackHole, waits while TX is active, invokes HALT, verifies stopTx was reached and TX disabled; capture peak was 0.225 in both placements. CAT remained disconnected. It verifies audio presence and stopped software state but does not measure the exact audio cessation time or physical PTT.

Stop TX audio follow-up: the FT2-Link BlackHole A/B additionally measures the captured audio from 0.8 to 2.0 seconds after the HALT timestamp; both placements had zero peak in this interval after a measured signal peak of 0.225. This bounds silence in that window, not the exact cessation instant; CAT remained disconnected.

Tune extraction: `bridge_tune_audio_ab.py` compares the original and extracted Tune methods with BlackHole audio and CAT disabled. Both placements started Tune, produced peak 0.900, invoked stopTune and had zero captured peak 0.8–2.0 seconds after stopping. This is virtual audio, not RF or physical PTT.

Tune legacy-owner A/B: `bridge_tune_legacy_audio_ab.py` forces embedded rig control off during isolated backend initialisation, selects legacy TX ownership, and compares Tune via BlackHole; both placements produce peak 0.900 and silence in the 0.8–2.0s window after stop, CAT false. `bridge_tunewatchdog_ab.py` also passes its existing disabled, bounds, active timeout and disarm cases. No real rig or PTT.

On 2026-11-10 the Tune lab's baseline placement was updated to include the moved helper declarations, then rerun. Both placements again produced a 0.900 peak and a silent post-stop window with CAT disabled; `bridge_refactor_evidence/tune-legacy-repeat-ab.txt`. The audio was routed only through BlackHole.

Tune guards A/B: `bridge_tune_guards_ab.py` simulates SSB mode and a TX already active; both original and extracted methods leave Tune inactive in both cases. CAT and monitor were off.

Tune RTL-SDR and toggle A/B: `bridge_tune_rtl_toggle_ab.py` enables the isolated RTL-SDR receive-only setting and verifies Tune is blocked, then simulates an active bridge Tune and verifies a second start request stops it. Both placements match with CAT and monitor off; no RTL hardware or RF was used.

Start TX extraction: `bridge_start_tx_loopback_ab.py` compares the saved pre-extraction implementation with the new translation unit using two isolated Decodium instances and BlackHole. Both placements reached startTx, generated FT8 audio with peak 0.402 and 127 active 100 ms windows, and decoded `CQ 9H1SR JM75`; CAT was disabled. This proves virtual FT8 audio, not physical PTT or all other startTx branches. The full source-equivalence suite and `test_tx_dispatch_guard` / `test_ft8_auto_tx_decode_gate` pass. `bridge_periodic_tx_dispatch_ab.py` also passes both variants with CAT off after its instrumentation was moved to the extracted file.

Start TX guards A/B: `bridge_start_tx_guards_ab.py` exercises SSB, unarmed CW/CW-R, JTTY, unarmed FT2-Link, active SSB, remote CW keying, receive-only RTL-SDR, already transmitting and already tuning, each with native and legacy backend selection. All 20 checks matched before and after extraction in isolated CAT-disabled processes. These are simulated software states; no hardware or legacy transmission was exercised.

Decode model constructor extraction: `bridge_decode_model_init_ab.py` compares the inline constructor block and the extracted initializer in isolated application processes. Both construct the three named models, relay all five tested display-frequency notifications and clear the RX-key cache on DX-call and callsign notifications; CAT and TX remain off. The script also checks byte-for-byte equivalence of the moved block. This does not exercise every model update, setting or legacy decode path.

Decode model legacy RX A/B: `bridge_decode_model_init_legacy_pair_ab.py` tests both placements with two isolated Decodium sessions connected by BlackHole. The receiving instance used its actual legacy backend and showed the transmitted 9H1SR FT8 message in Band Activity in both variants. Embedded legacy rig control was forcibly disabled for the test, and both sessions used `--lab-no-cat`. This is virtual audio evidence, not RF or physical CAT/PTT verification.

RTL initialization A/B: `bridge_rtl_init_ab.py` compares original and extracted setup in isolated applications. Synthetic status, duplicate status, error, stopped-after-error and ordinary stopped signals yielded identical state and notification counts; the single-shot retune timer was present. CAT and TX were off. No RTL device or sample stream was exercised.

RTL initialization retune and start A/B: `bridge_rtl_init_retune_ab.py` verifies synthetic retune clears spectrum and IQ state, increments its serial, emits frequency notifications and applies a configuration adjustment. `bridge_rtl_init_started_ab.py` checks the synthetic started event updates source identity, health/watchdog state and four notifications. Both placements match with CAT and TX off; these are signal-level simulations, without RTL hardware or sample acquisition.

Audio device initialization A/B: `bridge_audio_device_init_ab.py` exercises the original and extracted connection setup inside isolated app processes. Synthetic input/output device notifications invalidate the cache and schedule a refresh; a simulated active monitor causes the timer to defer by 30 seconds. Both single-shot timers and outputs match, with CAT and TX off. Physical device hot-plug and audio playback were not exercised by this check.

Audio device initialization FT8 loopback A/B: `bridge_audio_device_init_loopback_ab.py` verifies initialization was reached in two isolated applications and that a CQ 9H1SR JM75 is transmitted and decoded through BlackHole before and after extraction. Both variants recorded peak 0.402 and 127 active 100 ms windows; CAT was disabled. This does not verify physical device hot-plug or RF.

Satellite initialization A/B: `bridge_satellite_init_ab.py` exercises station grid and nominal-frequency synchronization, feedback suppression, and Doppler blocking during TX, Tune and inactive monitoring. `bridge_satellite_init_apply_ab.py` additionally verifies application of a synthetic Doppler frequency with monitoring enabled. Both constructor placements match in isolated processes with CAT and TX off; no satellite, rig or physical frequency change was exercised.

Follow-up regression run: the complete `run_bridge_refactor_ab.py` suite passed after the decode model, RTL, audio device and satellite initializer extractions, including their pre-extraction source checks. CTest also passed Q65 Doppler, FT2-Link satellite half duplex, RTL DSP, RF spectrum, capabilities, tuning plan and SSTV TX audio device (7/7). These software tests do not establish physical hardware or cross-platform runtime behavior.

Map initialization A/B: `bridge_map_init_ab.py` compares the original constructor block and extracted initializer in isolated application processes. Both propagate an offline and online transition to map, propagation, satellite, callsign and DX cluster services and emit two bridge notifications, with CAT and TX off and external networking sandboxed. The moon-layer callback and cancellation of active network replies were not exercised by this check.

Map follow-up A/B: `bridge_map_init_abort_ab.py` creates an actual pending HTTP request to a deliberately non-responsive local server and checks the offline transition cancels it in both placements. `bridge_map_init_moon_ab.py` instruments the real moon refresh entry point and checks moon-on invokes it while moon-off and another layer do not. Networking remains restricted to localhost, CAT and TX are off. The moon test verifies callback routing, not astronomical accuracy or rendered output.

WAV initialization A/B: `bridge_wav_init_ab.py` compares original and extracted initialization in isolated application processes. Both start and stop the real WavManager, propagate both recording-state notifications and write a valid 50-byte WAV containing three synthetic samples; the uniquely named test file is read and removed. CAT and TX are off. Diagnostic timed recording and physical audio input were not exercised.

WAV diagnostic timer A/B: `bridge_wav_init_timed_ab.py` sets DECODIUM_RECORD_RX_SECONDS=1 and checks both original and extracted initialization automatically produce and close a valid empty WAV under Qt's test Documents directory; the test removes it. Monitor, CAT and TX are off. An initial attempt to override HOME on macOS still resolved the real Documents directory; the two empty files created in that attempt were identified and removed before repeating with the normal Qt test directory. This does not prove physical audio capture.

Ready-profile initialization A/B: `bridge_ready_profile_init_ab.py` exercises all 12 toggle signals in isolated applications. In both placements each manual signal clears the active profile, while the applying-profile guard preserves it for all 12 signals. CAT and TX remain off; the laboratory emits signals directly rather than operating each QML control.

Ready-profile functional A/B: `bridge_ready_profile_apply_ab.py` applies the actual weak, contest, cpu and balanced profiles, verifies the selected profile remains active, then changes the conservative setting with its real setter and verifies the active profile clears. All four profiles and changes match between constructor placements in isolated CAT-disabled processes; QML controls are not exercised.

DX Cluster notification initialization A/B: `bridge_dxcluster_init_ab.py` emits 14 synthetic known spot/network failures and one generic failure in isolated app processes. Both initializer placements route the 14 to status and the generic failure to the error signal; the single-shot timer is also checked. CAT and TX are off and the test does not connect to a real cluster or exercise live spot delivery.

DX Cluster local socket A/B: `bridge_dxcluster_init_local_ab.py` connects the real cluster client to a temporary localhost TCP server, sends a syntactically valid FT8 DX spot and verifies it appears in the spot list and triggers the bridge notification timer. Both initializer placements pass with CAT and TX off. This uses a synthetic local server, not an external cluster.

Time synchronization initialization A/B: `bridge_timesync_init_ab.py` compares original and extracted setup in isolated app sessions, using synthetic NTP signals. Both update the global offset, suppress duplicate notifications, honour recent-TX rearm hold-off, and propagate sync/unsync and disabled state identically. CAT and TX are off. Actual NTP servers and the timer rearm path were not exercised.

Time synchronization rearm A/B: `bridge_timesync_init_rearm_ab.py` exercises the real period timer arm method with synthetic NTP signals in isolated apps. Both placements leave the session unchanged for a sub-threshold offset, stop and rearm it for an offset change of at least 50 ms, suppress a rapid repeat and rearm on a fresh sync. Scheduled callbacks are invalidated before exit; CAT and TX remain off. This is not a live NTP or RF test.

PSK and Cloudlog initialization A/B: `bridge_psk_cloudlog_init_ab.py` emits synthetic PSK connection and Cloudlog QSO, API-key and error signals from the real service objects in isolated processes. Both placements produce identical bridge notifications and text with CAT and TX off. Queued ADIF completion and actual service requests were not exercised.

Cloudlog queued ADIF A/B: `bridge_psk_cloudlog_adif_ab.py` inserts isolated pending upload records, emits the real Cloudlog completion signal and verifies queued delivery to the actual bridge completion handler. Successful and failed outcomes produce matching IDs, details and final status in both placements. No network request or external QSO upload occurs.

Cloudlog local HTTP A/B: `bridge_psk_cloudlog_http_ab.py` uses the real Cloudlog client to POST a synthetic ADIF record with synthetic credentials to an ephemeral localhost server. Both placements send the expected request and process the HTTP success response through the queued bridge completion handler as Sent. The sandbox permits local networking only; no external service or real QSO is contacted.

DecoLog initialization A/B: `bridge_decolog_init_ab.py` delivers synthetic worked-before batches, logged/duplicate/error QSO events, a spot and award data through the real link signals. `bridge_decolog_tune_ab.py` checks invalid and valid tune requests update isolated bridge state correctly. Both placements match with CAT and TX off. The tests emit link signals directly and do not connect to DecoDXLog or control a physical rig.

DecoLog local protocol A/B: `bridge_decolog_local_ab.py` connects the actual DecoLink client to an ephemeral localhost TCP server, exchanges a protocol hello and receives worked-before, spot and award JSON messages. The bridge reflects all three in both placements; CAT and TX remain off. The peer is a synthetic server, not the DecoDXLog application.

QRZ and WSPR initialization A/B: `bridge_qrz_wspr_init_ab.py` checks real QRZ service signals for key, QSO and error notifications, verifies queued QRZ ADIF completion and the existence of the WSPR uploader. Original and extracted placements match in isolated apps with CAT and TX off. Signals are synthetic; no QRZ or WSPR network upload was made.

Remote web initialization A/B: `bridge_remote_web_init_ab.py` starts the actual local services with ephemeral ports and tests queued RX-frequency command routing; `bridge_remote_web_http_ab.py` additionally verifies a real localhost HTTP response. Both placements match with CAT and TX off. Both labs replace the secure credential lookup block temporarily with a synthetic token to avoid access to the user's Keychain; they do not verify credential resolution, authenticated WebSocket commands, or every remote command.

Remote authenticated WebSocket A/B: `bridge_remote_web_ws_auth_ab.py` uses a real local QWebSocket client and validates rejection before authentication, rejection of a wrong token, successful synthetic-token authentication and an acknowledged RX-frequency command applied to the bridge. Both placements match with CAT and TX off. The server is bound to all interfaces to activate authentication, while the process sandbox permits inbound and outbound traffic only on localhost; secure credential lookup is replaced temporarily with a synthetic token. No external peer or physical rig is used.

Remote WebSocket settings A/B: `bridge_remote_web_commands_ab.py` authenticates over localhost and applies four real commands: RX frequency, TX audio frequency, Quick QSO and FT2 message count. Both placements acknowledge all commands and update bridge state identically, with monitoring, CAT, TX enable and transmitting off. It uses the same synthetic-token substitution and localhost sandbox as the authentication lab; no RF transmission occurs.

Secure settings regression: `test_secure_settings` passes separately, including synthetic backend lookup, plaintext fallback and migration cases. It is not an A/B test of the remote initializer and does not access or validate the user's Keychain.

Remote credential routing A/B: `bridge_remote_web_credentials_ab.py` compares the original and moved token-resolution block with a synthetic secret backend and temporary QSettings. Primary, legacy-profile, default-profile, plaintext fallback and placeholder cases match, including the new helper wrapper. This tests routing with a simulated `load_or_import`; the separate secure-settings test covers its implementation with a fake backend. Neither test accesses macOS Keychain.

Callsign intelligence initialization A/B: `bridge_callsign_init_ab.py` uses the real service with synthetic enrichment signals in isolated offline app sessions. Both placements synchronize the operator callsign, reject enrichment for another call, fill empty grid/name/QTH and preserve manually entered fields. CAT and TX are off; external lookup, URL opening and confirmed-ADIF import are not exercised.

Callsign confirmed ADIF A/B: `bridge_callsign_confirmed_adif_ab.py` emits the real service's confirmed-download signal with a synthetic local ADIF file, waits for the actual asynchronous import into a temporary logbook and verifies the call and LoTW confirmation were added. Both initializer placements pass offline with CAT and TX off; external provider downloads are not exercised.

Callsign external URL A/B: `bridge_callsign_url_ab.py` emits the lookup-request signal with an empty URL and a disallowed FTP URL, verifying both placements route the requests to the real URL validator and report two errors. No browser or external site is opened; successful browser launching remains untested.

US state and WSPR notification initialization A/B: `bridge_usstate_wspr_init_ab.py` emits synthetic service status, updating, data and WSPR upload-status signals in isolated app sessions. Both placements produce the same two state-change notifications and status messages with CAT and TX off. No online data refresh or WSPR upload occurs.

US state DXCC refresh A/B: `bridge_usstate_dxcc_refresh_ab.py` instruments the real decode refresh entry and checks updatingChanged does not invoke it while dataChanged invokes it once in both placements. The instrumentation is restored afterward; this verifies routing, not external database retrieval.

macOS legacy startup A/B: `bridge_mac_legacy_start_ab.py` verifies that both placements construct the legacy backend after QML readiness, while `bridge_mac_standalone_start_ab.py` verifies DECODIUM_FORCE_STANDALONE_UDP prevents its construction. Both use isolated sessions with embedded legacy rig control forced off, CAT disconnected and TX off. This verifies backend availability and policy, not physical rig control or startup ordering under a deliberate QML stall.

macOS legacy delayed-QML A/B: `bridge_mac_legacy_start_stall_ab.py` repeats both startup placements with a deliberate four-second QML boot stall, offscreen rendering and embedded rig control forced off. Main.qml was created before legacy configuration in both runs, and both exited normally with CAT and monitoring disabled. The measured times are environment-specific; no physical rig was controlled.

CAT connections extraction A/B: `bridge_cat_connections_ab.py` emits synthetic backend error and status signals in isolated standalone sessions. Both placements ignore inactive backends, handle the active Hamlib backend, respect error suppression and shutdown, and accept Hamlib events when TCI is selected. CAT and TX remain off. Frequency, connection, PTT and band-routing callbacks require further coverage; no rig was controlled.

CAT frequency and band A/B: `bridge_cat_connections_frequency_ab.py` routes real manager signals to a temporary fake property source, exercising small frequency changes, inactive-backend rejection, band changes and a BandManager frequency request. Both placements update isolated bridge state identically with CAT and TX off. The property-source substitution is test-only and does not prove actual CAT communication or RF.

CAT PTT feedback and disconnect A/B: `bridge_cat_connections_ptt_ab.py` supplies a temporary fake CAT property source and instruments the real PTT feedback entry. Both placements route active/inactive reports, ignore another backend and shutdown, and process a synthetic disconnect. The actual feedback method runs with no PTT pending; CAT and TX remain off. No PTT confirmation, hardware connection or relay operation is tested.

CAT mode, split and connection A/B: `bridge_cat_connections_mode_ab.py` verifies USB and DATA-L mapping, inactive-backend filtering and split notification. `bridge_cat_connections_connect_ab.py` feeds a fake CAT4OM property source through the actual connect/disconnect callbacks and verifies bridge state and notifications while the real managers stay disconnected. Both placements match; no physical CAT or PTT is exercised.

CAT pending PTT confirmation A/B: `bridge_cat_connections_confirm_ab.py` supplies synthetic inactive, wrong-backend and active feedback with a pending software transition. Both placements confirm only the active matching backend and invoke the queued confirmation action exactly once. That action is a test counter, not audio or PTT; CAT and TX remain off and no physical relay is exercised.

CAT network connection A/B: `bridge_cat_connections_rigctl_ab.py` connects both placements to an isolated local Hamlib dummy rigctld, verifies connection, the 145 MHz frequency update, disconnection and settings readback, and checks that PTT and transmission remain inactive. Network access is restricted to localhost. This demonstrates simulated CAT communication, not physical radio operation.

Core timer initialization A/B: `bridge_core_timers_init_ab.py` compares the original and extracted initialization in isolated application sessions, checking the period, UTC, spectrum, process usage, stall, async decode and release timers, plus a real UTC timeout and settings readback. CAT, monitoring and transmission are disabled. It does not demonstrate RF timing or exercise every timer callback.

Timer callback follow-up A/B: `bridge_period_timer_runtime_ab.py` directly exercises FT8 slot advancement, reversal and the SSTV blocking gate. `bridge_ft2_async_timer_sequential_ab.py` directly dispatches four FT2 asynchronous decode passes from synthetic audio; both implementations produce four expected decodes and identical frequency ranges. The tests keep CAT and TX disabled; the FT2 test enables software monitoring for synthetic samples. These direct callback tests do not independently verify timer-to-callback wiring.

CAT telemetry initialization A/B: `bridge_cat_telemetry_ab.py` sends manager signals using synthetic connection and meter properties. Both placements agree for Hamlib and CAT4OM power, SWR and ALC, ignore the inactive backend, clear disconnected telemetry and preserve the TCI modulator guard while TX is off. CAT and TX remain disabled. This does not verify TCI PCM delivery or active-transmission completion and does not test physical hardware.

TCI signal follow-up A/B: `bridge_tci_signals_ab.py` emits queued PCM and modulator signals, checking real sample delivery to the asynchronous buffer, source filtering, the active-modulator guard and the state reset when modulation stops. A test-only source predicate and a completion counter replace TCI selection and final TX completion, respectively; CAT and physical transmission remain disabled. This verifies the signal wiring and pre-completion logic, not actual audio transport or PTT release.

TCI completion follow-up A/B: `bridge_tci_complete_ab.py` exercises the same queued signal through the real `completeTxPlayback` routine, using the CW cleanup branch to check that transmitting and pending state clear. The test replaces the TCI source predicate and suppresses physical CAT PTT control; it does not verify TCI network transport, the TCI stop-stream path or a radio relay.

Diagnostic connections A/B: `bridge_diagnostic_connections_ab.py` emits synthetic error, warning, rig error and status signals in isolated applications, then reads the actual diagnostic and QSO journal files. Both placements record the expected messages and preserve settings; CAT and TX remain disabled.

Decode and cluster connections A/B: `bridge_decode_cluster_connections_ab.py` emits synthetic decode-list and cluster signals, verifies both forwarding calls, simulates the legacy guard, and observes connection and delayed spot notifications. Both placements agree in isolated applications with CAT and TX disabled; the legacy selection is simulated and no external cluster or reporting server is contacted.

CAT include cleanup: the telemetry and CAT connections modules were rebuilt on macOS with reduced direct includes. The CAT telemetry, TCI completion, dummy rigctld connection and frequency-routing A/B tests were repeated successfully after cleanup. The comparison checks behavior on this macOS build, not compilation on other platforms.

SSTV existing QSO choices A/B: `bridge_sstv_qso_choices_ab.py` compares both placements in isolated apps using a temporary logbook and synthetic ready-cache records. It verifies search, limits, invalid records, issued identifiers and invalidation after the cache generation changes. CAT and TX remain disabled; asynchronous cache population and actual QSO association are not exercised by this test.

SSTV QSO cache follow-up A/B: `bridge_sstv_qso_choices_async_ab.py` starts from a cold cache, loads two real synthetic ADIF records asynchronously, verifies search and issued identifiers, and rejects an identifier after the logbook file changes. Both placements agree with CAT and TX disabled. The separate `test_sstv_qso_log`, `test_sstv_storage` and `test_sstv_qso_qml` tests also pass; these cover components separately and are not an end-to-end A/B image-association proof.

SSTV existing-QSO association A/B: `bridge_sstv_qso_association_ab.py` loads a synthetic ADIF QSO asynchronously, stores a generated image in an isolated SSTV gallery, requests association through the actual bridge API, waits for completion and reads the image record back from the storage worker. Both placements return the expected QSO identifier with CAT and TX disabled. The test covers an existing QSO, not creation of a new QSO or real radio reception.

SSTV RX reset A/B: `bridge_sstv_rx_reset_ab.py` checks inactive guards, AFC and slant reset serials, and reset/abort of an active synthetic Replay runtime in isolated apps. Both placements agree with CAT and TX disabled; it does not test a live radio signal or an active WAV replay controller.

SSTV RX controls A/B: `bridge_sstv_rx_controls_ab.py` applies and persists manual AFC and slant values, rejects an out-of-range AFC value without changing the previous state, and checks the active-redecode guard in isolated apps. Both placements agree with CAT and TX disabled. The RX reset A/B test was also repeated after the methods were grouped.

SSTV WAV replay A/B: `bridge_sstv_wav_replay_ab.py` rejects a non-local URL, replays a temporary one-second silent PCM WAV through the actual replay controller and runtime, and checks completion and session cleanup in both placements. CAT and TX remain disabled; live-source restoration, cancellation during playback and decoded-image quality are not covered by this test.

SSTV WAV cancellation and route restoration A/B: `bridge_sstv_wav_replay_cancel_ab.py` starts a synthetic live runtime, starts and immediately cancels a temporary WAV replay, and verifies cancellation, session cleanup and restoration of the prior runtime route in both placements. The producer-tap rebind is replaced with a test marker, so physical audio capture is not exercised; CAT and TX remain disabled.

Repeated on 2026-11-09 after the subsequent Bridge extractions: both placements again passed with matching results, recorded in `bridge_refactor_evidence/sstv-wav-replay-cancel-repeat-ab.txt`. The local link emitted deployment-target warnings for installed libraries built for newer macOS versions; this run is not a deployment compatibility test.

The same A/B lab now also tries a non-local URL while the synthetic live route is active and verifies rejection, restoration of that route and clearing of replay state before the cancellation scenario. Both placements passed with matching results: `bridge_refactor_evidence/sstv-wav-failure-restore-ab.txt`. The producer-tap rebind remains a simulated marker, not an audio-device check.

A separate `bridge_sstv_wav_replay_real_rebind_ab.py` test uses the actual local audio-tap reconnection after cancellation and injects 1,024 synthetic PCM samples into the audio sink. Both placements restored the route and accepted all samples, with CAT and TX disabled; `bridge_refactor_evidence/sstv-wav-real-rebind-ab.txt`. This tests the in-process tap, not a physical input device or legacy audio tap.

Real-rebind follow-up: `bridge_sstv_wav_replay_real_rebind_ab.py` creates a
synthetic local audio sink without opening a physical input and compares the
actual reconnection in both placements. Both restored the live route and an
active local producer connection after cancellation; compact results are in
`bridge_refactor_evidence/sstv-wav-rebind-passed.txt`. The earlier failed
investigation accidentally retained the test substitution of the rebind call;
its results do not indicate an application defect. This check establishes the
connection and then injects 1,024 synthetic PCM samples via the real local sink:
the SSTV ingress accepted all 1,024 in both placements. The PCM evidence is in
`bridge_refactor_evidence/sstv-wav-rebind-pcm-passed.txt`. This does not test
physical capture or decoded-image quality. CAT and TX were off.

SSTV raw audio export A/B: `bridge_sstv_raw_audio_ab.py` checks empty-buffer guards, replays a temporary WAV, exports the retained recent audio through the real background job and validates/removes the resulting RIFF file in the isolated test profile. Both placements agree with CAT and TX disabled. Image-specific retained-audio association and cancellation of a running export are not exercised.

The separate `bridge_sstv_raw_cancel_ab.py` application test covers those two additional paths: it feeds a synthetic partial Martin M1 acquisition, checks that the WAV export is linked to its image acquisition and persists that association, then starts and cancels a recent-audio export. Both code placements passed with matching results on 2026-11-09; see `bridge_refactor_evidence/sstv-raw-image-cancel-ab.txt`. This does not exercise a live radio input.
Rerun on 2026-11-09 passed unchanged in both placements; compact output is in
`bridge_refactor_evidence/sstv-raw-audio-repeat-ab.txt`.

SSTV image audio and cancellation A/B: `bridge_sstv_raw_image_ab.py` feeds a generated Martin M1 signal into the real RX runtime, verifies that the retained acquisition matches the current image, and checks that an exported RIFF file is linked to that image. `bridge_sstv_raw_cancel_ab.py` additionally starts a recent-audio export and immediately cancels it, observing the cancellation result. Both placements agree with CAT and TX disabled. This uses synthetic audio, not an on-air reception.

SSTV re-decode A/B: `bridge_sstv_redecode_ab.py` checks the empty-buffer and invalid-parameter guards, plays a synthetic WAV, starts a real re-decode job with temporary AFC settings, waits for replay completion and verifies the prior controls were restored. Both placements agree with CAT and TX disabled. It does not assess decoded-image quality or an on-air signal.

SSTV RX diagnostics A/B: `bridge_sstv_rx_diagnostics_ab.py` reads diagnostics in the idle state, starts a synthetic Replay source, feeds PCM and checks the reported stream and retained samples. Both placements agree with CAT and TX disabled; this samples the diagnostics map rather than exhaustively validating every metric or the HAMDRM backend.

HAMDRM diagnostics follow-up A/B: `bridge_sstv_hamdrm_diagnostics_ab.py` submits a deliberately unsupported PCM rate to the initialized, inactive native RX backend and checks that diagnostics report one additional rejected chunk and an inactive backend. Both placements agree with CAT and TX disabled. Active RX, successful frames and the remaining HAMDRM counters are not covered by this test.

HAMDRM active diagnostics A/B: `bridge_sstv_hamdrm_active_diagnostics_ab.py` temporarily substitutes a native RX adapter with a no-op audio tap, starts it using a native HAMDRM profile, submits 1,000 silent PCM samples and checks active/requested state, stream ID and accepted chunk/sample counters. Both placements agree; no radio or physical capture is opened. Successful decoded frames and external interoperability are not demonstrated.

SSTV diagnostics snapshot A/B: `bridge_sstv_diagnostics_snapshot_ab.py` checks exposed capabilities and TX inactivity, then feeds synthetic PCM into a Replay runtime and verifies the exposed source and converted-sample metrics. Both placements agree with CAT and TX disabled. Other snapshot sections and physical radio behavior are not exhaustively exercised.

SSTV status A/B: `bridge_sstv_status_ab.py` checks the exposed controller objects, inactive state and empty image, then starts a synthetic Replay runtime and verifies requested/active state, source and shutdown in both placements. CAT and TX remain disabled; image URL generation with a decoded image and actual audio-device labels are not covered.

SSTV status getter follow-up: `bridge_sstv_status_getters_ab.py` extends the isolated application comparison with the actual `sstvDetectedMode` and `sstvDigital` getters. With SSTV and HAMDRM enabled, both placements report no detected mode before reception and expose the digital controller; both start and stop Replay successfully. Results are in `sstv-status-getters-ab.txt`. This does not test a positive VIS detection or an on-air HAMDRM reception.

Positive VIS getter follow-up: `bridge_sstv_detected_mode_ab.py` feeds a real synthesized Martin M1 transmission prefix into the Replay receiver in isolated apps. Both source placements expose `martin-m1` through `sstvDetectedMode`, create the expected image and clear it on stream reset. CAT and TX are disabled. Results are in `sstv-detected-mode-ab.txt`. This verifies a synthetic audio path, not reception over RF.

SSTV image status A/B: `bridge_sstv_image_status_ab.py` feeds a generated Martin M1 signal into the RX runtime and checks the resulting 320x256 image snapshot, the URL's generation/revision, stable repeated reads and removal after reset. Both placements agree with CAT and TX disabled. It does not test live audio-device labels or on-air reception.

SSTV RX image save A/B: `bridge_sstv_image_save_ab.py` feeds a generated Martin M1 signal, saves the resulting partial image through the actual storage worker in the isolated test profile, verifies the saved file and status, and checks duplicate suppression. Both placements agree with CAT and TX disabled. Automatic terminal-image saving and attached raw-audio metadata are not exercised.

SSTV automatic save and raw-audio attachment A/B: `bridge_sstv_auto_image_save_ab.py` creates a partial Martin M1 reception, exports its acquisition audio, checks that an in-progress image is not automatically saved, stops the runtime and verifies the resulting automatic gallery record references the exported audio. Both placements agree with CAT and TX disabled; the reception is synthetic.
On 2026-11-09 the script was repaired to include helper declarations relocated
by subsequent extractions, then rerun successfully in both placements; see
`bridge_refactor_evidence/sstv-auto-image-repeat-ab.txt`. All 801 test script
names referenced by this document and all literal fixture paths found in test
scripts existed at that time. Existence and Python syntax alone do not prove a
script still compiles or runs, as this repair illustrates.

SSTV reset publication regression: `bridge_sstv_reset_probe_ab.py` repeats the image-save flow and requires the old image URL and snapshot to remain cleared immediately and after 1.5 seconds following reset. Before the runtime generation guard, both placements exposed the old image immediately; with the guard both checks passed. The full `test_sstv_rx_runtime` integration test also passed. This is synthetic Replay audio, not an on-air test.
On 2026-11-09 both the manual image-save and reset-probe scripts were updated
for relocated helpers and passed again in both placements. Compact results:
`bridge_refactor_evidence/sstv-image-reset-repeat-ab.txt`.
The SSTV runtime-status and image-status A/B scripts needed the same helper
declarations; after repair both passed again with synthetic Replay input and
CAT/TX off. See `bridge_refactor_evidence/sstv-status-repeat-ab.txt`.
The related Studio-status A/B script was rerun unchanged and passed in both
placements; see `bridge_refactor_evidence/sstv-studio-status-repeat-ab.txt`.

SSTV Studio archive A/B: `bridge_sstv_studio_archive_ab.py` generates and prepares a calibration image, observes its draft saved once in an isolated gallery, then invokes transmitted-image archival with a synthetic accepted session and verifies the stored file. Both placements agree with CAT and TX disabled; the transmitted category is simulated and no physical transmission occurs.
Rerun on 2026-11-09 passed both placements; compact output is in
`bridge_refactor_evidence/sstv-studio-archive-ab.txt`.

SSTV Studio status A/B: `bridge_sstv_studio_status_ab.py` binds and releases a thumbnail provider, prepares a synthetic calibration image, reads the source and prepared snapshots, and checks the elapsed TX clock in both placements. CAT and TX remain disabled; no on-air transmission is performed.

HAMDRM gallery archive A/B: `bridge_hamdrm_gallery_archive_ab.py` archives synthetic received-hybrid and transmitted-category images through the real isolated storage worker and verifies categories, digital/source metadata, audio frequency and saved files. Both placements agree with CAT and TX disabled. These are simulated gallery events, not decoded or transmitted HAMDRM signals.

HAMDRM deferred gallery A/B: `bridge_hamdrm_deferred_gallery_ab.py` delays isolated storage initialization, queues eight synthetic received images, verifies the ninth is rejected with a full-queue notice, and checks that all eight are saved once storage becomes ready. Both placements agree with CAT and TX disabled; no HAMDRM signal is received on air.

Linux audio helpers A/B: `bridge_linux_audio_helpers_ab.py` compiles the Linux-only helper block on macOS with a temporary `pactl` stub and compares source/sink parsing, monitor names, matching, default-setting calls and invalid input. Both placements agree. This is a host-side functional simulation, not a Linux build or a real PulseAudio/PipeWire configuration test.

Linux audio error follow-up A/B: the same temporary `pactl` harness also checks malformed lines, empty source/sink names, empty device lists, explicit command failures and a command timeout. Both placements produce the same results; no actual system audio setting is changed.

Audio device identity A/B: `bridge_audio_device_identity_ab.py` compares the original and extracted helpers using controlled empty and nonempty IDs, hexadecimal case and whitespace, mismatches and audio format reporting. `bridge_audio_device_identity_runtime_ab.py` also builds both placements and selects two opposite pairs of real BlackHole virtual devices in isolated app sessions, checking persisted IDs and disabled CAT/monitor/TX. Both placements agree. This verifies virtual-device selection on macOS, not physical audio capture or playback.

SSTV QSO association extraction A/B: `bridge_sstv_qso_association_extraction_ab.py` exercises both placements in isolated app sessions. It stores a synthetic image, associates an existing ADIF QSO, reads the association from the real temporary storage worker, rejects invalid requests and checks an error completion signal. Both placements agree; CAT and TX remain disabled. It does not transmit or receive radio signals.

SSTV storage queue extraction A/B: `bridge_sstv_storage_queue_extraction_ab.py` compares both placements in isolated apps with the real storage worker. It imports a synthetic valid PNG, checks invalid handoff handling, absent worker and missing handoff, and verifies a malformed record is queued and rejected. Both placements agree with CAT and TX disabled. A valid record created through the image store is also inserted via `enqueueSstvStoredRecord` and read back from SQLite in both placements.

SSTV audio source extraction A/B: `bridge_sstv_audio_source_extraction_ab.py` compares isolated apps with both code placements. It exercises DecoPort precedence, the local capture request, simulated RTL-SDR selection during discovery, DecoPort priority over RTL-SDR, and stable stream IDs for all source kinds, with CAT and TX disabled. It does not establish a live TCI, RTL-SDR or DecoPort connection, and does not exercise the Linux monitor-source branch.

SSTV storage shutdown extraction A/B: `bridge_sstv_storage_shutdown_extraction_ab.py` stores and associates a synthetic image, closes the real worker, verifies pending QSO failure and repeated shutdown, and reopens SQLite to confirm the association persists. Both placements agree in isolated apps with CAT and TX disabled.

SSTV runtime shutdown extraction A/B: `bridge_sstv_runtime_shutdown_extraction_ab.py` starts a synthetic Replay stream, queues PCM, shuts down the runtime and storage, verifies pending request failure and repeated shutdown, and checks SQLite persistence. Both placements agree with CAT and TX disabled. Active real-radio capture, live sharing and active digital TX are not exercised.

SSTV workspace extraction A/B: `bridge_sstv_workspace_extraction_ab.py` checks entering, leaving and repeated calls in isolated apps with monitoring and CAT disabled. `bridge_sstv_workspace_mock_ab.py` additionally exercises monitor restoration, legacy decode cleanup and the transmit guard with simulated dependencies. Both placements agree; actual monitored audio or radio transmission is not tested.

SSTV RX stop extraction A/B: `bridge_sstv_rx_stop_extraction_ab.py` starts synthetic Replay RX, queues PCM, stops RX, starts it again and stops it through the monitor-stop path, with a repeated stop. Both placements agree in isolated apps with CAT and TX disabled. A native HAMDRM RX backend with synthetic audio hooks was also kept active through analog RX stop, then stopped by the monitor-stop path. Restoration of physical legacy monitoring was not exercised.

SSTV RX start extraction A/B: `bridge_sstv_rx_start_extraction_ab.py` selects BlackHole 2ch in an isolated profile and verifies start, monitor activation, repeated start, and stop against both placements. CAT and TX are disabled. The same test exercises an already active legacy monitor and confirms it resumes after SSTV stops, and rejects startup after runtime shutdown. No RF signal is received; rollback following a failed monitor or audio-capture start remains untested.

SSTV RX start rollback A/B: `bridge_sstv_rx_start_rollback_ab.py` temporarily instruments both code placements to force the SSTV worker start to fail after suspending an active legacy monitor. It verifies the request and ownership flags are cleared, the legacy monitor resumes and a subsequent start succeeds, using BlackHole and no CAT/TX. This is injected worker failure, not a physical audio-device failure; production source is restored and rebuilt by the test.

SSTV RX routing extraction A/B: `bridge_sstv_rx_source_route_extraction_ab.py` starts synthetic Replay RX, checks rejection of an invalid source, changes to a simulated TCI source, verifies stable repeated selection, resets the generation and queues PCM on the new route. Both placements agree with CAT and TX disabled. A digital-only native HAMDRM backend with synthetic audio hooks also exercises valid, invalid and reset source requests. A live TCI connection and an active WAV replay are not exercised.

SSTV TX lifecycle extraction A/B: `bridge_sstv_tx_lifecycle_extraction_ab.py` uses a prepared synthetic source and a coordinator with fake PTT/audio hooks, checking active-session cancellation, PTT release notification and shutdown in isolated applications. Both placements agree with real CAT and TX disabled. Playback progress and release of an actual bridge-owned transmission remain untested by this probe.

SSTV TX lifecycle virtual-audio A/B: `bridge_sstv_tx_lifecycle_loopback_ab.py` runs two isolated Decodium sessions through BlackHole 2ch in Robot 12 with CAT disabled and VOX laboratory mode. Both TX sessions reported Completed/success (14.256 s before, 14.298 s after). The before RX decoded a complete image and passed image comparison; after RX reached 99.979% coverage but did not report a complete image, so end-to-end RX equivalence is not established. An earlier before run also stopped at 99.958% coverage. This is virtual audio, not RF; investigate the incomplete RX separately.

SSTV TX lifecycle loopback follow-up: with 15 seconds additional observation, both before and after runs completed Robot 12 TX and passed the full image comparison (100% coverage). Earlier incomplete receptions occurred in both placements; the extra observation correlates with these successful runs but does not establish the root cause. The runner now fails if either end-to-end image check fails.

SSTV TX start extraction A/B: `bridge_sstv_tx_start_loopback_ab.py` runs two isolated sessions over BlackHole with CAT disabled. Both Robot 12 TX sessions reported Completed/success (14.252 s before, 14.275 s after). Both RX images stopped at 99.958% coverage despite 15 seconds additional observation; therefore the end-to-end image test failed for both placements. The physical RF path was not exercised. `bridge_sstv_tx_start_reject_ab.py` verifies that an unprepared image is rejected with an error and no TX ownership in both placements; other rejection branches remain untested.
Rerun on 2026-11-09 after fixing the lab's relocated helper declaration: both
transmissions completed, but RX remained incomplete (99.958% before and
99.938% after), so the end-to-end test still fails. Compact evidence:
`bridge_refactor_evidence/sstv-tx-start-repeat-incomplete-ab.txt`. This is
virtual audio and does not establish the cause or physical RF behavior.
A follow-up with simultaneous BlackHole capture enabled passed end to end in
both placements, with complete images and TX success. A separate captured run
reported no internal exact-zero runs and mean image error 2.63, but its last
column mean error was 67.31; the general image criteria passed and no last-column
threshold was specified. See `bridge_refactor_evidence/sstv-tx-start-captured-ab.txt`.
The earlier no-capture failures remain unexplained; the different capture setup
does not prove their cause. The runner now removes only profiles identified by
its own RX/TX logs, along with temporary artifacts.

Robot C12 edge diagnostic: one further refactored two-app BlackHole run without
an additional capture reached complete RX coverage, but failed the existing
image-quality gate (mean absolute error 8.794, p95 approximately 65). The
terminal trace reported 120 lines and no incomplete rows. At the final image
column the mean errors by RGB channel were approximately 5.5, 194.3 and 1.1;
the penultimate column was approximately 0, 146.2 and 0. This narrows this
particular error to the green component near the right edge; it does not prove
its cause or explain the separate incomplete-frame observations. The temporary
run directory was removed after retaining compact measurements and frame trace
in `bridge_refactor_evidence/robot-c12-edge-trace.json`. CAT and RF were off.
The shared two-app lab now removes its own uniquely named Qt test profiles in
its `finally` block. A subsequent captured A/B run left the count of existing
AVT profiles unchanged (352 before and after), but end-to-end RX failed before
extraction at 99.917% and succeeded after extraction. This confirms the
incomplete-image behavior can also occur with capture enabled; capture alone is
not a reliable fix. See `bridge_refactor_evidence/sstv-profile-cleanup-ab.txt`.
The existing `test_sstv_robot` decoder round-trip checks representative edge
pixels including the final column across the Robot families with synthetic
frequency observations, and passed on 2026-11-09. `test_sstv_robot_rx_session`
also passed. These controlled observations do not exercise the complete PCM
demodulator/BlackHole path, so they do not explain the observed edge error.
The broader `test_sstv_rx_runtime` also passed in the same run (389.86 s);
the complete three-test summary is in
`bridge_refactor_evidence/robot-regression-ctest.txt`.
The targeted `robotColour12FullPcmCompletesAndMeasuresEdges` runtime test uses
a locally generated solid-color Robot 12 frame followed by 1,024 silent samples.
The image completes, but mean RGB error on its last column was 155.25 versus
0.33 in the middle column. The test currently measures rather than asserts an
edge-quality threshold; it passes despite the measured defect. Evidence:
`bridge_refactor_evidence/robot12-synthetic-pcm-edges.txt`. This reproduction
does not use Bridge, BlackHole, a radio or independent software and therefore
places the edge-quality issue somewhere in the synthetic codec/runtime path,
but does not identify its exact cause.
The same diagnostic now measures individual columns: in the synthetic
constant-color frame, column 154 had mean RGB error 0.59, columns 156/157 had
37.18, and columns 158/159 had 174.93/155.25. Row samples also vary, so this
is not merely a uniformly missing final column. Results:
`bridge_refactor_evidence/robot12-synthetic-pcm-edge-profile.txt`. This is a
measurement, not a threshold-based regression or an established root cause.

Robot 12 partial-image investigation: `SstvRobotRxSession::finish` flushes sync prediction to the image extent but still uses observed pixels to decide completeness. The existing bounded terminal-row recovery is restricted to Robot BW8; it does not silently fill missing Robot Colour 12 pixels. This explains why a near-complete colour frame remains partial, but does not identify why those pixels were missing.

On 2026-11-10 the two-app TX-start A/B was repeated with capture enabled and 15 seconds extra observation. Both transmissions completed in 14.274 s, but RX was incomplete in both placements (99.979% before, 99.917% after), and the end-to-end test failed. This repeats the existing intermittent issue; it does not identify a refactor regression or establish its cause. The script restored the production source and rebuilt the application afterward. CAT was disabled and BlackHole virtual audio was used.

Robot 12 edge diagnostic, same synthetic PCM on 2026-11-10: the focused runtime test completed with and without its test-only band-pass override. Mean RGB error at the last column was 155.25 with the normal filter and 95.61 with it disabled; the middle column was 0.33 in both. Disabling the filter reduced but did not eliminate this synthetic edge error. This experiment does not establish the cause of incomplete BlackHole images, and the override is not a production fix.

Robot 12 observation trace: temporary instrumentation of the PCM runtime test
recorded the first two lines' final luminance and chroma frequency observations
(`robot-c12-frequency-trace.txt`). In line 1, the five observations assigned
to the final blue-chroma sample fall from about 1693 to 1303 Hz; the preceding
sample also changes rapidly. This establishes that a transition is already
present in the input assigned to the edge pixel, before colour conversion. It
does not by itself identify whether sync anchoring, sample timing, filtering or
another stage causes the transition to be assigned there. The focused test
passed its existing completion assertions; temporary instrumentation was
removed and both the test and application targets were rebuilt.

Robot 12 sync-anchor experiment: in the same synthetic PCM case, the first
line's observed anchor was 10920; subsequent anchors were approximately seven
samples later than the nominal 1200-sample line spacing. Temporarily shifting
later anchors earlier by seven samples reduced last-column mean RGB error from
155.25 to 3.04, but increased first-column error from 9.50 to 129.06. A ten
sample shift similarly moved distortion to the leading edge. These exploratory
changes were reverted; they are not a production correction. See
`robot-c12-anchor-shift-experiment.txt` and `robot-c12-anchor-first-edge.txt`.
The synthetic solid-colour case alone does not establish a safe correction or
the cause of incomplete virtual-audio receptions.

Filter/anchor diagnostic A/B on that same PCM fixture: with the normal
band-pass, observed anchors for lines 1–5 were 7–8 samples late relative to the
encoder timeline; with the test-only band-pass disabled they were 2–4 samples
late. The measured last-column error was 155.25 and 95.61 respectively, while
the middle remained 0.33 in both (`robot-c12-anchor-filter-ab.json`). This
supports a contribution from the filter to the observed offset, but the
remaining offset and edge distortion show that disabling it is not a complete
fix. These are synthetic observations, not an RF or independent-codec test.

The encoder timeline narrows the boundary failure in that fixture further. At
12 kHz its 910 ms header ends at sample 10920 and each 100 ms line occupies
1200 samples, so line 2 starts its sync at sample 13320. The observations
assigned to the last blue-chroma sample of line 1 were centred at samples
13322–13326, already inside the next line's sync interval, and fell towards
the 1200 Hz sync frequency. This is evidence of edge misassignment for this
synthetic case, not proof that all received recordings have the same offset.

Radiofax TX extraction A/B: `bridge_sstv_radiofax_tx_reject_ab.py` checks invalid IOC, rate and conflicting phasing, plus missing image, without TX ownership. `bridge_sstv_radiofax_tx_loopback_ab.py` runs two isolated sessions on BlackHole, testing centered IOC 576 at 240 LPM; both placements completed TX in 81.619 s and received a complete image. CAT and real RF were disabled. Other IOC, rate and phasing combinations were not exercised by the loopback.

FAX480 TX extraction A/B: `bridge_sstv_fax480_tx_reject_ab.py` checks an invalid profile and missing image for all three profiles. `bridge_sstv_fax480_tx_loopback_ab.py` compares the Dayton 480 profile in two isolated sessions on BlackHole: both TX runs completed (140.009 s before, 140.006 s after), both RX images reached full coverage and passed image comparison. CAT and real RF were disabled; QSSTV and libsstv profile loopbacks were not repeated for this extraction.

SSTV calibration TX extraction A/B: `bridge_sstv_calibration_tx_extraction_ab.py` checks invalid reference rejection and runs the sync-1200 calibration job through the real audio-only coordinator and BlackHole 2ch in isolated apps. Both placements complete and release TX ownership; CAT and RF are disabled. A follow-up `bridge_sstv_calibration_tx_capture_ab.py` independently captures BlackHole audio in both placements: the strongest FFT bin is 1195.31 Hz at 48000/4096 Hz resolution, consistent with the intended 1200 Hz tone; peak-window RMS was 0.5674 in both runs.

HAMDRM TX bridge extraction A/B: `bridge_sstv_hamdrm_tx_loopback_ab.py` transfers an identical 225-byte object between two isolated Decodium sessions over BlackHole in both placements. `bridge_sstv_hamdrm_tx_cancel_ab.py` checks a valid audio-only job, cancellation, release, invalid source and invalid session IDs. CAT and RF were disabled; this is Decodium-to-Decodium, not evidence of independent EasyPal interoperability.

SSTV TX initialization extraction A/B: `bridge_sstv_tx_init_capture_ab.py` creates the real coordinator in both placements and runs a calibration tone to BlackHole; both complete and release ownership, with an independently captured 1195.31 Hz FFT bin at 11.72 Hz resolution. CAT and RF were disabled. Physical PTT and audio-device failure/underrun callbacks were not exercised.

Audio sink retirement extraction A/B: `bridge_audio_sink_retire_ab.py` starts a real QAudioSink on BlackHole 2ch in each placement and verifies null-sink buffer cleanup, active sink muting, buffer deletion and macOS deferred retention. Both agree. Existing audio-stop and legacy-stop A/B tests also pass. This does not exercise RF, Windows WASAPI or physical devices.

Audio device resolution extraction A/B: `bridge_audio_device_resolve_ab.py` compares ID, exact and normalized name matching, ambiguity, missing/default selections and isolated settings writes with synthetic endpoints. `bridge_audio_device_resolve_runtime_ab.py` checks both BlackHole input/output combinations and stored IDs in isolated apps. Both placements agree; CAT, monitoring and TX were disabled.

Both audio-device resolution tests were rerun on 2026-11-10 and passed. The isolated app runs selected 2ch input/16ch output and the inverse combination with identical device IDs in both placements; the production source and QML build were restored by the harness. These checks do not establish physical audio capture.

Visual TX coalescing extraction A/B: `bridge_visual_tx_coalesce_ab.py` compiles both placements with the actual UTC helpers and checks slot merging, local-mirror preference, FT2/FT4 boundaries, unsupported mode and midnight. Existing RX rebuild and decode UI snapshot A/B tests also pass. This is a functional model test, not a live RF decode.

Decode mirror keys extraction A/B: `bridge_decode_mirror_keys_ab.py` compares both placements using real UTC and message-normalization helpers, checking slot equivalence, changed messages, new-entry selection, timestamp keys and loose keys. Existing RX rebuild, legacy mirror and decode UI snapshot comparisons also pass. These are functional model tests, not live RF reception.

Settings helper extraction A/B: `bridge_settings_helpers_ab.py` exercises aliases, SMTP key normalization, profile priority and placeholders with a fake credential backend. `bridge_settings_helpers_runtime_ab.py` verifies generic settings and aliases in two isolated app profiles. Both placements agree; the real OS credential store is not accessed by these tests.

Application mode helpers extraction A/B: `bridge_application_modes_ab.py` compares canonical FT8/FT4/Q65 and FT2-Link names, streaming/PCM keyboard modes and radio-only labels. `bridge_application_modes_runtime_ab.py` also decodes two WAV fixtures across FT8-to-FT4-to-FT8 mode changes in isolated apps. Both placements agree with CAT and TX disabled; no RF was used.

Bridge destructor extraction A/B: `bridge_destructor_runtime_ab.py` closes and reopens isolated applications and verifies settings persistence. `bridge_destructor_active_decode_ab.py` also closes the app while an FT8 WAV decode is actually active. Both placements exit cleanly with CAT and TX disabled; hardware PTT teardown and timeout/forced-thread termination are not exercised.

Panadapter GPU policy extraction A/B: `bridge_panadapter_gpu_init_ab.py` compares environment opt-in, settings opt-in, disabled GPU and disabled force using isolated inputs. `bridge_panadapter_gpu_init_runtime_ab.py` launches real isolated apps in default and disabled modes and checks the selected startup policy. Both placements agree with CAT, monitoring and TX disabled; actual GPU rendering is not tested.

UI startup settings extraction A/B: `bridge_ui_startup_settings_ab.py` compares default values, style migration and FPS clamping with temporary QSettings stores. `bridge_ui_startup_settings_runtime_ab.py` compares actual UI properties in isolated applications. Both placements agree; CAT and TX are disabled.

Audio level math extraction A/B: `bridge_audio_level_math_ab.py` checks clamping, inverse RX mapping, TX gain and legacy attenuation; `bridge_audio_level_math_runtime_ab.py` checks actual app controls with modern and legacy backends. Both placements agree, with CAT and TX disabled.

### File path helpers

`python3 tests/tools/bridge_file_paths_ab.py` compares the original and extracted
implementations and exercises home expansion, existing and newly created parent
directories, a blocked directory, TX recording names and WSPR WAV paths. It uses
an automatically removed temporary directory. This checks the helpers and real
filesystem operations; it does not by itself prove an end-to-end TX recording or
WSPR decode.

`python3 tests/tools/bridge_file_paths_runtime_ab.py` also runs the original and
extracted helpers inside isolated Decodium processes. Both generate TX and WSPR
paths and create and write an actual temporary file with CAT and TX disabled.
This does not exercise an on-air transmission or an actual WSPR decode.

### TX waveform helpers

`python3 tests/tools/bridge_tx_wave_helpers_ab.py` compares the original and
extracted TX-mode classification, WSPR power and waveform metrics, including
empty and nonempty samples. `python3 tests/tools/bridge_tx_wave_helpers_runtime_ab.py`
runs both implementations in isolated Decodium sessions with CAT and TX disabled.
The checks do not exercise physical transmission or measure actual RF power.

### TX slot helpers

`python3 tests/tools/bridge_tx_slot_helpers_ab.py` compares the original and
extracted period and reply-slot rules for FT8, FT4, FT2 and fallback cases.
`python3 tests/tools/bridge_tx_slot_helpers_runtime_ab.py` exercises the helpers
in isolated Decodium sessions with CAT and TX disabled. These checks do not
measure timing of radio PTT or RF.

### Decode deduplication keys

`python3 tests/tools/bridge_decode_keys_ab.py` and
`python3 tests/tools/bridge_decode_keys_runtime_ab.py` compare the original and
extracted normal and FT2 asynchronous keys, including frequency canonicalisation,
jitter buckets and distinct displayed time slots. The app sessions disable CAT and
TX. An empty displayed time is currently normalised to the current UTC time before
the timestamp fallback is reached; this existing behaviour is unchanged.

### Asynchronous decode logs

`python3 tests/tools/bridge_async_logs_ab.py` compares original and extracted
thread pools and performs real ordered writes to separate and aliased temporary
files. `python3 tests/tools/bridge_async_logs_runtime_ab.py` verifies the same
file contents in two isolated Decodium processes with CAT and TX disabled. The
checks do not exercise sudden process termination during a pending write.

### Diagnostic helpers

`python3 tests/tools/bridge_diagnostics_ab.py` compares message line formatting
and warning classification, including Italian and English messages. The companion
`bridge_diagnostics_runtime_ab.py` checks both implementations in isolated app
sessions with CAT and TX disabled.

### Legacy presentation helpers

`python3 tests/tools/bridge_legacy_presentation_helpers_ab.py` compares legacy
palette and depth mappings, including boundaries and fallback. The companion
`bridge_legacy_presentation_helpers_runtime_ab.py` checks the mappings and UI
palette property in isolated Decodium processes with CAT and TX disabled; it does
not establish actual legacy waterfall rendering.

`python3 tests/tools/bridge_legacy_palette_backend_runtime_ab.py` additionally
checks that both original and extracted mappings apply Raptor and Linrad to the
real embedded legacy backend. It verifies backend state, not rendered pixels.

### Autolog helpers

`python3 tests/tools/bridge_autolog_helpers_ab.py` compares call inference,
duplicate and AutoCQ time-window boundaries, and cache pruning. The companion
`bridge_autolog_helpers_runtime_ab.py` checks call inference, window boundaries and cache pruning
inside isolated Decodium sessions with CAT and TX disabled. It does not perform a
real radio QSO.

### TX output device resolution

`python3 tests/tools/bridge_tx_output_resolve_runtime_ab.py` compares real
BlackHole device resolution and missing-device fallback in isolated application
sessions with CAT and TX disabled. `python3 tests/tools/bridge_tx_output_resolve_linux_ab.py`
compares the Linux-specific success and failure paths with mocked Pulse/PipeWire
operations; it does not change the system default sink or prove Linux integration.

`python3 tests/tools/bridge_tx_output_linux_syntax.py` additionally checks that
the extracted Linux branch parses against Qt and its declared interfaces using
the host compiler. This is a syntax check, not a Linux build, link or runtime test.
`python3 tests/tools/bridge_tx_output_linux_link_ab.py` compiles and links the
actual extracted Linux branch with synthetic Pulse operations, comparing its
success, failure and fallback results against the original on the host. It is
not a substitute for a Linux system integration test.

### World map callsign helpers

`python3 tests/tools/bridge_worldmap_call_helpers_ab.py` compares normal,
portable, empty and placeholder callsigns. The companion
`bridge_worldmap_call_helpers_runtime_ab.py` checks the resulting helper values
in isolated app sessions with CAT and TX disabled. It does not verify rendered
map markers or a live spot feed.

### CAT backend helpers

`python3 tests/tools/bridge_cat_backend_helpers_ab.py` compares split mode,
backend classification, network rigctl recognition and PTT signal provenance.
`python3 tests/tools/bridge_cat_backend_helpers_runtime_ab.py` checks the same
rules in isolated app processes using a synthetic manager. CAT and TX are disabled;
no real rig control or physical PTT is exercised.

### Visual TX time

`python3 tests/tools/bridge_visual_tx_time_ab.py` compares visual TX slot
selection using a deterministic clock at FT8, FT4 and FT2 boundaries. The
companion `bridge_visual_tx_time_runtime_ab.py` compares actual app output to
the corrected-clock slot in isolated sessions with CAT and TX disabled. It does not measure RF timing.

### QSO journal

`python3 tests/tools/bridge_qso_journal_ab.py` compares journal filtering and
actual writes in uniquely named temporary Qt test profiles, including newline
sanitisation. `python3 tests/tools/bridge_qso_journal_runtime_ab.py` verifies
selection and file contents in isolated Decodium sessions with CAT and TX disabled.
The A/B test also creates dated files around the 30-day retention boundary and checks disabled-journal behaviour.

### Audio channel helpers

`python3 tests/tools/bridge_audio_channel_helpers_ab.py` checks serialized channel
names and macOS mono/stereo channel selection. The companion
`bridge_audio_channel_helpers_runtime_ab.py` checks these helpers inside isolated
app sessions with CAT and TX disabled. It does not emit or capture audio.

### Legacy rig ownership policy

`python3 tests/tools/bridge_legacy_rig_policy_ab.py` compares fallback and
ownership decisions with a synthetic backend, including all availability/control
combinations. `python3 tests/tools/bridge_legacy_rig_policy_runtime_ab.py` checks
the same policy against the embedded legacy backend in isolated app sessions.
CAT and TX remain disabled; no physical rig is controlled.

### Startup CAT migration

`python3 tests/tools/bridge_cat_migration_startup_ab.py` compares migration of
native CAT settings to Hamlib in isolated INI files and verifies that a second
run preserves later user changes. `python3 tests/tools/bridge_cat_migration_startup_runtime_ab.py`
checks migration inside isolated app sessions using the active profile; CAT and
TX remain disabled. Neither test opens a physical serial port.

### Precomputed TX waveform storage

`python3 tests/tools/bridge_precomputed_wave_store_ab.py` compares copying,
zero-fill, mode-sensitive clearing and size limits with a bounded synthetic
buffer. `python3 tests/tools/bridge_precomputed_wave_store_runtime_ab.py`
checks copy and clearing against the real shared buffer in isolated app processes
with CAT and TX disabled. It allocates the existing shared waveform buffer in
those temporary processes but does not play or transmit audio.

`python3 tests/tools/bridge_audio_channel_loopback_ab.py` additionally checks the
channel helper is invoked during an FT8 transfer between two isolated Decodium
sessions over BlackHole, with CAT disabled. Both before and after decoded the
message and passed the slot-alignment check on the final run. One earlier before
run decoded successfully but reported first audio at 12.98 seconds modulo the
slot and failed the alignment check; its cause has not been established. The test
records timing failures separately from successful audio decoding.

### Environment and receive ownership policy

`python3 tests/tools/bridge_environment_rx_policy_ab.py` compares CAT suppression,
truthy environment values and JT/FST4 ownership in isolated processes. The
companion `bridge_environment_rx_policy_runtime_ab.py` checks both implementations
in isolated app sessions and restores the test process environment. CAT and TX
remain disabled; no physical radio is controlled.

### FT decode thread budget helpers

`python3 tests/tools/bridge_ft_thread_budget_helpers_ab.py` compares automatic
CPU reserve, thread limits, environment overrides and adaptive limits.
`python3 tests/tools/bridge_ft_thread_budget_helpers_runtime_ab.py` checks the
same policies in isolated app processes with CAT and TX disabled. The Windows
scheduler and physical CPU performance are not measured by these tests.

### Legacy decode filter connections

`python3 tests/tools/bridge_legacy_filter_connections_ab.py` checks the exact
signal-connection bodies and their original constructor order. The companion
`bridge_legacy_filter_connections_runtime_ab.py` sends all three filter change
signals in isolated app sessions with the embedded legacy backend, verifies
revision invalidation, and keeps CAT and TX disabled. It does not verify a live
radio decode or visual presentation of filter results.

### TX audio startup

`python3 tests/tools/bridge_tx_audio_startup_ab.py` checks identical constructor
connection bodies and order. `python3 tests/tools/bridge_tx_audio_startup_runtime_ab.py`
checks real object creation, error propagation and an injected modulator-idle
hold in isolated app sessions with CAT disabled. `python3 tests/tools/bridge_tx_audio_startup_loopback_ab.py`
compares a real FT8 transfer between two Decodium sessions over BlackHole: both
versions decoded successfully and passed the timing check in the final run. The
macOS PCM path did not emit the modulator-idle callback during that transfer;
the callback was exercised separately by the injected event. An earlier before
run decoded successfully but failed the timing check at 12.82 seconds modulo
the slot; its cause remains unknown. No physical PTT or RF was used.

### CAT dispatch and SSTV PTT routing

`python3 tests/tools/bridge_cat_dispatch_ab.py` compares Hamlib, OmniRig, CAT4OM,
VOX and DecoPort routing with synthetic managers, including an explicitly supplied
CAT4OM manager for the SSB permission check. `python3 tests/tools/bridge_cat_dispatch_runtime_ab.py`
compares the disconnected DecoPort and SSTV paths in isolated Decodium processes
with CAT and TX disabled. No real radio, remote link or physical PTT is exercised.

`python3 tests/tools/bridge_cat_dispatch_ssb_runtime_ab.py` also compares the
original and forwarded SSB permission check using a real BlackHole output and a
disconnected CAT4OM manager. It verifies the prerequisites and that SSB does not
start; no physical CAT or PTT is operated.

### FT scheduler

`python3 tests/tools/bridge_ft_scheduler_ab.py` compares OpenMP limits, Windows
priority decisions, default and preserved environment variables, and disabled
scheduling using synthetic OS calls. `python3 tests/tools/bridge_ft_scheduler_runtime_ab.py`
checks the actual OpenMP thread limit in isolated app sessions and restores it.
Windows behaviour is simulated on the Mac; it is not a Windows integration test.

### SSTV storage startup

`python3 tests/tools/bridge_sstv_storage_startup_ab.py` compares the original
startup body and checks worker readiness, autosave controls and rejection of an
invalid incoming handoff in isolated application sessions. The companion
`bridge_sstv_storage_startup_share_ab.py` performs signed end-to-end encrypted
transfers of two images between two Decodium processes and a local authenticated
HTTPS server for both versions. Storage, TX archive, incoming Gallery import
and Gallery model component tests also pass. CAT and TX are disabled; these tests
do not establish RF or third-party interoperability.
The signed two-app A/B test also accepts image A into the receiving Gallery,
checks that the imported file exists under the isolated test profile and compares
its decoded pixels with the original. This exercises the valid incoming-handoff
connection as well as the transfer.

### SSTV audio relay

`python3 tests/tools/bridge_sstv_audio_relay_ab.py` compares the original relay
with the extracted private header, with and without HAMDRM. It covers disabled
input, ordinary and empty packets, clock regression, digital forwarding and
draining a packet in flight. The `bridge_sstv_audio_relay_radiofax_loopback_ab.py`
and `bridge_sstv_audio_relay_hamdrm_loopback_ab.py` scripts exercise the actual
application before and after extraction using two isolated Decodium sessions and
BlackHole virtual audio. The Radiofax case checks a complete image and pixel
comparison; the HAMDRM case checks the received object byte for byte. These do
not establish physical RF or third-party interoperability.

### SSTV producer taps

`bridge_sstv_producer_taps_radiofax_loopback_ab.py` and
`bridge_sstv_producer_taps_hamdrm_loopback_ab.py` compare the original and
extracted tap methods in two actual Decodium sessions over BlackHole virtual
audio. Radiofax received a complete image with pixel comparison; HAMDRM
received an identical object. Other input devices and physical RF were not
exercised by these loopbacks. The extracted bodies match their saved fixture.
The HAMDRM loopback was rerun on 2026-11-09 and passed both placements with
225 identical received bytes; see
`bridge_refactor_evidence/hamdrm-taps-repeat-ab.txt`. This compares two local
Decodium sessions and does not establish third-party interoperability.
The corresponding Radiofax loopback was rerun on 2026-11-09; both placements
completed TX and received a complete image passing the end-to-end comparison.
Results: `bridge_refactor_evidence/radiofax-taps-repeat-ab.txt`. This is local
virtual audio, not independent-decoder or RF evidence.

### SSB compilation unit

`bridge_ssb_unit_runtime_ab.py` compares the included original with the new
compilation unit in isolated real application sessions. It checks CAT-unavailable
PTT refusal, invalid mode, bounded AGC settings and idle SSB calls. The existing
`bridge_audiosink_ssb_ab.py` confirms 240 local PCM samples reach SSB while
remote and non-SSB inputs are gated. Neither test keys a physical radio, and
active SSB transmission or remote hardware is not covered.

### Bridge constructor

`bridge_constructor_runtime_ab.py` builds the original and extracted constructor
and compares initial managers, timers, SSTV runtime, mode, CAT and TX state in
isolated app profiles. `bridge_constructor_legacy_runtime_ab.py` compares real
macOS legacy startup ordering under a deliberate QML stall; CAT and monitor are
disabled and the legacy rig-control hook is suppressed for safety. The
constructor body is otherwise preserved, with its existing logger forwarded.

### Diagnostic log routing

`bridge_log_routing_ab.py` compares all 64 message forwarding functions,
including diagnostic output, QSO journal routing and FT2-Link flush requests.
`bridge_log_routing_runtime_ab.py` starts isolated original and extracted app
variants, writes a unique QML diagnostic marker and reads it from the actual
diagnostic log. CAT and transmission remain disabled.

### SSTV helpers

`bridge_sstv_helpers_controls_ab.py` compares two advanced SSTV and Radiofax
control profiles in isolated actual app sessions. The Radiofax and HAMDRM
`bridge_sstv_helpers_*_loopback_ab.py` tests compare complete image reception
and byte-exact digital reception using two apps over BlackHole. They do not
prove physical RF or third-party interoperability.

### Bridge service accessors

`bridge_services_runtime_ab.py` checks the four service accessors and the
satellite-window signal in isolated original and extracted applications,
with CAT and transmission disabled.

### CAT PTT state accessors

`bridge_cat_ptt_state_runtime_ab.py` compares CAT4OM, OmniRig and Hamlib
manager state in isolated app sessions, including trimmed PTT method and its
default. The managers remain disconnected; no physical PTT is operated.

### Sharing service accessors

`bridge_sharing_accessors_runtime_ab.py` compares actual service accessors and
CAT backend persistence for direct, DecoPort with previous backend and DecoPort
without previous backend. The remote-link selection and service-accessor A/B
checks also pass with CAT and TX disabled.

### TX preflight

The start-TX A/B tests compare the original function with the extracted
`startTxPreflight()` using 20 safety cases and an FT8 virtual-audio loopback
between two isolated applications. The source comparison reconstructs the
original ordered guards from the helper. No physical CAT/PTT is operated.

`bridge_sstv_helpers_qso_share_ab.py` additionally repeats the QSO association
and signed encrypted two-app share tests with the SSTV helpers in both their
original and extracted placements. Both paths verify persisted association,
rejection/error reporting, two transferred images and Gallery import; CAT and
TX remain disabled.

### Bridge state accessors

`bridge_state_runtime_ab.py` compares state getters, UTC timer and CAT-settings
signal in isolated applications. The SSB audio-sink A/B also verifies the
updated test insertion point. CAT and TX remain disabled.

### TX format adapters

`bridge_tx_adapters_runtime_ab.py` compares format reporting, format equality,
PCM conversion on two channel selections and default-device handling in isolated
apps. `bridge_tx_adapters_loopback_ab.py` also compares an FT8 message sent
between two isolated Decodium sessions over BlackHole, with CAT disabled.

### TX message selection

The source comparison reconstructs the original selected-slot lambda and guard
block from `prepareStartTxSelection`. Start-TX guard and FT8 two-app virtual
audio A/B tests pass; the existing Hound preparation test checks its blocked
case. Physical RF and an active Hound QSO were not exercised.

### Shared runtime defaults

`bridge_shared_runtime_ab.py` compares the PSK debug flag, FT decoder thread
limit and UTC timezone in isolated app variants. The environment companion
checks eight true/false spellings in fresh processes; both versions agree.

### Decode adapters

`bridge_decode_adapters_runtime_ab.py` compares callsign extraction and
unresolved grid/hash classification in isolated original and extracted apps.
An FT8 virtual-audio loopback after extraction decoded the expected message;
its compact result is retained under `bridge_refactor_evidence/decode_adapters`.

### RX monitoring accessor

`bridge_rx_monitoring_loopback_ab.py` compares the original and extracted
monitoring accessor with two isolated FT8 applications over virtual audio.
`bridge_modeswitch_runtime_ab.py` also compares mode changes with CAT and TX
disabled. No physical radio or PTT is operated.

### UI and audio state accessors

`bridge_ui_audio_state_runtime_ab.py` compares the audio and service accessors
and applies Raptor and Linrad palettes to the real legacy backend in isolated
apps. The existing virtual-device selection A/B and general suite also pass;
CAT and TX remain disabled.

### TX policy adapters

`bridge_tx_policy_adapters_runtime_ab.py` compares the Hound constant, FTx mode
classification and 12 timing combinations in isolated apps. FT8 two-app
virtual-audio A/B and the shared runtime A/B also pass, with CAT disabled.

### Audio adapters

`bridge_audio_adapters_runtime_ab.py` compares virtual audio device identity,
format logging and the compatibility TX device initializer in isolated apps.
The virtual-device selection A/B and general suite also pass with CAT and TX
disabled.

### Bridge layout cleanup

`bridge_layout_runtime_ab.py` verifies that the layout edit only removes
repeated relocation comments, an empty namespace and excess blank lines, and
compares startup state in isolated apps. The general suite also passes.

### Decode colour method placement A/B

`bridge_decode_colors_placement_ab.py` now compares the methods in their original
and extracted locations in real isolated app sessions. Each location exercises
20 colour categories across two profiles, including persistence after restart.
The original helper implementation remains source-compared separately. The
probe checks properties and persistence, not pixel rendering or RF.

### FTx settings method placement A/B

`bridge_ftx_settings_placement_ab.py` now compares the original and extracted
FTx settings methods in actual isolated applications. Both versions exercise
27 QML properties across two profiles, including readback after restart. CAT,
monitoring and TX remain disabled; the test does not establish RF behavior.

### Timed PCM offset loopback A/B

`bridge_tx_pcm_offset_loopback_ab.py` compares the original and extracted PCM
offset method in two isolated FT8 applications with CAT disabled and actual
slot alignment. To reach the common QAudioSink path on macOS, the test
explicitly disables the Mac-specific PCM branch and enables PCM preparation;
these substitutions and the diagnostic marker are restored after each run.
Both variants reached the timed offset calculation and decoded the message.
This is a forced laboratory path on macOS, not a Windows/Linux runtime or RF
proof. Compact success and earlier failed-attempt evidence is retained under
`bridge_refactor_evidence/tx_pcm_offset`.

### Smooth decode queue runtime A/B

`bridge_decodeflow_queue_runtime_ab.py` exercises the real Bridge queue in
both method placements with three synthetic FT8 messages. It verifies the
first two-item drain, immediate flush of the remaining item when smooth flow is
disabled, and message order. CAT and TX remain disabled. This test does not
establish live decoder or RF behavior.

### TX selection branches in real applications

`bridge_start_tx_selection_runtime_ab.py` compares the original and extracted
start-TX selection in isolated apps. A test-only stop before audio/PTT verifies
CQ fallback, acceptance of a synthetic Hound payload, SWR rejection and disabled
slot rejection. Both variants agree, with CAT disconnected and no RF. This is
not an active over-the-air Hound QSO.

### SSTV playback lifecycle A/B

`bridge_sstv_tx_lifecycle_loopback_ab.py` compares lifecycle method placement
with two isolated Radiofax sessions over BlackHole. It checks real SoundOutput
playback tracking, release of bridge TX ownership, successful completion and a
complete received image in both variants. VOX is simulated; no physical PTT or
RF is exercised. Compact results are retained in `bridge_refactor_evidence`.

### MAM expiry and retry runtime A/B

`bridge_mamprune_expiry_runtime_ab.py` now exercises the real Bridge in both
placements with a retry-exhausted slot, a never-called expired slot, a live slot,
an expired late recovery and stale/fresh cooldowns. It checks retained recovery,
resume carry and change signals. CAT and TX remain disabled; this does not
exercise an on-air QSO.

### Legacy ALL.TXT augmentation runtime A/B

`bridge_legacyaugment_runtime_ab.py` invokes the original and extracted methods
on the real Bridge with a test-only redirected ALL.TXT in a temporary profile.
Both prime the file, append a synthetic FT8 decode and avoid duplicating it on
the next read. Decode filters are explicitly bypassed in this test; filter
behavior is exercised separately. CAT and TX remain disabled, and the temporary
path override is restored after the run.

### UI stall during active virtual monitoring A/B

`bridge_uistall_monitor_runtime_ab.py` compares original and extracted stall
methods in real isolated apps while monitoring BlackHole 16ch. A controlled
1.2-second main-thread stall activates the pressure guard in both versions.
CAT remains disconnected and TX inactive. This checks virtual audio monitoring,
not physical receiver or RF behavior.

### Log preview snapshots runtime A/B

`bridge_logpreview_snapshot_runtime_ab.py` compares original and extracted
preview methods in isolated apps for frozen snapshot, pending and live QSO
state, mode and comment overrides, and end-time fallback. CAT and TX remain
disabled; no QSO is submitted.

JT65 reference comparator availability audit: the existing `jt65_decode65a_compare` target requires `DECODIUM_JT65_REFERENCE_LIBRARY` to point to an official WSJT-X `libwsjt_fort.a`. The current build has no such target (`ninja: unknown target`); the project contains a library at `lib/ft2/libwsjt_fort.a`, but its provenance as an official independent reference has not been established, so it was not substituted. The comparator also checks wrapper/direct equivalence, not necessarily a successful expected-message decode. Inspection confirms that the native decoder copies at most the first 52 seconds of the supplied PCM window. No conclusion about the JT65 failure can be drawn from this inspection alone.

JT65 frontend cross-check: the separately built `jt9` executable at `/Users/salvo/Desktop/ft2/decodium3-build/build-utils/jt9` identifies itself as a native C++ decoder frontend, and its CMake cache points to the Decodium-derived `decodium3-build` source, not an independently verified official WSJT-X build. Using the same SHA-256-verified `130610_2343.wav`, the command `jt9 -6 -d 3 -L 700 -H 1100 -f 975 -F 150 <wav>` exited 0 but reported `<DecodeFinished> 0 0 0`, with no decoded rows. This supports reproducing the failure outside the Bridge app test in a related native frontend; it neither establishes an independent decoder result nor identifies the cause.

Method-inventory follow-up: `bridge_legacyptttransition_runtime_lab.py` was rerun against both placements. It passed pending, confirmed and cleared state, command and matching feedback confirmation, timeout, cancellation and simulated audio failure, with CAT and TX off. These operations indirectly exercise the PTT transition helpers despite their names being absent from the test-source token inventory. The test does not exercise a physical relay or actual rig feedback. Compact output: `bridge_refactor_evidence/legacyptttransition-coverage-recheck-ab.txt`.

Method-inventory follow-up: `bridge_sstv_wav_replay_cancel_ab.py` was rerun for both placements and passed replay start, cancellation, completion and simulated route restoration, with matching results, CAT and TX off. This exercises the replay cleanup and restore flow indirectly; its producer-tap rebind remains a test marker and is not proof of physical audio capture. Output: `bridge_refactor_evidence/sstv-replay-cancel-coverage-recheck-ab.txt`.

SSTV image-save unavailable-storage guard A/B: `bridge_sstv_image_save_guards_ab.py` forces the actual Bridge storage-ready flag off in isolated before/after applications. Both reject an automatic save without changing idle status and reject a manual save with a nonempty error, with CAT and TX off. This is an injected readiness condition, not an actual database or disk failure. The script restores sources and rebuilds; compact results: `bridge_refactor_evidence/sstv-image-save-guards-ab.txt`.

Build inclusion audit (2026-11-10): all 410 `src/bridge/DecodiumBridge*.cpp` files have compilation rules for both `wsjtx` and `decodium_qml` in the current `build/build.ninja`. `cmake --build build --target wsjtx decodium_qml -j 2` completed successfully. The existing `build/compile_commands.json` did not contain the recently extracted files, so it was not used to infer current target membership. This confirms the configured macOS build only; it does not establish functional coverage or other platforms' builds.

After the station, Cabrillo and TX-protection application A/B probes, both macOS targets were rebuilt successfully on 2026-11-10, and the three new Python scripts passed `py_compile`. The linker warned that locally installed Homebrew dependencies target newer macOS releases than the configured deployment target; this build alone does not prove compatibility with macOS 13.

After the calibration, network and populated-history application probes, both macOS targets rebuilt successfully again. A check found no remaining processes for these probes, no temporary probe markers in `src/bridge`, and no profiles with the six new test prefixes in the inspected Qt test application-support and cache directories. This is a scoped cleanup check, not an audit of all temporary files or user data.

Regression availability check (2026-11-10): the configured CTest inventory contained `ft2_async_ring_test` and `test_fox_elastic_cycle` without built executables. Both targets were built explicitly and both CTests passed; `test_custom_cq_persistence` also passed against the relocated sources. These are focused regressions, not proof of all CTests or RF behavior.

Documentation and source-regression audit (2026-11-10): all 810 distinct `bridge_*.py` script names mentioned in this report exist under `tests`; this is a filename check, not a test-execution or coverage claim. The configured `test_audio_mode_change_capture_policy`, `test_file_dialog_focus`, and `test_rigctl_dashboard_controls` targets were built and their focused CTests passed on macOS. These checks do not verify a physical audio device, file dialog interaction or rig.

TX completion guard follow-up: `bridge_complete_tx_guards_ab.py` compares the original and extracted completion method in isolated applications. With CAT disabled, it injects a pending PCM buffer and future playback hold then checks that an error clears TX, CW state, hold and buffer; a second simulated case checks forced completion at the five-held-reason limit. Both placements pass. The test supplies synthetic state and does not use a physical PTT or audio device. The baseline harness was updated for moved helper declarations before the successful run. Evidence: `bridge_refactor_evidence/complete-tx-guards-ab.txt`.

TX completion timer follow-up: `bridge_complete_tx_hold_timer_ab.py` injects a 300 ms pending hold and two completion requests in isolated before/after app sessions. Both defer, coalesce the second request, then the real Qt timer clears TX and the CW state with exactly one transmitting-ended notification. CAT is disabled; the TX state is synthetic and no audio device or radio is used. Compact output: `bridge_refactor_evidence/complete-tx-hold-timer-ab.txt`.

The TX completion timer A/B lab was repeated after strengthening explicit assertions for deferred state, coalescing and exactly one completion notification. Both placements passed and the script restored/rebuilt the application; `bridge_refactor_evidence/complete-tx-hold-timer-repeat-ab.txt`.

TX completion cross-thread A/B: `bridge_complete_tx_worker_thread_ab.py` calls the actual completion method from a C++ worker thread in isolated before/after apps. In both placements the state remains active until the queued callback runs, then one completion notification occurs on the Bridge owner thread and CW/TX state clears. CAT is disabled; the TX state and caller are synthetic, and no audio or radio hardware is exercised. Evidence: `bridge_refactor_evidence/complete-tx-worker-thread-ab.txt`.

TX completion cross-thread error A/B: `bridge_complete_tx_worker_error_ab.py` submits a simulated error from a worker while a future hold is active. Both placements queue completion to the Bridge owner thread, clear TX/CW and the hold, and emit exactly one completion and one error notification. CAT is disabled and no physical audio or radio is involved. Evidence: `bridge_refactor_evidence/complete-tx-worker-error-ab.txt`.

SSTV replay source guard A/B: `bridge_sstv_replay_source_guard_ab.py` starts an actual synthetic WAV replay and sends TCI reset and invalid-source requests while replay is active. Both placements preserve the same Replay route and generation without reporting source errors. CAT and TX are off; the source changes are injected and no physical device or live TCI is used. Evidence: `bridge_refactor_evidence/sstv-replay-source-guard-ab.txt`.

SSTV invalid-source notification A/B: `bridge_sstv_invalid_source_guard_ab.py` verifies that an invalid source is ignored while RX is inactive, but an unknown source and a zero stream ID each emit one error without changing the synthetic active route. Both placements match with CAT and TX disabled; there is no physical audio or TCI connection. Evidence: `bridge_refactor_evidence/sstv-invalid-source-guard-ab.txt`.

SSTV source notification A/B: `bridge_sstv_source_signals_ab.py` compares actual Bridge signals with a synthetic active RX route. A source change emits one each of state, source-device and snapshot notifications; a repeated identical selection emits none; a reset emits one more of each. Both placements agree with CAT and TX off; no live TCI or physical input is used. Evidence: `bridge_refactor_evidence/sstv-source-signals-ab.txt`.

SSTV stale-source PCM A/B: `bridge_sstv_stale_source_pcm_ab.py` switches a synthetic active route and resets it, then submits PCM tagged with the prior and current route tokens. Both placements reject stale tokens and accept the current tokens in both cases. CAT/TX are off; direct synthetic enqueue does not reproduce a concurrent physical producer callback. Evidence: `bridge_refactor_evidence/sstv-stale-source-pcm-ab.txt`.

SSTV relay concurrency follow-up: `bridge_sstv_audio_relay_ab.py` passed again for both placements with HAMDRM compiled in and out. Its controlled producer blocks inside a fake receiver while another thread calls `disableAndDrain`; the drain remains blocked until the producer is released, then subsequent submit is rejected. This checks the extracted relay's synchronization with substitutes, not a concurrent physical capture device. Evidence: `bridge_refactor_evidence/sstv-audio-relay-concurrency-repeat-ab.txt`.

Start-TX preflight user notifications A/B: `bridge_start_tx_preflight_notices_ab.py` checks mixed-case JTTY emits one status, simulated remote CW emits one error, and RTL-SDR receive-only mode emits one error, for both native and legacy backend selections. Both placements passed all six checks and kept CAT/TX off. Conditions and backend choice are simulated; no hardware transmission is performed. Evidence: `bridge_refactor_evidence/start-tx-preflight-notices-ab.txt`.

TX dispatch notification follow-up: `bridge_txdispatchguard_notifications_ab.py` uses isolated before/after apps to block an automatic TX6 twice, verify exactly one TX-enabled change notification, allow an explicitly armed manual TX6 through hold/startup guards, and reject that request after changing slots. Both placements agree; CAT/TX are off and the requests are synthetic. Evidence: `bridge_refactor_evidence/txdispatchguard-notifications-ab.txt`.

Smart FT2 cancellation timer A/B: `bridge_smartft2_cancel_timer_ab.py` schedules a synthetic async request in both app placements, invokes the actual deferred-request invalidation and waits 3.2 seconds while monitoring transmission state. Both clear pending state, advance generation and serial, and remain out of TX. CAT and real monitoring are disabled; this checks cancellation of the scheduled callback, not a physical rig or successful transmission. Evidence: `bridge_refactor_evidence/smartft2-cancel-timer-ab.txt`.

Smart FT2 manual-hold timer A/B: `bridge_smartft2_hold_timer_ab.py` schedules an async request, applies manual hold without changing its timer serial or generation, then waits 3.2 seconds. In both placements the real callback clears pending state without entering TX. CAT and real monitoring are disabled; the timer and request state are synthetic, and no physical rig is involved. Evidence: `bridge_refactor_evidence/smartft2-hold-timer-ab.txt`.

Smart FT2 mode guard timer A/B: `bridge_smartft2_mode_timer_ab.py` schedules a synthetic async request, changes the internal mode to FT8 without changing timer serial/generation and waits 3.2 seconds. Both app placements clear the pending request without entering TX. This tests the callback guard directly rather than the full public mode-change lifecycle; CAT and real monitoring are disabled. Evidence: `bridge_refactor_evidence/smartft2-mode-timer-ab.txt`.

Independent-decoder availability follow-up: `/Applications/jtdx.app` reports version v2.2.159-32A and contains an arm64 `jtdxjt9` binary with JT65 decoder symbols. A trial CLI invocation on the SHA-256-verified reference WAV exited 0 but emitted no output; `--help` also emitted no output. No supported direct-file invocation was established, so this is not an independent decoding result and does not resolve JT65. No user JTDX settings were changed.

JTDX documentation follow-up: the documentation bundled with the installed JTDX describes opening `130610_2343.wav` from the GUI and identifies a JT65 signal near 815 Hz originating from W7VP in that recording. This is guidance for a separate expected signal and suggests an additional target for the existing 200–3000 Hz native test. The documentation is not itself an observed decode by this installation; no JTDX radio settings were deliberately changed. A direct GUI-based independent decode remains to be performed in an isolated profile.

JTDX isolation follow-up: the installed executable advertises `rig-name` and `test-mode` options, but the available native UI automation surface failed to start, so the GUI comparison was not executed. These option strings alone do not prove isolation of radio configuration, and no independently observed JT65 decode is claimed.

JTDX standalone limitation: upstream JTDX `lib/jt9.f90` states that this decoder is a shared-memory backend and calls `jt9a()` only for that mode; the file-reading code is not active. Source inspected: https://raw.githubusercontent.com/jtdx-project/jtdx/master/lib/jt9.f90 (upstream master, not verified identical to the installed binary). A second local attempt with the original sample filename and `--jt65` likewise exited without output. This explains a plausible CLI limitation but does not independently decode the recording.

Independent JT65 control: official WSJT-X 3.0.2 Apple Silicon was downloaded from the official GitHub release and its package SHA-256 `1b7eff83de7ded13c75b8e65f1eab75a6143d6ecb94c140e4764933b210b23bb` matched the release metadata. Without installing the package, its extracted `jt9` decoded nine JT65 messages from the hash-verified sample, including `CQ DL7ACA JO40` at 975 Hz and `KK4DSD W7VP -16` at 815 Hz, with deep decoding and 200–3000 Hz range. Details: `bridge_refactor_evidence/jt65-official-wsjtx-reference.txt`. This independently confirms the recording and expected message; the native Decodium before/after failure remains unresolved, and its cause is not yet attributed to the refactor or decoder.

JT65 window-length diagnostic: both original and extracted Bridge placements were temporarily run with the native worker's 52-second PCM allocation enlarged to 60 seconds, using the independently verified reference WAV. Both still returned empty JT65 rows and the expected-message test failed; `bridge_refactor_evidence/jt65-60second-probe-ab.txt`. This rules out the allocation length alone under this instrumented setup, not every timing or decoder difference. `Detector/JT65Decoder.cpp` was restored byte-for-byte and the application rebuilt. The official WSJT-X source declares a 60-second buffer; that declaration alone does not prove its effective input length or make the change correct.

Independent 52-second control: official WSJT-X 3.0.2 `jt9` decoded the same nine JT65 messages, including `CQ DL7ACA JO40`, from a derivative of the verified reference WAV with the first 52 seconds retained and the final eight seconds replaced by silence. The output UTC was 0000 because the derivative filename was not in the expected timestamp form; signal content still decoded. This strengthens the conclusion that discarding this recording's last eight seconds alone does not explain the native failure. The separately available `decodium3-build` comparison tools passed their repeat-state checks on this sample, but their source calls the related native implementation twice, so they are not an independent Fortran-versus-native comparison and do not establish successful message decoding.

JT65 PCM integrity diagnostic: `bridge_jt65_pcm_integrity_probe.py` instruments both Bridge placements to hash the exact `DecodeRequest` PCM at dispatch. Both match the verified WAV's mono 16-bit 12 kHz PCM SHA-256 `dde7805efb626243680f60468b422e1ab7600600b1849d19c6162b192e56b84f`; both still return no expected JT65 message. The probe checks the injected forced-snapshot route, not all live audio capture, and intentionally does not claim a decode success. Instrumentation was restored and the app rebuilt. Evidence: `bridge_refactor_evidence/jt65-pcm-integrity-ab.txt`.

JT65 dispatch parameter diagnostic: `bridge_jt65_request_parameters_probe.py` logs the actual reference request in both Bridge placements. Both supplied the matching verified PCM with range 200–3000 Hz, RX frequency 1500 Hz, tolerance 0, depth 3, new-data 1, two-pass field 1, submode 0 and experience flags 0. Both returned no expected JT65 row. A separate earlier instrumented probe explicitly set tolerance 150 and RX frequency 975 and also failed; therefore the default tolerance alone does not explain the failure. These are forced-snapshot native tests, not normal legacy RX. Evidence: `bridge_refactor_evidence/jt65-request-parameters-ab.txt`.

JT65 algorithm comparison follow-up: the official WSJT-X 3.0.2 `filbig.f90` uses a 672,000-sample FFT and sets its `newdat` argument to zero after caching the FFT. The native `filbig_compute` uses the same nominal FFT length; its caller currently passes `newdat=1` for each candidate. This is a source-level difference in cache handling, not an established cause of failed decoding. The `decodium3-build` Fortran source differs from official `decode65a.f90` at least in its FFT compatibility call and its existing comparator tests repeated native state, so it cannot be represented as an official independent equivalence test. Official source: https://github.com/WSJTX/wsjtx/blob/v3.0.2/lib/filbig.f90.

JT65 stage comparison against locally compiled official WSJT-X 3.0.2 source: official source archive SHA-256 `574aee4a36c58c4dfeda4f2d659e74542212d4d9c25bfc040feba47c9f2b15d7` matched GitHub release metadata. `wsjt_fort` was built separately in `/private/tmp`. The first comparator link failed on duplicate `pctile_`; a copy of the official archive with only that object removed was then linked, sharing Decodium's percentile implementation, so this is not a fully isolated official pipeline. On the reference WAV, symbol-spectrum and synchronization comparators reported agreement, while `jt65_decode65a_compare` failed: the Fortran-side call decoded `N2SU W0JMW R-14` with dt 1.7463 and nft=1, whereas the native call returned an empty message, dt 4.0925 and nft=0. The two calls are sequential and share process state, so the first divergent internal operation and causality remain unproven. Evidence: `bridge_refactor_evidence/jt65-official-stage-comparison.txt`. CMake options `WSJT_BUILD_UTILS=OFF` and empty `DECODIUM_JT65_REFERENCE_LIBRARY` were restored.

JT65 native filter correction: the official `filbig` uses FFTW_FORWARD (-1) for the 77,175-point output transform; the native implementation used +1. A temporary stage comparison on the independently decoded WAV found matching output lengths/power but very different complex samples; after changing only the native sign to -1, all 77,175 output samples compared exactly in that experiment and both official and native `decode65a` returned `N2SU W0JMW R-14` (the comparator still failed its tight numerical sync tolerance: 434.730652 versus 434.775726). The official Fortran archive in this diagnostic used the shared Decodium percentile implementation due to a duplicate linker symbol; results are not a fully isolated internal equivalence proof. The sign change is now in production source. `bridge_feeddecoder_jt65_reference_ab.py` passed both pre-extraction and extracted Bridge placements using this corrected decoder, and both produced the expected CQ plus eight other decoded rows; compact results are in `bridge_refactor_evidence/jt65-reference-fixed-ab.txt`. Prior uncorrected before/after failures remain recorded above. This proves the forced native snapshot and publication path for this recording on macOS, not live RF or normal legacy-mirror reception.

After the JT65 filter correction, `test_q65_decoder_continuity` and `test_bridge_txwave_real_ab` passed and both macOS app targets built. The source file retains its existing CRLF endings; `git -c core.whitespace=cr-at-eol diff --check` passes.

JT65 FFT direction direct A/B: `bridge_jt65_fft_direction_ab.py` runs the same hash-verified reference recording against the previous (+1) and corrected (-1) native filter signs; for each sign it also tests both Bridge placements. With +1 both fail to decode/display the expected CQ; with -1 both pass. The lab restores the corrected source and rebuilds. This is a forced native snapshot/publication test with CAT disabled, not live RF or normal legacy-mirror reception. Evidence: `bridge_refactor_evidence/jt65-fft-direction-ab.txt`.

JT65 decoded-content comparison: `bridge_jt65_compare_reference_messages.py` checks saved official WSJT-X output against each corrected Bridge placement and confirms all nine message texts match; it deliberately does not require equal SNR, time or frequency estimates and does not itself run a decoder. After the filter fix, `bridge_feeddecoder_jt9_reference_ab.py` was rerun on the same independently sourced WAV; both Bridge placements decoded and displayed the expected JT9 message, with CAT/TX off and native publication forced. Evidence: `bridge_refactor_evidence/jt9-post-jt65-fix-ab.txt`.

JT65 callback publication A/B: `bridge_ft8_ready_jt65_reference_ab.py` runs the corrected decoder with the independent WAV and compares original/extracted `onFt8DecodeReady` placement. Both app builds decode and publish `CQ DL7ACA JO40` to the decode list. The lab forces native snapshot/time-sync/publication in this macOS legacy environment; CAT and TX are disabled, so this does not prove normal legacy mirror or RF. The old callback harness required its moved helper declarations to be forwarded when rebuilding the baseline placement. Evidence: `bridge_refactor_evidence/jt65-callback-reference-ab.txt`.

JT65 synthetic transmit waveform follow-up: after the filter-direction fix, `bridge_feeddecoder_jt65_ab.py` was updated for the extracted helper declarations and an explicit 200–3000 Hz receive range. Both Bridge placements decoded and displayed `CQ K1ABC FN42` generated by Decodium's JT65 waveform builder with added deterministic noise. This is an in-process generated waveform and forced native snapshot/publication, not an independent radio transfer; CAT/TX remained off. It supersedes the earlier failed synthetic test for this configuration. Evidence: `bridge_refactor_evidence/jt65-synthetic-fixed-ab.txt`.

JT65 synthetic false-candidate follow-up: the worker produced an additional invalid-looking `XM5979JQU-6E3` candidate in both placements, but the actual decode list at the moment of expected CQ publication contained only `CQ K1ABC FN42` in both. `bridge_feeddecoder_jt65_ab.py` now checks that this particular candidate is absent from the displayed list. This records one synthetic candidate and one observation point, not a general false-decode rate or proof about every JT65 invalid message. Evidence: `bridge_refactor_evidence/jt65-synthetic-display-ab.txt`.

After the JT65 filter correction, the strengthened synthetic decode-list assertion passed for both Bridge placements (`bridge_refactor_evidence/jt65-synthetic-display-assert-ab.txt`), and `python3 tests/run_bridge_refactor_ab.py --base v1.0.737` exited 0 on macOS. The suite's coverage remains limited to its implemented checks and does not establish RF or all application paths.

The full `run_bridge_refactor_ab.py --base v1.0.737` command was rerun after the subsequent application probes on 2026-11-10 and again exited successfully. This is a fresh run of its implemented A/B checks, not proof of complete method or branch coverage.

JT65 regression follow-up: `test_jt65_native_decode` is now a CTest that generates a deterministic noisy JT65 signal and requires the native decoder to recover `CQ K1ABC FN42`. It passed on the configured macOS build. This is a local synthetic encode/decode test; the earlier FFT-direction A/B and independent WSJT-X comparison remain the evidence for the original defect, and this test does not exercise a live radio or the complete Bridge UI path.

FST4-15 clean-signal independent control: the deterministic simulator generated `CQ K1ABC FN42`, TR 15 s, 1500 Hz, DT -0.5, no fading and SNR 100 (noise disabled); 0.5 s silence was prepended without wrapping. The padded mono PCM SHA-256 was `5eeb50dd0c35ed35c2c3df42d719b8478d02051a082259507c80fea5af6608d4`, reproduced across independent simulator runs. Official WSJT-X 3.0.2 `jt9 -7 -p 15 -d 2 -f 1500 -L 1480 -H 1520` decoded the expected CQ. The forced native app test with `DECODIUM_FST4_TEST_SNR=100` returned the same five unrelated raw rows and no expected message in both Bridge placements; it intentionally exited nonzero. Noisy -10 dB prior tests passed. Although simulator parameters and regenerated PCM were verified, the failing app run did not independently log its input hash, so the exact delivered bytes are not proven in that run. CAT/TX were disabled. Evidence: `bridge_refactor_evidence/fst4-clean-official-comparison-ab.txt`. No production decoder change was made.

FST4-15 clean PCM integrity: `bridge_fst4_clean_pcm_integrity_probe.py` hashes the actual native `DecodeRequest` PCM for both Bridge placements. Both hashes equal the independent control's PCM SHA-256 `5eeb50dd0c35ed35c2c3df42d719b8478d02051a082259507c80fea5af6608d4`, yet neither produces the expected CQ. The diagnostic exits successfully only when this specific known unsuccessful outcome is reproduced; it is not a passing decode test. CAT/TX are disabled and native snapshot/publication are forced. Evidence: `bridge_refactor_evidence/fst4-clean-pcm-integrity-ab.txt`.

Correction to the initial FST4-15 clean control: `bridge_fst4_clean_request_parameters_probe.py` showed the requested 1480–1520 Hz bounds become 1480–1530 Hz in both app placements due to the minimum 50 Hz interval; tolerance is 150 Hz and depth 2. Repeating official WSJT-X with matching 1480–1530 Hz and tolerance 150 also produced no decode. With upper bound 1520 or 1550, the official decoder recovered the CQ; tolerance 20 and 150 behaved alike in these cases. `bridge_fst4_clean_wide_probe_ab.py` then tested 1480–1550 Hz at tolerance 150: both Bridge placements received PCM with the verified SHA-256 and decoded/displayed `CQ K1ABC FN42`. Thus the earlier apparent native-versus-official difference used nonmatching frequency bounds. The 1530 Hz clean-signal sensitivity remains observed in both implementations, not attributed to the Bridge refactor. Forced native snapshot/publication and CAT/TX isolation apply. Evidence: `bridge_refactor_evidence/fst4-clean-matched-range-ab.txt` and `bridge_refactor_evidence/fst4-clean-wide-ab.txt`.

The official FST4 bound/tolerance matrix is reproducible with `bridge_fst4_official_bounds_control.py`, given an independently obtained WSJT-X `jt9` and the hash-verified synthetic WAV. Six observed cases are saved in `bridge_refactor_evidence/fst4-official-frequency-bounds.json`; the tool uses an isolated temporary data directory and does not control a radio. It documents this particular signal and decoder version, not a general performance boundary.

JT65 period-timer A/B: `bridge_period_timer_jt65_audio_ab.py` injects the independently verified reference PCM into the Bridge receive buffer, advances a completed 60-second slot and calls the original or extracted period timer. Both app placements decode and display `CQ DL7ACA JO40` with CAT/TX disabled. The test forces native time-sync dispatch and publication in the macOS legacy environment; it does not exercise physical audio capture, RF or normal legacy mirroring. Evidence: `bridge_refactor_evidence/jt65-period-timer-ab.txt`.
