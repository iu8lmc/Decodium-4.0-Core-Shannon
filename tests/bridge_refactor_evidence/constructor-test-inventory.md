# Constructor test entry-point inventory

A syntactic inventory, not a list of required tests or a coverage claim. It includes scripts already pointing to this file before the recent 53 changes. Previous functional evidence remains in the main report.

| Script | Initializer tokens | Current audit |
| --- | --- | --- |
| `bridge_audio_device_init_ab.py` | `initialiseAudioDeviceNotifications()` | Current before fixture compiles in the constructor on macOS without extra headers. Earlier app A/B is described in `tests/BRIDGE_REFACTOR_AB.md`; the current script was not rerun. |
| `bridge_audio_device_init_loopback_ab.py` | `initialiseAudioDeviceNotifications()` | Prior before/after app results retained in `audio_device_init/before.json` and `after.json`: both observed initializer and decoded CQ over BlackHole; the script has not been rerun after its path edit. |
| `bridge_callsign_confirmed_adif_ab.py` | `initialiseCallsignIntelligence()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_callsign_init_ab.py` | `initialiseCallsignIntelligence()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_callsign_url_ab.py` | `initialiseCallsignIntelligence()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_cat_connections_ab.py` | `initialiseCatConnections()` | application A/B passed; `cat_connections_constructor/base.txt` |
| `bridge_cat_connections_confirm_ab.py` | `initialiseCatConnections()` | application A/B passed; `cat_connections_constructor/confirm.txt` |
| `bridge_cat_connections_connect_ab.py` | `initialiseCatConnections()` | application A/B passed; `cat_connections_constructor/connect.txt` |
| `bridge_cat_connections_frequency_ab.py` | `initialiseCatConnections()` | application A/B passed; `cat-frequency-constructor-ab.txt` |
| `bridge_cat_connections_mode_ab.py` | `initialiseCatConnections()` | application A/B passed; `cat_connections_constructor/mode.txt` |
| `bridge_cat_connections_ptt_ab.py` | `initialiseCatConnections()` | application A/B passed; `cat_connections_constructor/ptt.txt` |
| `bridge_cat_connections_rigctl_ab.py` | `initialiseCatConnections()` | Current app A/B passed against a local dummy rig; `cat-rigctl-constructor-ab.txt`. |
| `bridge_cat_telemetry_ab.py` | `initialiseCatTelemetry()` | Current constructor app A/B passed after dependency repair; `cat-telemetry-constructor-current-ab.txt`. |
| `bridge_core_timers_init_ab.py` | `initialiseCoreTimers()` | application A/B checked |
| `bridge_decode_cluster_connections_ab.py` | `initialiseDecodeClusterConnections()` | not individually revalidated in this audit |
| `bridge_decode_model_init_ab.py` | `initialiseDecodeModels()` | application A/B checked |
| `bridge_decode_model_init_legacy_pair_ab.py` | `initialiseDecodeModels()` | Current two-session legacy virtual-audio A/B passed after dependency repair; `decode-model-legacy-pair-constructor-current-ab.txt`. |
| `bridge_decolog_init_ab.py` | `initialiseDecoLogLink()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_decolog_local_ab.py` | `initialiseDecoLogLink()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_decolog_tune_ab.py` | `initialiseDecoLogLink()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_diagnostic_connections_ab.py` | `initialiseDiagnosticConnections()` | Current script: before/after app A/B passed; `diagnostic-connections-constructor-ab.txt`. |
| `bridge_dxcluster_init_ab.py` | `initialiseDxClusterNotifications()` | not individually revalidated in this audit |
| `bridge_dxcluster_init_local_ab.py` | `initialiseDxClusterNotifications()` | not individually revalidated in this audit |
| `bridge_legacy_filter_connections_ab.py` | `initialiseDecodeClusterConnections(), initialiseLegacyCqFilterConnections(), initialiseLegacyMyCallFilterConnection(), initialiseReadyProfileConnections()` | static ordering checked |
| `bridge_mac_legacy_start_ab.py` | `initialiseMacLegacyStartup()` | not individually revalidated in this audit |
| `bridge_mac_legacy_start_stall_ab.py` | `initialiseMacLegacyStartup()` | Current before constructor fixture passed a macOS syntax-only check; prior functional evidence is described in the main report. |
| `bridge_mac_standalone_start_ab.py` | `initialiseMacLegacyStartup()` | not individually revalidated in this audit |
| `bridge_map_init_ab.py` | `initialiseMapIntelligence()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_map_init_abort_ab.py` | `initialiseMapIntelligence()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_map_init_moon_ab.py` | `initialiseMapIntelligence()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_psk_cloudlog_adif_ab.py` | `initialisePskAndCloudlog()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_psk_cloudlog_http_ab.py` | `initialisePskAndCloudlog()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_psk_cloudlog_init_ab.py` | `initialisePskAndCloudlog()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_qrz_wspr_init_ab.py` | `initialiseQrzAndWspr()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_ready_profile_apply_ab.py` | `initialiseReadyProfileConnections()` | not individually revalidated in this audit |
| `bridge_ready_profile_init_ab.py` | `initialiseReadyProfileConnections()` | not individually revalidated in this audit |
| `bridge_remote_web_commands_ab.py` | `initialiseRemoteWebConsole()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_remote_web_http_ab.py` | `initialiseRemoteWebConsole()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_remote_web_init_ab.py` | `initialiseRemoteWebConsole()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_remote_web_ws_auth_ab.py` | `initialiseRemoteWebConsole()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_rtl_init_ab.py` | `initialiseRtlSdr()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_rtl_init_retune_ab.py` | `initialiseRtlSdr()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_rtl_init_started_ab.py` | `initialiseRtlSdr()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_satellite_init_ab.py` | `initialiseSatelliteTracking()` | not individually revalidated in this audit |
| `bridge_satellite_init_apply_ab.py` | `initialiseSatelliteTracking()` | not individually revalidated in this audit |
| `bridge_tci_complete_ab.py` | `initialiseCatTelemetry()` | Current constructor app A/B passed after dependency repair; `tci-constructor-current-ab.txt`. |
| `bridge_tci_signals_ab.py` | `initialiseCatTelemetry()` | Current constructor app A/B passed after dependency repair; `tci-constructor-current-ab.txt`. |
| `bridge_timesync_init_ab.py` | `initialiseTimeSync()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_timesync_init_rearm_ab.py` | `initialiseTimeSync()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_tx_audio_startup_ab.py` | `initialiseTxAudioOutput(), initialiseUiStartupSettings()` | Static ordering checked. Prior application A/B output in `tx_audio_startup/before.json` and `after.json` records FT8 CQ decoded via virtual audio; `modulator_idle_callback_seen` was false in both runs. |
| `bridge_usstate_dxcc_refresh_ab.py` | `initialiseUsStateAndWsprNotifications()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_usstate_wspr_init_ab.py` | `initialiseUsStateAndWsprNotifications()` | Current before variant: compiler syntax check passed; `constructor-script-fixtures-syntax.txt`. Earlier functional A/B evidence remains in the main report; this check did not rerun it. |
| `bridge_wav_init_ab.py` | `initialiseWavManager()` | application A/B checked |
| `bridge_wav_init_timed_ab.py` | `initialiseWavManager()` | not individually revalidated in this audit |
