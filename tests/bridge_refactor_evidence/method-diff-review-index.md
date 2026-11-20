# Candidate evidence for source-comparison outliers

A navigation index, not proof that cited scripts execute each method or that differences are defects. Review test bodies and retained results before using any candidate as evidence.

| Module | Method | Candidate scripts | Report lines |
| --- | --- | --- | --- |
| `DecodiumBridgeAudioDeviceCacheRefresh.cpp` | `refreshAudioDeviceCache` | `bridge_audio_device_recovery_ab.py`, `bridge_audiocacheaccess_ab.py`, `bridge_audiocachehelpers_ab.py`, `bridge_audiocacherefresh_ab.py` | 756, 760, 762, 1405 |
| `DecodiumBridgeAutoCqHistory.cpp` | `rememberRecentAutoCqAbandoned` | `bridge_autocqhistory_ab.py`, `bridge_autocqhistory_runtime_lab.py`, `bridge_mamprune_ab.py` | 1101, 1219 |
| `DecodiumBridgeAutoCqHistory.cpp` | `rememberRecentAutoCqWorked` | `bridge_autocqhistory_ab.py`, `bridge_courtesy_ab.py`, `bridge_courtesy_success_runtime_ab.py`, `bridge_mamcaller_ab.py`, `bridge_mamlog_ab.py` | 1113, 1219, 1223, 1227 |
| `DecodiumBridgeAutoLogSnapshot.cpp` | `armLateAutoLogSnapshot` | `bridge_autologsnapshot_ab.py` | 746 |
| `DecodiumBridgeAutoRxLevel.cpp` | `updateAutoRxInputLevel` | `bridge_autorx_ab.py` | 702 |
| `DecodiumBridgeConstructor.cpp` | `DecodiumBridge` | `bridge_adif_full_export_ab.py`, `bridge_adif_import_wrapper_ab.py`, `bridge_adifappend_ab.py`, `bridge_adifsupport_ab.py`, `bridge_adiftcp_ab.py`, `bridge_adifudp_ab.py` | 7, 12, 54, 69, 254, 272, 308, 342, 382, 420, 427, 441 |
| `DecodiumBridgeCourtesySignoff.cpp` | `maybeStartCourtesySignoff` | `bridge_courtesy_ab.py`, `bridge_courtesy_runtime_lab.py`, `bridge_courtesy_success_runtime_ab.py` | 1223 |
| `DecodiumBridgeCpuPressure.cpp` | `effectiveFtThreadLimit` |  |  |
| `DecodiumBridgeCpuProfile.cpp` | `setFtThreads` | `bridge_psk_ab.py` | 660 |
| `DecodiumBridgeCpuProfile.cpp` | `cycleFtThreads` |  |  |
| `DecodiumBridgeDecodeFilterSettings.cpp` | `readDecodeUserFilterConfig` | `bridge_autocallselect_ab.py`, `bridge_ft4_legacy_revision_diagnostic_ab.py`, `bridge_ft8_ready_legacy_immediate_ab.py`, `bridge_ft8_ready_legacy_publish_ab.py`, `bridge_legacy_sync_blacklist_ab.py`, `bridge_legacy_sync_cq_filter_ab.py` | 1439, 1616, 1618, 1620, 1684, 1686, 1688, 1690, 1700 |
| `DecodiumBridgeDecodeRowParsers.cpp` | `parseFt8Row` | `bridge_autocallselect_ab.py`, `bridge_caller_scoring_ab.py`, `bridge_ft2_sync_ready_cache_ab.py` | 986, 1439, 1654 |
| `DecodiumBridgeDecodeRowParsers.cpp` | `parseWsprRow` |  |  |
| `DecodiumBridgeDecodeSecondary.cpp` | `drainLegacyDecodeSecondaryWork` |  |  |
| `DecodiumBridgeDecodeSecondary.cpp` | `drainNativeDecodeSecondaryWork` |  |  |
| `DecodiumBridgeFt2Access.cpp` | `ft2LinkAccessPasswordConfigured` |  |  |
| `DecodiumBridgeFt2Access.cpp` | `verifyFt2LinkAccessPassword` | `bridge_ft2_access_runtime_lab.py`, `bridge_ft2_access_success_ab.py` | 308 |
| `DecodiumBridgeHalt.cpp` | `haltWithReason` | `bridge_halt_ab.py` | 1443 |
| `DecodiumBridgeLegacyAllTxt.cpp` | `appendLegacyAllTxtDecodeLine` |  |  |
| `DecodiumBridgeLegacyAugment.cpp` | `augmentLegacyMirrorWithAllTxt` | `bridge_legacyaugment_ab.py`, `bridge_legacyaugment_runtime_ab.py` | 894, 896, 2723 |
| `DecodiumBridgeLegacyDialogSync.cpp` | `syncLegacyBackendDialogState` | `bridge_legacydialogsync_ab.py` | 782 |
| `DecodiumBridgeLegacyHash.cpp` | `resolveFt8HashPlaceholdersFromRecentAllTxt` | `bridge_legacyalltxt_ab.py`, `bridge_legacyhash_app_ab.py`, `bridge_legacyhashbridge_ab.py` | 652, 892 |
| `DecodiumBridgeLoadSettings.cpp` | `loadSettings` | `bridge_fontscale_ab.py`, `bridge_load_settings_ab.py`, `bridge_load_settings_audio_bounds_ab.py`, `bridge_reload_settings_runtime_ab.py`, `bridge_tx_audio_startup_ab.py` | 876, 1032, 2454 |
| `DecodiumBridgeLogPreview.cpp` | `pendingLogQsoPreview` | `bridge_autologsnapshot_runtime_lab.py`, `bridge_log_prompt_runtime_lab.py`, `bridge_logpreview_ab.py`, `bridge_logpreview_runtime_lab.py`, `bridge_logpreview_snapshot_runtime_ab.py`, `bridge_txappend_runtime_lab.py` | 698, 746, 750, 752, 754, 2740 |
| `DecodiumBridgeLogQso.cpp` | `logQsoNow` | `bridge_log_qso_legacy_delegate_ab.py`, `bridge_log_qso_legacy_real_ab.py`, `bridge_log_qso_runtime_ab.py`, `bridge_logstall_ab.py`, `bridge_mamlog_ab.py` | 868, 1113, 1812, 1814, 1816, 1818 |
| `DecodiumBridgeMamPrune.cpp` | `mamPruneSlots` | `bridge_mamprune_ab.py`, `bridge_mamprune_expiry_runtime_ab.py` | 1101, 2715 |
| `DecodiumBridgeModulatorFinish.cpp` | `finishModulatorIdlePlayback` | `bridge_complete_tx_audio_ab.py`, `bridge_modulator_cw_pending_ab.py`, `bridge_modulator_finish_loopback_ab.py`, `bridge_modulator_ft2_audio_ab.py`, `bridge_modulator_idle_ft2_ab.py`, `bridge_modulator_idle_legacy_ab.py` | 1215, 1826, 1828, 1830, 1832, 1834, 1838, 1850, 1876, 1884 |
| `DecodiumBridgePskReporter.cpp` | `replayPskReporterRecentEntries` | `bridge_decode_cluster_connections_ab.py`, `bridge_psk_replay_app_ab.py` | 662, 2010 |
| `DecodiumBridgeRemoteActivity.cpp` | `publishRemoteActivityEntry` | `bridge_remote_activity_ws_ab.py` |  |
| `DecodiumBridgeSaveSettings.cpp` | `saveSettingsInternal` |  |  |
| `DecodiumBridgeSstvCalibrationTx.cpp` | `startSstvCalibrationTone` | `bridge_sstv_calibration_tx_capture_ab.py`, `bridge_sstv_calibration_tx_extraction_ab.py`, `bridge_sstv_tx_init_capture_ab.py` | 2238, 2242 |
| `DecodiumBridgeSstvControls.cpp` | `sstvRxControls` | `bridge_sstv_controls_runtime_lab.py` |  |
| `DecodiumBridgeSstvFax480Tx.cpp` | `startSstvFax480Tx` | `bridge_sstv_fax480_tx_reject_ab.py` | 2236 |
| `DecodiumBridgeSstvHamDrmTx.cpp` | `startHamDrmPreparedAudio` | `bridge_sstv_hamdrm_tx_cancel_ab.py` | 2240 |
| `DecodiumBridgeSstvRadiofaxTx.cpp` | `startSstvRadiofaxTxAdvanced` | `bridge_sstv_radiofax_tx_reject_ab.py` | 2234 |
| `DecodiumBridgeSstvStorageStartup.cpp` | `initialiseSstvStorage` | `bridge_sstv_storage_startup_ab.py`, `bridge_sstv_storage_startup_share_ab.py` | 2488, 2491 |
| `DecodiumBridgeSstvTxInit.cpp` | `initialiseSstvTx` |  |  |
| `DecodiumBridgeSstvTxStart.cpp` | `startSstvTx` | `bridge_sstv_tx_start_loopback_ab.py`, `bridge_sstv_tx_start_reject_ab.py` | 2134 |
| `DecodiumBridgeStartTx.cpp` | `startTx` | `bridge_autocq_two_app_lab.py`, `bridge_catrigmode_ab.py`, `bridge_directaudiotx_ab.py`, `bridge_periodic_tx_cancel_ab.py`, `bridge_periodic_tx_direct_audio_ab.py`, `bridge_periodic_tx_dispatch_ab.py` | 1178, 1180, 1203, 1419, 1429, 1770, 1772, 1774, 1776, 1778, 1780, 1782 |
| `DecodiumBridgeStationFrequencies.cpp` | `addStationFrequencyRow` | `bridge_frequency_calibration_app_ab.py`, `bridge_stationfreq_restart_ab.py` | 441, 467 |
| `DecodiumBridgeStationFrequencies.cpp` | `updateStationFrequencyRow` | `bridge_stationfreq_restart_ab.py` | 467 |
| `DecodiumBridgeStopTx.cpp` | `stopTx` | `bridge_halt_ab.py`, `bridge_stop_tx_audio_ab.py`, `bridge_stop_tx_audio_runner.py`, `bridge_stop_tx_routes_ab.py`, `bridge_stop_tx_runtime_ab.py` | 1443, 1890, 1892, 1896 |
| `DecodiumBridgeSyncSchedule.cpp` | `scheduleSyncTxAtNextValidSlot` | `bridge_decodetxwait_ab.py`, `bridge_syncschedule_ab.py`, `bridge_syncschedule_timer_ab.py` | 1178, 1180, 1255 |
| `DecodiumBridgeTxPrecompute.cpp` | `resolveTxOutputDevice` | `bridge_audio_buffers_runtime_ab.py`, `bridge_audiobuffers_ab.py`, `bridge_cat_dispatch_ssb_runtime_ab.py`, `bridge_ssb_unit_runtime_ab.py`, `bridge_tx_output_fallback_ab.py`, `bridge_txprecompute_ab.py` | 726, 1148, 1199, 1888, 2473, 2532 |
| `DecodiumBridgeTxPrepare.cpp` | `ensureTxAudioPrepared` | `bridge_txprecompute_ab.py`, `bridge_txprepare_ab.py`, `bridge_txprepare_errors_ab.py` | 1148, 1199 |
| `DecodiumBridgeWindowState.cpp` | `saveWindowStatesAsync` |  |  |
| `DecodiumBridgeWorldMapEntry.cpp` | `replayWorldMapEntry` | `bridge_worldmapentry_ab.py`, `bridge_worldmapreset_ab.py` | 800, 808 |
| `DecodiumBridgeWorldMapQueue.cpp` | `queueWorldMapEntryForReplay` | `bridge_visualfeedscheduling_ab.py`, `bridge_worldmapqueue_ab.py` | 796, 798 |
| `DecodiumBridgeWorldMapReset.cpp` | `worldMapEntryFreshEnough` |  |  |

## Review of entries without a literal Python-tool reference

- CPU thread limit and cycling: the real extracted methods are exercised in
  `bridge_refactor_cpupressure_harness.cpp` and
  `bridge_refactor_cpuprofile_harness.cpp`; corresponding application tests
  check thread-limit and low-CPU controls. The retained full A/B log reports
  `cpuprofile: A/B OK`.
- WSPR row parsing: `bridge_refactor_decode_rows_harness.cpp` calls the parser
  with valid and invalid WSPR rows. The main report also records synthetic
  callback and reference-WAV app comparisons.
- Both secondary drains: `bridge_refactor_secondary_harness.cpp` enqueues
  legacy/native rows, pumps Qt events and asserts the queues drain with the
  expected order, deduplication and routed side effects. Collaborators are
  simulated; separate app evidence covers a WAV-derived active-station row.
- FT2 password configuration: `bridge_refactor_ft2_gate_harness.cpp` directly
  checks configured and missing credentials, and the retained full A/B log
  records the gate variants. A separate app test uses synthetic credentials.
- Legacy ALL.TXT append: `bridge_refactor_alltxt_harness.cpp` writes and reads
  a temporary file, checks content and guards, with a substituted legacy path
  provider. The report records a separate application file-writing probe.
- Settings save, SSTV TX initialization and world-map freshness: their paths
  and app A/B evidence are reviewed in `method-inventory-review.md` and
  `completion_audit.md`; the freshness harness calls the method directly.
- Async window-state save: `bridge_refactor_window_state_harness.cpp` writes
  temporary QSettings and waits for the real worker pool. The retained full
  A/B log reports `window state: A/B OK`; native visual placement is not
  covered by that result.

These references narrow the manual review; they do not assert that every
indirect application path or physical device behavior was observed. No test
was rerun for this documentation update.

## AutoCQ and automatic-log review

`rememberRecentAutoCqAbandoned` and `rememberRecentAutoCqWorked` are exercised
by `autocqhistory/runtime-ab.txt` in both application placements, with cache,
cooldown and queue checks. `armLateAutoLogSnapshot` is called directly in the
A/B harness with expiry and partner assertions; the separate app TX5 test
observes the pending snapshot/prompt, not the late snapshot itself. The
courtesy-signoff app A/B records prepared, started, released and cleared
states in `courtesy-success-runtime-ab.txt`, with TX dispatch suppressed.
Automatic RX gain was reviewed separately in `method-inventory-review.md`.
These observations do not prove an RF QSO. No tests were rerun in this review.

## Filtering, decoding and reporting review

- Decode filters and row parsers have before/after harness comparisons. The
  filter app probe observed blacklist rejection and bypass; it did not prove
  live RF or isolation of every legacy setting path.
- Legacy hash recovery has a direct app A/B over synthetic ALL.TXT data:
  `legacyhash-app-ab.txt` records successful resolution and wrong-mode guard
  in both placements. Legacy augmentation also has temporary-file harness
  coverage and separate app ALL.TXT evidence.
- QSO preview has a matching before/after app scenario and a separate A/B
  harness; it did not commit a real QSO. PSK replay app output records the
  same synthetic callsign queued in both placements (`psk-replay-app-ab.txt`).
- Remote activity has a controlled A/B harness and a real local authenticated
  WebSocket scenario. `remote-activity-websocket-ab.txt` records one synthetic
  activity and one reset in both placements, without external networking.
- Legacy dialog synchronization uses a recording substitute; the separate
  virtual-audio RX test does not observe every dialog setting or hardware CAT.

These are scoped observations from existing results, not a claim of full
hardware or all-mode coverage. No test was rerun for this review.

## TX preparation, start and stop review

The retained FT2 precompute A/B records 120,960 samples and the same SHA-256
for both placements and cache passes (`txprecompute-wave-hash-ab.txt`).
`start_tx/before.json` and `after.json` record both app pairs reaching startTX
and decoding the synthetic FT8 CQ. `modulator_finish/before.json` and
`after.json` record the finish callback reached and the CQ decoded in both
app pairs; `stop-tx-routes-repeat-ab.txt` records matching routed stop checks.
The main report additionally documents real in-memory pending PCM, synthetic
legacy PTT-off routing and a BlackHole HALT/silence check. CAT was disabled;
these virtual/synthetic tests do not establish actual relay or RF timing.
The precise-timer sync scheduling change is intentional and documented in
the main report, rather than an unchanged source move. No tests were rerun
for this review.

## SSTV and logging review

The main report records A/B app tests for synthetic QSO persistence (including
an actual legacy writer follow-up), isolated station-frequency persistence,
SSTV storage/controls and TX validation. Radiofax and FAX480 were exercised
between two local apps, a 1200 Hz calibration tone was captured, and HAMDRM
transferred an exact synthetic payload in both placements. The retained Robot
C12 capture `sstv-tx-start-captured-ab.txt` records complete receptions but
also a large last-column error; this remains a limitation, not a passing edge
quality claim. These observations do not prove EasyPal interoperability,
physical audio/PTT or all profiles. No tests were rerun for this review.

## HALT, MAM and frequency review

The `haltWithReason` difference includes an intentional fix rather than a
pure source move: the retained CW, FT2-Link and telemetry A/B state results
show the original reporting TX active after HALT while the corrected version
reports it inactive. Corrected-version BlackHole captures record silence
after HALT for CW and FT2-Link; these do not establish physical PTT/RF.
`mamprune` app results record the same surviving synthetic slot before and
after; station-frequency harness/app tests cover temporary records, update,
delete, errors and restart. Sync scheduling has a separately documented
intentional precise-timer change and controlled timer tests. These findings
are distinct from exact behavioral equivalence and are based on retained
results, not new executions.

## Constructor, audio cache, map and SSTV storage review

The constructor matches the saved pre-extraction fixture according to the
source-comparison inventory, and initializers have separate A/B app evidence;
startup timings and the intentionally deferred legacy initialization are
reviewed in `completion_audit.md`. Audio-device cache refresh has a before/after
harness with simulated devices and a two-app BlackHole recovery scenario. Map
entry and queue methods have controlled A/B tests and an isolated WAV-derived
contact/replay app comparison. SSTV storage startup has an A/B readiness test;
`sstv_storage_startup_share/before.json` and `after.json` record two signed,
encrypted image transfers and Gallery import through a local server in both
placements. These tests are scoped to the stated fixtures and virtual/local
services; they do not prove hardware or external service behavior. No tests
were rerun in this review.
