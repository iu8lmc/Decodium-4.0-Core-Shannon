# Bridge module runtime-reference navigation

Literal-reference index only. A reference does not prove execution, A/B or complete coverage; absence does not prove missing coverage because tests may exercise the module indirectly or through generated fixtures. This is not a count of remaining tests.

| Module | Candidate application scripts | Main report mentions module |
| --- | --- | --- |
| `DecodiumBridge.cpp` | `bridge_adif_full_export_ab.py`, `bridge_adif_import_wrapper_ab.py`, `bridge_amplifier_port_runtime_ab.py`, `bridge_amplifierconfig_runtime_lab.py`, `bridge_application_modes_runtime_ab.py`, `bridge_async_logs_runtime_ab.py`, `bridge_audio_adapters_runtime_ab.py`, `bridge_audio_buffers_runtime_ab.py`, `bridge_audio_channel_helpers_runtime_ab.py`, `bridge_audio_device_identity_runtime_ab.py`, `bridge_audio_device_recovery_runtime_lab.py`, `bridge_audio_device_resolve_runtime_ab.py`, `bridge_audio_level_math_runtime_ab.py`, `bridge_audiocapture_sstv_ab.py`, `bridge_audiocapture_start_ab.py`, `bridge_audiofrequencies_runtime_lab.py`, `bridge_audioidentity_runtime_lab.py`, `bridge_audiorefresh_runtime_lab.py`, `bridge_audioruntimechange_runtime_lab.py`, `bridge_audiosink_gateway_ab.py`, `bridge_audiosink_recording_ab.py`, `bridge_audiosink_ssb_ab.py`, `bridge_audiosink_sstv_ab.py`, `bridge_audiosink_taps_ab.py`, `bridge_auto_sequence_runtime_ab.py`, `bridge_autocallcomplete_runtime_lab.py`, `bridge_autocallselect_runtime_lab.py`, `bridge_autocqhistory_runtime_lab.py`, `bridge_autocqlock_runtime_lab.py`, `bridge_autocqrepeat_runtime_lab.py`, `bridge_autolog_helpers_runtime_ab.py`, `bridge_cabrillo_app_ab.py`, `bridge_cat_backend_helpers_runtime_ab.py`, `bridge_cat_dispatch_runtime_ab.py`, `bridge_cat_dispatch_ssb_runtime_ab.py`, `bridge_cat_migration_startup_runtime_ab.py`, `bridge_cat_profiles_runtime_ab.py`, `bridge_cat_ptt_state_runtime_ab.py`, `bridge_catbackendswitch_runtime_lab.py`, `bridge_catqsy_runtime_lab.py`, `bridge_catrecovery_runtime_lab.py`, `bridge_catrigmode_runtime_lab.py`, `bridge_catsplitsync_runtime_lab.py`, `bridge_cleartx_active_ab.py`, `bridge_cleartxmessages_runtime_lab.py`, `bridge_complete_tx_guards_ab.py`, `bridge_complete_tx_hold_timer_ab.py`, `bridge_complete_tx_loopback_ab.py`, `bridge_complete_tx_pcm_ab.py`, `bridge_complete_tx_runtime_ab.py`, `bridge_complete_tx_sink_ab.py`, `bridge_complete_tx_special_ab.py`, `bridge_complete_tx_sstv_legacy_ab.py`, `bridge_complete_tx_worker_error_ab.py`, `bridge_complete_tx_worker_thread_ab.py`, `bridge_constructor_legacy_runtime_ab.py`, `bridge_constructor_runtime_ab.py`, `bridge_correctedtime_runtime_lab.py`, `bridge_courtesy_runtime_lab.py`, `bridge_courtesy_success_runtime_ab.py`, `bridge_decode_adapters_runtime_ab.py`, `bridge_decode_colors_placement_ab.py`, `bridge_decode_history_app_ab.py`, `bridge_decode_keys_runtime_ab.py`, `bridge_decode_visibility_app_ab.py`, `bridge_decodeflow_queue_runtime_ab.py`, `bridge_decodetxwait_runtime_lab.py`, `bridge_decolinklogin_runtime_lab.py`, `bridge_decoportaudioinput_runtime_lab.py`, `bridge_decoportcredentials_runtime_lab.py`, `bridge_decoportdriver_runtime_lab.py`, `bridge_decoportgateway_nokey_runtime_lab.py`, `bridge_decoportgateway_runtime_lab.py`, `bridge_decoporthubrefresh_runtime_lab.py`, `bridge_decoporthubrows_runtime_lab.py`, `bridge_decoportmonitor_runtime_lab.py`, `bridge_decoportobject_runtime_lab.py`, `bridge_decoportptt_runtime_lab.py`, `bridge_decoporttxaudio_runtime_lab.py`, `bridge_decoporttxframes_runtime_lab.py`, `bridge_decoportuse_runtime_lab.py`, `bridge_deferredseq_runtime_lab.py`, `bridge_destructor_active_decode_ab.py`, `bridge_destructor_runtime_ab.py`, `bridge_diagnostics_runtime_ab.py`, `bridge_directaudiotx_runtime_lab.py`, `bridge_directory_dialog_ab.py`, `bridge_display_frequency_runtime_ab.py`, `bridge_dxcall_runtime_lab.py`, `bridge_dxcluster_commands_runtime_lab.py`, `bridge_environment_rx_policy_runtime_ab.py`, `bridge_feeddecoder_fst4_ab.py`, `bridge_feeddecoder_fst4_reference_ab.py`, `bridge_feeddecoder_ft2_ab.py`, `bridge_feeddecoder_ft4_ab.py`, `bridge_feeddecoder_ft8_ab.py`, `bridge_feeddecoder_jt4_ab.py`, `bridge_feeddecoder_jt65_ab.py`, `bridge_feeddecoder_jt65_reference_ab.py`, `bridge_feeddecoder_jt9_reference_ab.py`, `bridge_feeddecoder_msk144_ab.py`, `bridge_feeddecoder_q65_ab.py`, `bridge_feeddecoder_wspr_reference_ab.py`, `bridge_file_dialog_runtime_ab.py`, `bridge_file_paths_runtime_ab.py`, `bridge_fileio_runtime_ab.py`, `bridge_frequency_calibration_app_ab.py`, `bridge_frequencyupdate_runtime_lab.py`, `bridge_fst4_clean_pcm_integrity_probe.py`, `bridge_fst4_clean_request_parameters_probe.py`, `bridge_fst4_clean_wide_probe_ab.py`, `bridge_ft2_access_success_ab.py`, `bridge_ft2_async_ready_audio_ab.py`, `bridge_ft2_async_ready_guards_ab.py`, `bridge_ft2_async_timer_adaptive_ab.py`, `bridge_ft2_async_timer_audio_ab.py`, `bridge_ft2_async_timer_guards_ab.py`, `bridge_ft2_async_timer_ranges_ab.py`, `bridge_ft2_async_timer_sequential_ab.py`, `bridge_ft2_sync_ready_audio_ab.py`, `bridge_ft2_sync_ready_cache_ab.py`, `bridge_ft2audiogates_runtime_lab.py`, `bridge_ft2smtpcredentials_runtime_lab.py`, `bridge_ft2smtpsend_runtime_lab.py`, `bridge_ft4_legacy_revision_diagnostic_ab.py`, `bridge_ft4early_runtime_lab.py`, `bridge_ft8_ready_audio_ab.py`, `bridge_ft8_ready_cq_filter_ab.py`, `bridge_ft8_ready_dedup_ab.py`, `bridge_ft8_ready_fst4_reference_ab.py`, `bridge_ft8_ready_ft2_audio_ab.py`, `bridge_ft8_ready_ft4_audio_ab.py`, `bridge_ft8_ready_guards_ab.py`, `bridge_ft8_ready_jt4_audio_ab.py`, `bridge_ft8_ready_jt65_reference_ab.py`, `bridge_ft8_ready_jt9_reference_ab.py`, `bridge_ft8_ready_legacy_immediate_ab.py`, `bridge_ft8_ready_legacy_mirror_ab.py`, `bridge_ft8_ready_legacy_publish_ab.py`, `bridge_ft8_ready_msk144_audio_ab.py`, `bridge_ft8_ready_mycall_filter_ab.py`, `bridge_ft8_ready_q65_audio_ab.py`, `bridge_ft8bridgegate_runtime_lab.py`, `bridge_ft8early_decode_ab.py`, `bridge_ft8early_gates_ab.py`, `bridge_ft8queue_decode_ab.py`, `bridge_ft8queue_params_ab.py`, `bridge_ft_scheduler_runtime_ab.py`, `bridge_ft_thread_budget_helpers_runtime_ab.py`, `bridge_ftx_settings_placement_ab.py`, `bridge_halt_active_ab.py`, `bridge_halt_runtime_lab.py`, `bridge_houndtxprepare_runtime_lab.py`, `bridge_jt65_deep_probe.py`, `bridge_jt65_pcm_integrity_probe.py`, `bridge_jt65_request_parameters_probe.py`, `bridge_jt65_tolerance_probe.py`, `bridge_jtdecodefallback_runtime_lab.py`, `bridge_layout_runtime_ab.py`, `bridge_legacy_bridge_audio_guard_ab.py`, `bridge_legacy_bridge_audio_loopback_ab.py`, `bridge_legacy_bridge_audio_stop_ab.py`, `bridge_legacy_palette_backend_runtime_ab.py`, `bridge_legacy_preflight_abort_ab.py`, `bridge_legacy_preflight_loopback_ab.py`, `bridge_legacy_presentation_helpers_runtime_ab.py`, `bridge_legacy_rig_policy_runtime_ab.py`, `bridge_legacy_sync_blacklist_ab.py`, `bridge_legacy_sync_cq_filter_ab.py`, `bridge_legacy_sync_dedup_ab.py`, `bridge_legacy_sync_guards_ab.py`, `bridge_legacy_sync_queued_ab.py`, `bridge_legacy_sync_runtime_ab.py`, `bridge_legacyaudiopolicy_runtime_lab.py`, `bridge_legacyaudiostart_runtime_lab.py`, `bridge_legacyaugment_runtime_ab.py`, `bridge_legacyhash_app_ab.py`, `bridge_legacyinit_runtime_lab.py`, `bridge_legacyptttransition_runtime_lab.py`, `bridge_legacystatesync_runtime_lab.py`, `bridge_load_settings_audio_bounds_ab.py`, `bridge_log_prompt_cluster_offline_ab.py`, `bridge_log_prompt_dialog_ab.py`, `bridge_log_prompt_dialog_add_ab.py`, `bridge_log_prompt_dialog_satellite_ab.py`, `bridge_log_qso_legacy_delegate_ab.py`, `bridge_log_qso_legacy_real_ab.py`, `bridge_log_qso_runtime_ab.py`, `bridge_log_routing_runtime_ab.py`, `bridge_logpreview_snapshot_runtime_ab.py`, `bridge_lotw_cache_app_ab.py`, `bridge_lotw_cache_runtime_ab.py`, `bridge_lotw_download_runtime_ab.py`, `bridge_lotw_empty_download_runtime_ab.py`, `bridge_lotw_existing_runtime_ab.py`, `bridge_lotw_migration_runtime_ab.py`, `bridge_lotw_network_empty_cache_ab.py`, `bridge_lotw_network_error_runtime_ab.py`, `bridge_lotw_newest_runtime_ab.py`, `bridge_mamcaller_runtime_lab.py`, `bridge_mamprune_expiry_runtime_ab.py`, `bridge_manualhold_runtime_lab.py`, `bridge_mode_period_ab.py`, `bridge_modecleanup_runtime_lab.py`, `bridge_modeswitch_decode_ab.py`, `bridge_modeswitch_runtime_ab.py`, `bridge_modulator_cw_pending_ab.py`, `bridge_modulator_finish_loopback_ab.py`, `bridge_modulator_idle_ft2_ab.py`, `bridge_modulator_idle_legacy_ab.py`, `bridge_modulator_idle_pcm_ab.py`, `bridge_modulator_idle_runtime_ab.py`, `bridge_multianswer_mode_ab.py`, `bridge_multianswer_mode_slot_ab.py`, `bridge_networkprefs_restart_ab.py`, `bridge_ntp_manual_runtime_lab.py`, `bridge_offline_runtime_ab.py`, `bridge_panadapter_gpu_init_runtime_ab.py`, `bridge_partner_guard_ft8_loopback_ab.py`, `bridge_partner_guard_old_roger_ab.py`, `bridge_partner_guard_reject_ab.py`, `bridge_partner_guard_repair_ab.py`, `bridge_partner_guard_retry_ab.py`, `bridge_partner_guard_roger_ab.py`, `bridge_period_timer_ft8_audio_ab.py`, `bridge_period_timer_jt65_audio_ab.py`, `bridge_period_timer_runtime_ab.py`, `bridge_period_timer_watchdog_ab.py`, `bridge_periodic_tx_cancel_ab.py`, `bridge_periodic_tx_dispatch_ab.py`, `bridge_periodic_tx_ft8_loopback_ab.py`, `bridge_periodic_tx_guards_ab.py`, `bridge_periodic_tx_late_ab.py`, `bridge_periodic_tx_mam_ab.py`, `bridge_periodic_tx_parity_ab.py`, `bridge_periodic_tx_retry_ab.py`, `bridge_periodtimerarm_runtime_lab.py`, `bridge_precomputed_wave_store_runtime_ab.py`, `bridge_processusage_runtime_lab.py`, `bridge_psk_local_response_ab.py`, `bridge_psk_replay_app_ab.py`, `bridge_q65split_runtime_lab.py`, `bridge_qml_load_timestamp_ab.py`, `bridge_qso_journal_runtime_ab.py`, `bridge_qsofinish_runtime_lab.py`, `bridge_qsoreset_runtime_lab.py`, `bridge_qsotransition_runtime_lab.py`, `bridge_qsycommand_runtime_lab.py`, `bridge_reload_settings_runtime_ab.py`, `bridge_remote_activity_ws_ab.py`, `bridge_remotefrequency_runtime_lab.py`, `bridge_remotelinkobjects_runtime_lab.py`, `bridge_remotelinkselection_runtime_lab.py`, `bridge_remoteradiostate_runtime_lab.py`, `bridge_remoterxaudio_runtime_lab.py`, `bridge_rigdiscovery_runtime_lab.py`, `bridge_rigtelemetry_runtime_lab.py`, `bridge_rtlsdrcapture_runtime_lab.py`, `bridge_rtlsdrconfig_runtime_lab.py`, `bridge_rtlsdrdiscovery_runtime_lab.py`, `bridge_rtlsdrstatus_runtime_lab.py`, `bridge_rttyrigmode_runtime_lab.py`, `bridge_rx_monitoring_loopback_ab.py`, `bridge_rxaudiotx_runtime_lab.py`, `bridge_rxcontrol_audio_ab.py`, `bridge_save_settings_restart_ab.py`, `bridge_secondary_runtime_ab.py`, `bridge_secondinstance_runtime_lab.py`, `bridge_selfcheck_runtime_lab.py`, `bridge_sendtx_ft8_deferred_ab.py`, `bridge_sendtx_ft8_guards_ab.py`, `bridge_sendtx_ft8_loopback_ab.py`, `bridge_sendtx_jtty_ab.py`, `bridge_sendtx_jtty_result_ab.py`, `bridge_sendtx_legacy_delegate_ab.py`, `bridge_sendtx_repair_partner_ab.py`, `bridge_sendtx_stale_partner_ab.py`, `bridge_services_runtime_ab.py`, `bridge_settings_helpers_runtime_ab.py`, `bridge_shared_runtime_ab.py`, `bridge_sharing_accessors_runtime_ab.py`, `bridge_sharingconfig_runtime_lab.py`, `bridge_shutdown_active_ab.py`, `bridge_shutdown_runtime_ab.py`, `bridge_smartft2_cancel_timer_ab.py`, `bridge_smartft2_hold_timer_ab.py`, `bridge_smartft2_mode_timer_ab.py`, `bridge_smartft2scheduler_runtime_lab.py`, `bridge_specialop_wrappers_ab.py`, `bridge_spectrum_timer_cpu_ab.py`, `bridge_spectrum_timer_fallback_frame_ab.py`, `bridge_spectrum_timer_gpu_handoff_ab.py`, `bridge_spectrum_timer_hidden_ab.py`, `bridge_spectrum_timer_visible_fallback_ab.py`, `bridge_ssb_unit_runtime_ab.py`, `bridge_sstvqso_runtime_lab.py`, `bridge_start_tx_guards_ab.py`, `bridge_start_tx_loopback_ab.py`, `bridge_start_tx_preflight_notices_ab.py`, `bridge_start_tx_selection_runtime_ab.py`, `bridge_startup_legacy_decode_order_ab.py`, `bridge_startup_legacy_decode_stall_ab.py`, `bridge_startupmode_runtime_lab.py`, `bridge_startupservices_runtime_lab.py`, `bridge_state_runtime_ab.py`, `bridge_stationfreq_restart_ab.py`, `bridge_stop_tx_routes_ab.py`, `bridge_stop_tx_runtime_ab.py`, `bridge_swr_admission_app_ab.py`, `bridge_targetcall_decode_runtime_ab.py`, `bridge_targetcall_timeout_runtime_ab.py`, `bridge_timesyncdispatch_decode_ab.py`, `bridge_timesyncdispatch_runtime_ab.py`, `bridge_tune_guards_ab.py`, `bridge_tune_rtl_toggle_ab.py`, `bridge_tunewatchdog_runtime_lab.py`, `bridge_tx_adapters_loopback_ab.py`, `bridge_tx_adapters_runtime_ab.py`, `bridge_tx_enable_runtime_ab.py`, `bridge_tx_output_device_loopback_ab.py`, `bridge_tx_output_fallback_ab.py`, `bridge_tx_output_resolve_runtime_ab.py`, `bridge_tx_pcm_offset_loopback_ab.py`, `bridge_tx_policy_adapters_runtime_ab.py`, `bridge_tx_slot_helpers_runtime_ab.py`, `bridge_tx_wave_helpers_runtime_ab.py`, `bridge_txaudiotelemetry_explicit_lab.py`, `bridge_txboundary_runtime_lab.py`, `bridge_txdispatchguard_notifications_ab.py`, `bridge_txdispatchguard_runtime_lab.py`, `bridge_txfinish_runtime_lab.py`, `bridge_txrecording_runtime_lab.py`, `bridge_udpinit_runtime_lab.py`, `bridge_udpinit_wire_runtime_lab.py`, `bridge_udprestart_runtime_lab.py`, `bridge_ui_actions_runtime_ab.py`, `bridge_ui_audio_state_runtime_ab.py`, `bridge_ui_startup_settings_runtime_ab.py`, `bridge_uistall_monitor_runtime_ab.py`, `bridge_verify_otp_runtime_ab.py`, `bridge_vhfuhfsetting_runtime_lab.py`, `bridge_visual_tx_time_runtime_ab.py`, `bridge_worldmap_call_helpers_runtime_ab.py`, `bridge_worldmapreplay_app_ab.py`, `bridge_worldmapreplay_synthetic_app_ab.py`, `bridge_worldmapreplay_wav_isolated_ab.py`, `bridge_worldmapreplay_wav_unfiltered_ab.py` | yes |
| `DecodiumBridgeAdifAppend.cpp` | No literal match | no literal match |
| `DecodiumBridgeAdifEditing.cpp` | `bridge_adif_full_export_ab.py` | yes |
| `DecodiumBridgeAdifImport.cpp` | `bridge_adif_import_wrapper_ab.py` | yes |
| `DecodiumBridgeAdifRecord.cpp` | No literal match | yes |
| `DecodiumBridgeAdifTcp.cpp` | No literal match | no literal match |
| `DecodiumBridgeAdifUdp.cpp` | No literal match | no literal match |
| `DecodiumBridgeAdvancedDecode.cpp` | No literal match | yes |
| `DecodiumBridgeAlcCalibration.cpp` | No literal match | no literal match |
| `DecodiumBridgeAmplifierConfig.cpp` | `bridge_amplifier_port_runtime_ab.py`, `bridge_amplifierconfig_runtime_lab.py` | yes |
| `DecodiumBridgeAppInfo.cpp` | No literal match | no literal match |
| `DecodiumBridgeApplicationModes.cpp` | `bridge_application_modes_runtime_ab.py` | no literal match |
| `DecodiumBridgeAsyncHistory.cpp` | No literal match | yes |
| `DecodiumBridgeAsyncLogs.cpp` | `bridge_async_logs_runtime_ab.py` | no literal match |
| `DecodiumBridgeAudioAdapters.cpp` | `bridge_audio_adapters_runtime_ab.py` | no literal match |
| `DecodiumBridgeAudioBuffers.cpp` | `bridge_audio_buffers_runtime_ab.py` | yes |
| `DecodiumBridgeAudioCaptureStart.cpp` | `bridge_audiocapture_sstv_ab.py`, `bridge_audiocapture_start_ab.py` | yes |
| `DecodiumBridgeAudioCaptureStop.cpp` | No literal match | yes |
| `DecodiumBridgeAudioChannelHelpers.cpp` | `bridge_audio_channel_helpers_runtime_ab.py` | no literal match |
| `DecodiumBridgeAudioDeviceCacheAccess.cpp` | No literal match | no literal match |
| `DecodiumBridgeAudioDeviceCacheRefresh.cpp` | No literal match | no literal match |
| `DecodiumBridgeAudioDeviceIdentity.cpp` | `bridge_audio_device_identity_runtime_ab.py` | no literal match |
| `DecodiumBridgeAudioDeviceInit.cpp` | `bridge_audio_device_init_ab.py`, `bridge_audio_device_init_loopback_ab.py` | no literal match |
| `DecodiumBridgeAudioDeviceResolve.cpp` | `bridge_audio_device_resolve_runtime_ab.py` | no literal match |
| `DecodiumBridgeAudioFrequencies.cpp` | `bridge_audiofrequencies_runtime_lab.py` | yes |
| `DecodiumBridgeAudioHealth.cpp` | No literal match | yes |
| `DecodiumBridgeAudioIdentity.cpp` | `bridge_audioidentity_runtime_lab.py` | yes |
| `DecodiumBridgeAudioInputSelection.cpp` | No literal match | no literal match |
| `DecodiumBridgeAudioLevelMath.cpp` | `bridge_audio_level_math_runtime_ab.py` | no literal match |
| `DecodiumBridgeAudioLevels.cpp` | No literal match | yes |
| `DecodiumBridgeAudioRecovery.cpp` | `bridge_audio_device_recovery_runtime_lab.py` | yes |
| `DecodiumBridgeAudioRefresh.cpp` | `bridge_audiorefresh_runtime_lab.py` | yes |
| `DecodiumBridgeAudioRuntimeChange.cpp` | `bridge_audioruntimechange_runtime_lab.py` | yes |
| `DecodiumBridgeAudioSelection.cpp` | No literal match | no literal match |
| `DecodiumBridgeAudioSink.cpp` | `bridge_audiosink_gateway_ab.py`, `bridge_audiosink_recording_ab.py`, `bridge_audiosink_ssb_ab.py`, `bridge_audiosink_sstv_ab.py`, `bridge_audiosink_taps_ab.py` | yes |
| `DecodiumBridgeAudioSinkRetire.cpp` | No literal match | no literal match |
| `DecodiumBridgeAutoCall.cpp` | `bridge_autocallcomplete_runtime_lab.py` | yes |
| `DecodiumBridgeAutoCallSelect.cpp` | `bridge_autocallselect_runtime_lab.py` | yes |
| `DecodiumBridgeAutoCqCadence.cpp` | No literal match | yes |
| `DecodiumBridgeAutoCqDedupe.cpp` | No literal match | yes |
| `DecodiumBridgeAutoCqHistory.cpp` | `bridge_autocqhistory_runtime_lab.py` | yes |
| `DecodiumBridgeAutoCqLockQueue.cpp` | `bridge_autocqlock_runtime_lab.py` | yes |
| `DecodiumBridgeAutoCqRepeat.cpp` | `bridge_autocqrepeat_runtime_lab.py` | yes |
| `DecodiumBridgeAutoLogSnapshot.cpp` | No literal match | yes |
| `DecodiumBridgeAutoRxLevel.cpp` | No literal match | yes |
| `DecodiumBridgeAutoSeqAfterTx.cpp` | `bridge_deferredseq_runtime_lab.py` | yes |
| `DecodiumBridgeAutoSequence.cpp` | `bridge_auto_sequence_runtime_ab.py` | no literal match |
| `DecodiumBridgeAutologHelpers.cpp` | `bridge_autolog_helpers_runtime_ab.py` | no literal match |
| `DecodiumBridgeCabrillo.cpp` | `bridge_cabrillo_app_ab.py` | yes |
| `DecodiumBridgeCallerQueue.cpp` | No literal match | no literal match |
| `DecodiumBridgeCallerScoring.cpp` | No literal match | no literal match |
| `DecodiumBridgeCallsignInit.cpp` | `bridge_callsign_confirmed_adif_ab.py`, `bridge_callsign_init_ab.py`, `bridge_callsign_url_ab.py` | no literal match |
| `DecodiumBridgeCatBackendHelpers.cpp` | `bridge_cat_backend_helpers_runtime_ab.py` | no literal match |
| `DecodiumBridgeCatBackendSwitch.cpp` | `bridge_catbackendswitch_runtime_lab.py` | yes |
| `DecodiumBridgeCatConnections.cpp` | `bridge_cat_connections_ab.py`, `bridge_cat_connections_confirm_ab.py`, `bridge_cat_connections_connect_ab.py`, `bridge_cat_connections_frequency_ab.py`, `bridge_cat_connections_mode_ab.py`, `bridge_cat_connections_ptt_ab.py`, `bridge_cat_connections_rigctl_ab.py` | no literal match |
| `DecodiumBridgeCatDispatch.cpp` | `bridge_cat_dispatch_runtime_ab.py`, `bridge_cat_dispatch_ssb_runtime_ab.py`, `bridge_cat_ptt_state_runtime_ab.py` | no literal match |
| `DecodiumBridgeCatMigrationStartup.cpp` | `bridge_cat_migration_startup_runtime_ab.py` | no literal match |
| `DecodiumBridgeCatModes.cpp` | No literal match | no literal match |
| `DecodiumBridgeCatProfiles.cpp` | `bridge_cat_profiles_runtime_ab.py` | no literal match |
| `DecodiumBridgeCatQsy.cpp` | `bridge_catqsy_runtime_lab.py` | yes |
| `DecodiumBridgeCatRecovery.cpp` | `bridge_catrecovery_runtime_lab.py` | yes |
| `DecodiumBridgeCatRigMode.cpp` | `bridge_catrigmode_runtime_lab.py` | yes |
| `DecodiumBridgeCatSplitMath.cpp` | No literal match | no literal match |
| `DecodiumBridgeCatSplitSync.cpp` | `bridge_catsplitsync_runtime_lab.py` | yes |
| `DecodiumBridgeCatTelemetry.cpp` | `bridge_cat_telemetry_ab.py`, `bridge_tci_complete_ab.py`, `bridge_tci_signals_ab.py` | no literal match |
| `DecodiumBridgeCompleteTxPlayback.cpp` | `bridge_complete_tx_guards_ab.py`, `bridge_complete_tx_hold_timer_ab.py`, `bridge_complete_tx_loopback_ab.py`, `bridge_complete_tx_pcm_ab.py`, `bridge_complete_tx_runtime_ab.py`, `bridge_complete_tx_sink_ab.py`, `bridge_complete_tx_special_ab.py`, `bridge_complete_tx_sstv_legacy_ab.py`, `bridge_complete_tx_worker_error_ab.py`, `bridge_complete_tx_worker_thread_ab.py`, `bridge_tci_complete_ab.py` | no literal match |
| `DecodiumBridgeConstructor.cpp` | `bridge_audio_device_init_ab.py`, `bridge_audio_device_init_loopback_ab.py`, `bridge_callsign_confirmed_adif_ab.py`, `bridge_callsign_init_ab.py`, `bridge_callsign_url_ab.py`, `bridge_cat_connections_ab.py`, `bridge_cat_connections_confirm_ab.py`, `bridge_cat_connections_connect_ab.py`, `bridge_cat_connections_frequency_ab.py`, `bridge_cat_connections_mode_ab.py`, `bridge_cat_connections_ptt_ab.py`, `bridge_cat_connections_rigctl_ab.py`, `bridge_cat_telemetry_ab.py`, `bridge_constructor_legacy_runtime_ab.py`, `bridge_constructor_runtime_ab.py`, `bridge_core_timers_init_ab.py`, `bridge_decode_cluster_connections_ab.py`, `bridge_decode_model_init_ab.py`, `bridge_decode_model_init_legacy_pair_ab.py`, `bridge_decolog_init_ab.py`, `bridge_decolog_local_ab.py`, `bridge_decolog_tune_ab.py`, `bridge_diagnostic_connections_ab.py`, `bridge_dxcluster_init_ab.py`, `bridge_dxcluster_init_local_ab.py`, `bridge_layout_runtime_ab.py`, `bridge_legacy_filter_connections_runtime_ab.py`, `bridge_mac_legacy_start_ab.py`, `bridge_mac_legacy_start_stall_ab.py`, `bridge_mac_standalone_start_ab.py`, `bridge_map_init_ab.py`, `bridge_map_init_abort_ab.py`, `bridge_map_init_moon_ab.py`, `bridge_psk_cloudlog_adif_ab.py`, `bridge_psk_cloudlog_http_ab.py`, `bridge_psk_cloudlog_init_ab.py`, `bridge_qrz_wspr_init_ab.py`, `bridge_ready_profile_apply_ab.py`, `bridge_ready_profile_init_ab.py`, `bridge_remote_web_commands_ab.py`, `bridge_remote_web_http_ab.py`, `bridge_remote_web_init_ab.py`, `bridge_remote_web_ws_auth_ab.py`, `bridge_rtl_init_ab.py`, `bridge_rtl_init_retune_ab.py`, `bridge_rtl_init_started_ab.py`, `bridge_satellite_init_ab.py`, `bridge_satellite_init_apply_ab.py`, `bridge_tci_complete_ab.py`, `bridge_tci_signals_ab.py`, `bridge_timesync_init_ab.py`, `bridge_timesync_init_rearm_ab.py`, `bridge_tx_audio_startup_loopback_ab.py`, `bridge_tx_audio_startup_runtime_ab.py`, `bridge_usstate_dxcc_refresh_ab.py`, `bridge_usstate_wspr_init_ab.py`, `bridge_wav_init_ab.py`, `bridge_wav_init_timed_ab.py` | no literal match |
| `DecodiumBridgeCoreTimers.cpp` | `bridge_core_timers_init_ab.py` | no literal match |
| `DecodiumBridgeCorrectedTime.cpp` | `bridge_correctedtime_runtime_lab.py` | yes |
| `DecodiumBridgeCourtesySignoff.cpp` | `bridge_courtesy_runtime_lab.py`, `bridge_courtesy_success_runtime_ab.py` | yes |
| `DecodiumBridgeCpuPressure.cpp` | No literal match | yes |
| `DecodiumBridgeCpuProfile.cpp` | No literal match | yes |
| `DecodiumBridgeCqReplyWindow.cpp` | No literal match | no literal match |
| `DecodiumBridgeCwMonitorSettings.cpp` | No literal match | no literal match |
| `DecodiumBridgeCwPorts.cpp` | No literal match | no literal match |
| `DecodiumBridgeCwRemote.cpp` | No literal match | no literal match |
| `DecodiumBridgeCwRig.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecoLogInit.cpp` | `bridge_decolog_init_ab.py`, `bridge_decolog_local_ab.py`, `bridge_decolog_tune_ab.py` | no literal match |
| `DecodiumBridgeDecoPortCredentials.cpp` | `bridge_decoportcredentials_runtime_lab.py` | yes |
| `DecodiumBridgeDecoPortDriver.cpp` | `bridge_decoportdriver_runtime_lab.py`, `bridge_decoportgateway_nokey_runtime_lab.py`, `bridge_decoportobject_runtime_lab.py` | yes |
| `DecodiumBridgeDecoPortGateway.cpp` | `bridge_decoportgateway_nokey_runtime_lab.py`, `bridge_decoportgateway_runtime_lab.py` | yes |
| `DecodiumBridgeDecoPortHubRefresh.cpp` | `bridge_decoporthubrefresh_runtime_lab.py` | yes |
| `DecodiumBridgeDecoPortHubRows.cpp` | `bridge_decoporthubrows_runtime_lab.py` | yes |
| `DecodiumBridgeDecoPortMonitor.cpp` | `bridge_decoportmonitor_runtime_lab.py` | yes |
| `DecodiumBridgeDecoPortObject.cpp` | `bridge_decoportobject_runtime_lab.py` | yes |
| `DecodiumBridgeDecoPortPtt.cpp` | `bridge_decoportptt_runtime_lab.py` | yes |
| `DecodiumBridgeDecoPortTxAudio.cpp` | `bridge_decoporttxaudio_runtime_lab.py` | yes |
| `DecodiumBridgeDecoPortTxFrames.cpp` | `bridge_decoporttxframes_runtime_lab.py` | yes |
| `DecodiumBridgeDecoPortUseRemote.cpp` | `bridge_decoportuse_runtime_lab.py` | yes |
| `DecodiumBridgeDecodeAdapters.cpp` | `bridge_decode_adapters_runtime_ab.py` | no literal match |
| `DecodiumBridgeDecodeAlerts.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecodeBacklog.cpp` | No literal match | yes |
| `DecodiumBridgeDecodeClear.cpp` | No literal match | yes |
| `DecodiumBridgeDecodeClick.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecodeClusterConnections.cpp` | `bridge_decode_cluster_connections_ab.py` | no literal match |
| `DecodiumBridgeDecodeColors.cpp` | `bridge_decode_colors_placement_ab.py` | yes |
| `DecodiumBridgeDecodeDedup.cpp` | No literal match | yes |
| `DecodiumBridgeDecodeDepth.cpp` | No literal match | yes |
| `DecodiumBridgeDecodeEnrichment.cpp` | `bridge_enrichment_ab.py` | no literal match |
| `DecodiumBridgeDecodeFilterSettings.cpp` | `bridge_ft4_legacy_revision_diagnostic_ab.py`, `bridge_ft8_ready_legacy_immediate_ab.py`, `bridge_ft8_ready_legacy_publish_ab.py`, `bridge_legacy_sync_blacklist_ab.py`, `bridge_legacy_sync_cq_filter_ab.py` | no literal match |
| `DecodiumBridgeDecodeFlow.cpp` | `bridge_decodeflow_queue_runtime_ab.py` | yes |
| `DecodiumBridgeDecodeHistory.cpp` | `bridge_decode_history_app_ab.py` | yes |
| `DecodiumBridgeDecodeIdentity.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecodeIndicators.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecodeKeys.cpp` | `bridge_decode_keys_runtime_ab.py` | no literal match |
| `DecodiumBridgeDecodeMetrics.cpp` | No literal match | yes |
| `DecodiumBridgeDecodeMirrorKeys.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecodeModelFilter.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecodeModelInit.cpp` | `bridge_decode_model_init_ab.py`, `bridge_decode_model_init_legacy_pair_ab.py` | no literal match |
| `DecodiumBridgeDecodePersistence.cpp` | No literal match | yes |
| `DecodiumBridgeDecodeRefresh.cpp` | `bridge_usstate_dxcc_refresh_ab.py` | no literal match |
| `DecodiumBridgeDecodeRowParsers.cpp` | No literal match | yes |
| `DecodiumBridgeDecodeRxAppend.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecodeSecondary.cpp` | `bridge_secondary_runtime_ab.py` | yes |
| `DecodiumBridgeDecodeSemantic.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecodeShutdown.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecodeTiming.cpp` | No literal match | yes |
| `DecodiumBridgeDecodeTxRendezvous.cpp` | `bridge_decodetxwait_runtime_lab.py` | yes |
| `DecodiumBridgeDecodeUiNotifications.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecodeUiSchedule.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecodeUiSnapshot.cpp` | No literal match | no literal match |
| `DecodiumBridgeDecodeVisibility.cpp` | `bridge_decode_visibility_app_ab.py` | no literal match |
| `DecodiumBridgeDecodeWorkers.cpp` | No literal match | yes |
| `DecodiumBridgeDecolinkLogin.cpp` | `bridge_decolinklogin_runtime_lab.py` | yes |
| `DecodiumBridgeDeferredAutoSeq.cpp` | No literal match | yes |
| `DecodiumBridgeDestructor.cpp` | `bridge_destructor_active_decode_ab.py`, `bridge_destructor_runtime_ab.py` | no literal match |
| `DecodiumBridgeDiagnosticConnections.cpp` | `bridge_diagnostic_connections_ab.py` | no literal match |
| `DecodiumBridgeDiagnostics.cpp` | `bridge_diagnostics_runtime_ab.py` | no literal match |
| `DecodiumBridgeDirectAudioTx.cpp` | `bridge_directaudiotx_runtime_lab.py` | yes |
| `DecodiumBridgeDirectedReportGuard.cpp` | No literal match | no literal match |
| `DecodiumBridgeDisplayFrequency.cpp` | `bridge_display_frequency_runtime_ab.py` | no literal match |
| `DecodiumBridgeDxCall.cpp` | `bridge_dxcall_runtime_lab.py` | yes |
| `DecodiumBridgeDxClusterCommands.cpp` | `bridge_dxcluster_commands_runtime_lab.py` | yes |
| `DecodiumBridgeDxClusterInit.cpp` | `bridge_dxcluster_init_ab.py`, `bridge_dxcluster_init_local_ab.py` | no literal match |
| `DecodiumBridgeEnvironmentRxPolicy.cpp` | `bridge_environment_rx_policy_runtime_ab.py` | no literal match |
| `DecodiumBridgeExternalAdif.cpp` | No literal match | no literal match |
| `DecodiumBridgeExternalUrl.cpp` | No literal match | yes |
| `DecodiumBridgeFeedDecoder.cpp` | `bridge_feeddecoder_fst4_ab.py`, `bridge_feeddecoder_fst4_reference_ab.py`, `bridge_feeddecoder_ft2_ab.py`, `bridge_feeddecoder_ft4_ab.py`, `bridge_feeddecoder_ft8_ab.py`, `bridge_feeddecoder_jt4_ab.py`, `bridge_feeddecoder_jt65_ab.py`, `bridge_feeddecoder_jt65_reference_ab.py`, `bridge_feeddecoder_jt9_reference_ab.py`, `bridge_feeddecoder_msk144_ab.py`, `bridge_feeddecoder_q65_ab.py`, `bridge_feeddecoder_wspr_reference_ab.py`, `bridge_fst4_clean_pcm_integrity_probe.py`, `bridge_fst4_clean_request_parameters_probe.py`, `bridge_fst4_clean_wide_probe_ab.py`, `bridge_jt65_deep_probe.py`, `bridge_jt65_pcm_integrity_probe.py`, `bridge_jt65_request_parameters_probe.py`, `bridge_jt65_tolerance_probe.py` | yes |
| `DecodiumBridgeFileIO.cpp` | `bridge_fileio_runtime_ab.py` | no literal match |
| `DecodiumBridgeFilePaths.cpp` | `bridge_file_paths_runtime_ab.py` | no literal match |
| `DecodiumBridgeFilterControls.cpp` | No literal match | no literal match |
| `DecodiumBridgeFontSettings.cpp` | No literal match | yes |
| `DecodiumBridgeFoxVerifier.cpp` | `bridge_verify_otp_runtime_ab.py` | no literal match |
| `DecodiumBridgeFrequencyCalibration.cpp` | `bridge_frequency_calibration_app_ab.py` | yes |
| `DecodiumBridgeFrequencyUpdate.cpp` | `bridge_frequencyupdate_runtime_lab.py` | yes |
| `DecodiumBridgeFt2Access.cpp` | `bridge_ft2_access_success_ab.py` | yes |
| `DecodiumBridgeFt2AsyncReady.cpp` | `bridge_ft2_async_ready_audio_ab.py`, `bridge_ft2_async_ready_guards_ab.py` | yes |
| `DecodiumBridgeFt2AsyncTimer.cpp` | `bridge_ft2_async_timer_adaptive_ab.py`, `bridge_ft2_async_timer_audio_ab.py`, `bridge_ft2_async_timer_guards_ab.py`, `bridge_ft2_async_timer_ranges_ab.py`, `bridge_ft2_async_timer_sequential_ab.py` | yes |
| `DecodiumBridgeFt2AudioGates.cpp` | `bridge_ft2audiogates_runtime_lab.py` | yes |
| `DecodiumBridgeFt2LogBridge.cpp` | No literal match | no literal match |
| `DecodiumBridgeFt2PartnerGuards.cpp` | No literal match | no literal match |
| `DecodiumBridgeFt2SmtpCredentials.cpp` | `bridge_ft2smtpcredentials_runtime_lab.py` | yes |
| `DecodiumBridgeFt2SmtpSend.cpp` | `bridge_ft2smtpsend_runtime_lab.py` | yes |
| `DecodiumBridgeFt2SyncReady.cpp` | `bridge_ft2_sync_ready_audio_ab.py`, `bridge_ft2_sync_ready_cache_ab.py` | yes |
| `DecodiumBridgeFt4Early.cpp` | `bridge_ft4early_runtime_lab.py` | yes |
| `DecodiumBridgeFt8AutoGate.cpp` | `bridge_ft8bridgegate_runtime_lab.py` | yes |
| `DecodiumBridgeFt8Early.cpp` | `bridge_ft8early_decode_ab.py`, `bridge_ft8early_gates_ab.py` | yes |
| `DecodiumBridgeFt8Queue.cpp` | `bridge_ft8queue_decode_ab.py`, `bridge_ft8queue_params_ab.py` | yes |
| `DecodiumBridgeFt8Ready.cpp` | `bridge_feeddecoder_fst4_ab.py`, `bridge_feeddecoder_fst4_reference_ab.py`, `bridge_feeddecoder_ft2_ab.py`, `bridge_feeddecoder_ft4_ab.py`, `bridge_feeddecoder_jt4_ab.py`, `bridge_feeddecoder_jt65_ab.py`, `bridge_feeddecoder_jt65_reference_ab.py`, `bridge_feeddecoder_jt9_reference_ab.py`, `bridge_feeddecoder_msk144_ab.py`, `bridge_feeddecoder_q65_ab.py`, `bridge_feeddecoder_wspr_reference_ab.py`, `bridge_fst4_clean_pcm_integrity_probe.py`, `bridge_fst4_clean_request_parameters_probe.py`, `bridge_fst4_clean_wide_probe_ab.py`, `bridge_ft2_async_ready_audio_ab.py`, `bridge_ft2_async_timer_audio_ab.py`, `bridge_ft2_sync_ready_audio_ab.py`, `bridge_ft4_legacy_revision_diagnostic_ab.py`, `bridge_ft8_ready_audio_ab.py`, `bridge_ft8_ready_cq_filter_ab.py`, `bridge_ft8_ready_dedup_ab.py`, `bridge_ft8_ready_fst4_reference_ab.py`, `bridge_ft8_ready_ft2_audio_ab.py`, `bridge_ft8_ready_ft4_audio_ab.py`, `bridge_ft8_ready_guards_ab.py`, `bridge_ft8_ready_jt4_audio_ab.py`, `bridge_ft8_ready_jt65_reference_ab.py`, `bridge_ft8_ready_jt9_reference_ab.py`, `bridge_ft8_ready_legacy_immediate_ab.py`, `bridge_ft8_ready_legacy_mirror_ab.py`, `bridge_ft8_ready_legacy_publish_ab.py`, `bridge_ft8_ready_msk144_audio_ab.py`, `bridge_ft8_ready_mycall_filter_ab.py`, `bridge_ft8_ready_q65_audio_ab.py`, `bridge_jt65_deep_probe.py`, `bridge_jt65_pcm_integrity_probe.py`, `bridge_jt65_request_parameters_probe.py`, `bridge_jt65_tolerance_probe.py`, `bridge_period_timer_jt65_audio_ab.py` | yes |
| `DecodiumBridgeFtScheduler.cpp` | `bridge_ft_scheduler_runtime_ab.py` | no literal match |
| `DecodiumBridgeFtThreadBudget.cpp` | `bridge_ft_thread_budget_helpers_runtime_ab.py` | no literal match |
| `DecodiumBridgeFtxProfiles.cpp` | `bridge_ready_profile_apply_ab.py`, `bridge_ready_profile_init_ab.py` | yes |
| `DecodiumBridgeFtxSettings.cpp` | `bridge_ftx_settings_placement_ab.py` | yes |
| `DecodiumBridgeFullSpectrumSnapshot.cpp` | No literal match | no literal match |
| `DecodiumBridgeGenericSettings.cpp` | No literal match | no literal match |
| `DecodiumBridgeGhostFilter.cpp` | No literal match | no literal match |
| `DecodiumBridgeGraphicsStartup.cpp` | `bridge_qml_load_timestamp_ab.py` | no literal match |
| `DecodiumBridgeGrid.cpp` | No literal match | no literal match |
| `DecodiumBridgeHalt.cpp` | `bridge_halt_active_ab.py`, `bridge_halt_runtime_lab.py` | yes |
| `DecodiumBridgeHoundTxPrepare.cpp` | `bridge_houndtxprepare_runtime_lab.py` | yes |
| `DecodiumBridgeJtDecodeFallback.cpp` | `bridge_jtdecodefallback_runtime_lab.py` | yes |
| `DecodiumBridgeKeyboardAudio.cpp` | No literal match | no literal match |
| `DecodiumBridgeKeyboardLog.cpp` | No literal match | no literal match |
| `DecodiumBridgeKnownCq.cpp` | No literal match | yes |
| `DecodiumBridgeLegacyAdifMirror.cpp` | No literal match | no literal match |
| `DecodiumBridgeLegacyAllTxt.cpp` | `bridge_cabrillo_app_ab.py`, `bridge_legacyaugment_runtime_ab.py`, `bridge_legacyhash_app_ab.py` | yes |
| `DecodiumBridgeLegacyAudio.cpp` | No literal match | no literal match |
| `DecodiumBridgeLegacyAudioPolicy.cpp` | `bridge_legacyaudiopolicy_runtime_lab.py` | yes |
| `DecodiumBridgeLegacyAudioStart.cpp` | `bridge_legacyaudiostart_runtime_lab.py` | yes |
| `DecodiumBridgeLegacyAugment.cpp` | `bridge_legacyaugment_runtime_ab.py` | no literal match |
| `DecodiumBridgeLegacyBridgeAudio.cpp` | `bridge_legacy_bridge_audio_guard_ab.py`, `bridge_legacy_bridge_audio_loopback_ab.py`, `bridge_legacy_bridge_audio_stop_ab.py` | no literal match |
| `DecodiumBridgeLegacyDecodeDelta.cpp` | No literal match | yes |
| `DecodiumBridgeLegacyDialogSync.cpp` | No literal match | no literal match |
| `DecodiumBridgeLegacyFilterConnections.cpp` | `bridge_legacy_filter_connections_runtime_ab.py` | no literal match |
| `DecodiumBridgeLegacyHash.cpp` | `bridge_legacyhash_app_ab.py` | no literal match |
| `DecodiumBridgeLegacyInit.cpp` | `bridge_constructor_legacy_runtime_ab.py`, `bridge_decode_model_init_legacy_pair_ab.py`, `bridge_legacy_bridge_audio_loopback_ab.py`, `bridge_legacy_filter_connections_runtime_ab.py`, `bridge_legacy_preflight_loopback_ab.py`, `bridge_legacyinit_runtime_lab.py`, `bridge_legacystatesync_runtime_lab.py`, `bridge_log_qso_legacy_real_ab.py`, `bridge_mac_legacy_start_ab.py`, `bridge_mac_legacy_start_stall_ab.py`, `bridge_mac_standalone_start_ab.py`, `bridge_startup_legacy_decode_order_ab.py`, `bridge_startup_legacy_decode_stall_ab.py`, `bridge_stop_tx_routes_ab.py` | yes |
| `DecodiumBridgeLegacyJtCallbacks.cpp` | `bridge_feeddecoder_ft4_ab.py`, `bridge_ft8_ready_ft4_audio_ab.py` | no literal match |
| `DecodiumBridgeLegacyMirror.cpp` | No literal match | no literal match |
| `DecodiumBridgeLegacyOwnership.cpp` | No literal match | no literal match |
| `DecodiumBridgeLegacyPcmRecovery.cpp` | No literal match | no literal match |
| `DecodiumBridgeLegacyPreflight.cpp` | `bridge_legacy_preflight_abort_ab.py`, `bridge_legacy_preflight_loopback_ab.py` | no literal match |
| `DecodiumBridgeLegacyPresentationHelpers.cpp` | `bridge_legacy_palette_backend_runtime_ab.py`, `bridge_legacy_presentation_helpers_runtime_ab.py` | no literal match |
| `DecodiumBridgeLegacyPttTransition.cpp` | `bridge_cat_connections_ptt_ab.py`, `bridge_complete_tx_sstv_legacy_ab.py`, `bridge_legacyptttransition_runtime_lab.py`, `bridge_modulator_idle_legacy_ab.py` | yes |
| `DecodiumBridgeLegacyRefresh.cpp` | No literal match | no literal match |
| `DecodiumBridgeLegacyRigPolicy.cpp` | `bridge_legacy_rig_policy_runtime_ab.py` | no literal match |
| `DecodiumBridgeLegacyRxHandoff.cpp` | No literal match | no literal match |
| `DecodiumBridgeLegacySettings.cpp` | `bridge_frequency_calibration_app_ab.py`, `bridge_networkprefs_restart_ab.py`, `bridge_stationfreq_restart_ab.py`, `bridge_worldmapreplay_wav_isolated_ab.py` | yes |
| `DecodiumBridgeLegacyState.cpp` | `bridge_legacystatesync_runtime_lab.py` | yes |
| `DecodiumBridgeLegacySync.cpp` | `bridge_legacy_sync_blacklist_ab.py`, `bridge_legacy_sync_cq_filter_ab.py`, `bridge_legacy_sync_dedup_ab.py`, `bridge_legacy_sync_guards_ab.py`, `bridge_legacy_sync_queued_ab.py`, `bridge_legacy_sync_runtime_ab.py` | yes |
| `DecodiumBridgeLegacyWaterfall.cpp` | No literal match | no literal match |
| `DecodiumBridgeLinuxAudioHelpers.cpp` | No literal match | no literal match |
| `DecodiumBridgeLoadSettings.cpp` | `bridge_load_settings_audio_bounds_ab.py`, `bridge_reload_settings_runtime_ab.py` | no literal match |
| `DecodiumBridgeLocalDataState.cpp` | No literal match | yes |
| `DecodiumBridgeLocalUpdate.cpp` | `bridge_localupdate_success_lab.py` | yes |
| `DecodiumBridgeLogPreview.cpp` | `bridge_logpreview_snapshot_runtime_ab.py` | yes |
| `DecodiumBridgeLogPrompt.cpp` | No literal match | yes |
| `DecodiumBridgeLogPromptDialog.cpp` | `bridge_log_prompt_cluster_offline_ab.py`, `bridge_log_prompt_dialog_ab.py`, `bridge_log_prompt_dialog_add_ab.py`, `bridge_log_prompt_dialog_satellite_ab.py` | yes |
| `DecodiumBridgeLogPromptLifecycle.cpp` | No literal match | no literal match |
| `DecodiumBridgeLogQso.cpp` | `bridge_log_qso_legacy_delegate_ab.py`, `bridge_log_qso_legacy_real_ab.py`, `bridge_log_qso_runtime_ab.py` | no literal match |
| `DecodiumBridgeLogRouting.cpp` | `bridge_log_routing_runtime_ab.py` | no literal match |
| `DecodiumBridgeLogStallProbe.cpp` | No literal match | no literal match |
| `DecodiumBridgeLogbookProfiles.cpp` | No literal match | yes |
| `DecodiumBridgeMamCaller.cpp` | `bridge_mamcaller_runtime_lab.py` | yes |
| `DecodiumBridgeMamControls.cpp` | No literal match | yes |
| `DecodiumBridgeMamDispatch.cpp` | `bridge_periodic_tx_mam_ab.py` | yes |
| `DecodiumBridgeMamFrequencyPlan.cpp` | No literal match | yes |
| `DecodiumBridgeMamIngest.cpp` | No literal match | yes |
| `DecodiumBridgeMamLog.cpp` | No literal match | yes |
| `DecodiumBridgeMamPromote.cpp` | No literal match | yes |
| `DecodiumBridgeMamPrune.cpp` | `bridge_mamprune_expiry_runtime_ab.py` | yes |
| `DecodiumBridgeMamQueue.cpp` | No literal match | yes |
| `DecodiumBridgeMamRecovery.cpp` | No literal match | yes |
| `DecodiumBridgeMamState.cpp` | `bridge_multianswer_mode_ab.py`, `bridge_multianswer_mode_slot_ab.py` | yes |
| `DecodiumBridgeMamWave.cpp` | No literal match | yes |
| `DecodiumBridgeManualHold.cpp` | `bridge_manualhold_runtime_lab.py` | yes |
| `DecodiumBridgeMapInit.cpp` | `bridge_map_init_ab.py`, `bridge_map_init_abort_ab.py`, `bridge_map_init_moon_ab.py` | no literal match |
| `DecodiumBridgeMode.cpp` | `bridge_mode_period_ab.py`, `bridge_modecleanup_runtime_lab.py`, `bridge_modeswitch_decode_ab.py`, `bridge_modeswitch_runtime_ab.py` | yes |
| `DecodiumBridgeModulatorFinish.cpp` | `bridge_modulator_cw_pending_ab.py`, `bridge_modulator_finish_loopback_ab.py`, `bridge_modulator_idle_ft2_ab.py`, `bridge_modulator_idle_legacy_ab.py`, `bridge_modulator_idle_pcm_ab.py`, `bridge_modulator_idle_runtime_ab.py` | no literal match |
| `DecodiumBridgeNetworkPreferences.cpp` | `bridge_networkprefs_restart_ab.py` | yes |
| `DecodiumBridgeOffline.cpp` | `bridge_offline_runtime_ab.py` | yes |
| `DecodiumBridgeOnlineLogbook.cpp` | `bridge_psk_cloudlog_adif_ab.py`, `bridge_psk_cloudlog_http_ab.py`, `bridge_psk_cloudlog_init_ab.py`, `bridge_qrz_wspr_init_ab.py` | no literal match |
| `DecodiumBridgePanadapterFft.cpp` | No literal match | yes |
| `DecodiumBridgePanadapterGpuInit.cpp` | `bridge_panadapter_gpu_init_runtime_ab.py` | no literal match |
| `DecodiumBridgePanadapterRegistration.cpp` | No literal match | no literal match |
| `DecodiumBridgePartnerMemory.cpp` | No literal match | yes |
| `DecodiumBridgePeriodTimer.cpp` | `bridge_period_timer_ft8_audio_ab.py`, `bridge_period_timer_jt65_audio_ab.py`, `bridge_period_timer_runtime_ab.py`, `bridge_period_timer_watchdog_ab.py` | yes |
| `DecodiumBridgePeriodTimerArm.cpp` | `bridge_periodtimerarm_runtime_lab.py` | yes |
| `DecodiumBridgePeriodicTx.cpp` | `bridge_periodic_tx_cancel_ab.py`, `bridge_periodic_tx_dispatch_ab.py`, `bridge_periodic_tx_ft8_loopback_ab.py`, `bridge_periodic_tx_guards_ab.py`, `bridge_periodic_tx_late_ab.py`, `bridge_periodic_tx_mam_ab.py`, `bridge_periodic_tx_parity_ab.py`, `bridge_periodic_tx_retry_ab.py` | yes |
| `DecodiumBridgePortSettings.cpp` | No literal match | no literal match |
| `DecodiumBridgePrecomputedWave.cpp` | `bridge_precomputed_wave_store_runtime_ab.py` | no literal match |
| `DecodiumBridgeProcessUsage.cpp` | `bridge_processusage_runtime_lab.py` | yes |
| `DecodiumBridgePromptSnapshot.cpp` | No literal match | no literal match |
| `DecodiumBridgePskQueries.cpp` | `bridge_psk_local_response_ab.py` | no literal match |
| `DecodiumBridgePskReporter.cpp` | `bridge_psk_replay_app_ab.py` | yes |
| `DecodiumBridgeQ65Split.cpp` | `bridge_q65split_runtime_lab.py` | yes |
| `DecodiumBridgeQsoCache.cpp` | No literal match | yes |
| `DecodiumBridgeQsoCompletion.cpp` | No literal match | no literal match |
| `DecodiumBridgeQsoFields.cpp` | No literal match | no literal match |
| `DecodiumBridgeQsoFinish.cpp` | `bridge_qsofinish_runtime_lab.py` | yes |
| `DecodiumBridgeQsoJournal.cpp` | `bridge_qso_journal_runtime_ab.py` | no literal match |
| `DecodiumBridgeQsoLimit.cpp` | No literal match | no literal match |
| `DecodiumBridgeQsoReset.cpp` | `bridge_qsoreset_runtime_lab.py` | yes |
| `DecodiumBridgeQsoTransition.cpp` | `bridge_qsotransition_runtime_lab.py` | yes |
| `DecodiumBridgeQsyCommand.cpp` | `bridge_qsycommand_runtime_lab.py` | yes |
| `DecodiumBridgeQsyGuard.cpp` | No literal match | no literal match |
| `DecodiumBridgeReceivedFiles.cpp` | `bridge_directory_dialog_ab.py`, `bridge_file_dialog_runtime_ab.py` | yes |
| `DecodiumBridgeRemoteActivity.cpp` | `bridge_remote_activity_ws_ab.py` | no literal match |
| `DecodiumBridgeRemoteFrequency.cpp` | `bridge_remotefrequency_runtime_lab.py` | yes |
| `DecodiumBridgeRemoteLinkObjects.cpp` | `bridge_remotelinkobjects_runtime_lab.py` | yes |
| `DecodiumBridgeRemoteLinkSelection.cpp` | `bridge_remotelinkselection_runtime_lab.py` | yes |
| `DecodiumBridgeRemoteRadioState.cpp` | `bridge_remoteradiostate_runtime_lab.py` | yes |
| `DecodiumBridgeRemoteRxAudio.cpp` | `bridge_decoportaudioinput_runtime_lab.py`, `bridge_remoterxaudio_runtime_lab.py` | yes |
| `DecodiumBridgeRemoteWebInit.cpp` | `bridge_remote_activity_ws_ab.py`, `bridge_remote_web_commands_ab.py`, `bridge_remote_web_http_ab.py`, `bridge_remote_web_init_ab.py`, `bridge_remote_web_ws_auth_ab.py` | no literal match |
| `DecodiumBridgeResumeReply.cpp` | No literal match | no literal match |
| `DecodiumBridgeRigDiscovery.cpp` | `bridge_rigdiscovery_runtime_lab.py` | yes |
| `DecodiumBridgeRigTelemetry.cpp` | `bridge_rigtelemetry_runtime_lab.py` | yes |
| `DecodiumBridgeRtlInit.cpp` | `bridge_rtl_init_ab.py`, `bridge_rtl_init_retune_ab.py`, `bridge_rtl_init_started_ab.py` | no literal match |
| `DecodiumBridgeRtlSdrCapture.cpp` | `bridge_rtlsdrcapture_runtime_lab.py` | yes |
| `DecodiumBridgeRtlSdrConfig.cpp` | `bridge_rtlsdrconfig_runtime_lab.py` | yes |
| `DecodiumBridgeRtlSdrDiscovery.cpp` | `bridge_rtlsdrdiscovery_runtime_lab.py` | yes |
| `DecodiumBridgeRtlSdrPlayback.cpp` | No literal match | yes |
| `DecodiumBridgeRtlSdrSamples.cpp` | No literal match | yes |
| `DecodiumBridgeRtlSdrStatus.cpp` | `bridge_rtlsdrstatus_runtime_lab.py` | yes |
| `DecodiumBridgeRttyRigMode.cpp` | `bridge_rttyrigmode_runtime_lab.py` | yes |
| `DecodiumBridgeRxAudioTx.cpp` | `bridge_rxaudiotx_runtime_lab.py` | yes |
| `DecodiumBridgeRxControl.cpp` | `bridge_rxcontrol_audio_ab.py` | yes |
| `DecodiumBridgeRxMonitoring.cpp` | `bridge_rx_monitoring_loopback_ab.py` | no literal match |
| `DecodiumBridgeRxRecording.cpp` | No literal match | no literal match |
| `DecodiumBridgeRxTerminal.cpp` | No literal match | no literal match |
| `DecodiumBridgeSatelliteHalfDuplex.cpp` | No literal match | yes |
| `DecodiumBridgeSatelliteInit.cpp` | `bridge_satellite_init_ab.py`, `bridge_satellite_init_apply_ab.py` | no literal match |
| `DecodiumBridgeSatelliteOptions.cpp` | No literal match | yes |
| `DecodiumBridgeSaveSettings.cpp` | `bridge_save_settings_restart_ab.py` | no literal match |
| `DecodiumBridgeSecondInstance.cpp` | `bridge_secondinstance_runtime_lab.py` | yes |
| `DecodiumBridgeSelfCheck.cpp` | `bridge_selfcheck_runtime_lab.py` | yes |
| `DecodiumBridgeSendTx.cpp` | `bridge_sendtx_ft8_deferred_ab.py`, `bridge_sendtx_ft8_guards_ab.py`, `bridge_sendtx_ft8_loopback_ab.py`, `bridge_sendtx_jtty_ab.py`, `bridge_sendtx_jtty_result_ab.py`, `bridge_sendtx_legacy_delegate_ab.py`, `bridge_sendtx_repair_partner_ab.py`, `bridge_sendtx_stale_partner_ab.py` | yes |
| `DecodiumBridgeSequenceOptions.cpp` | No literal match | no literal match |
| `DecodiumBridgeServices.cpp` | `bridge_services_runtime_ab.py`, `bridge_sharing_accessors_runtime_ab.py` | no literal match |
| `DecodiumBridgeSettingsHelpers.cpp` | `bridge_settings_helpers_runtime_ab.py` | no literal match |
| `DecodiumBridgeSharedRuntime.cpp` | `bridge_shared_runtime_ab.py`, `bridge_tx_policy_adapters_runtime_ab.py` | no literal match |
| `DecodiumBridgeSharingConfig.cpp` | `bridge_sharingconfig_runtime_lab.py` | yes |
| `DecodiumBridgeShutdown.cpp` | `bridge_shutdown_active_ab.py`, `bridge_shutdown_runtime_ab.py` | no literal match |
| `DecodiumBridgeSmartFt2Scheduler.cpp` | `bridge_smartft2_cancel_timer_ab.py`, `bridge_smartft2_hold_timer_ab.py`, `bridge_smartft2_mode_timer_ab.py`, `bridge_smartft2scheduler_runtime_lab.py` | yes |
| `DecodiumBridgeSpecialOperation.cpp` | `bridge_specialop_wrappers_ab.py` | yes |
| `DecodiumBridgeSpectrum.cpp` | No literal match | yes |
| `DecodiumBridgeSpectrumTimer.cpp` | `bridge_spectrum_timer_cpu_ab.py`, `bridge_spectrum_timer_fallback_frame_ab.py`, `bridge_spectrum_timer_gpu_handoff_ab.py`, `bridge_spectrum_timer_hidden_ab.py`, `bridge_spectrum_timer_visible_fallback_ab.py` | yes |
| `DecodiumBridgeSpectrumVisibility.cpp` | No literal match | no literal match |
| `DecodiumBridgeSsb.cpp` | `bridge_ssb_unit_runtime_ab.py` | no literal match |
| `DecodiumBridgeSstv.cpp` | `bridge_amplifier_port_runtime_ab.py`, `bridge_async_logs_runtime_ab.py`, `bridge_audio_adapters_runtime_ab.py`, `bridge_audio_buffers_runtime_ab.py`, `bridge_audio_channel_helpers_runtime_ab.py`, `bridge_audio_device_init_ab.py`, `bridge_audiorefresh_runtime_lab.py`, `bridge_audiosink_gateway_ab.py`, `bridge_audiosink_ssb_ab.py`, `bridge_audiosink_taps_ab.py`, `bridge_auto_sequence_runtime_ab.py`, `bridge_autocallcomplete_runtime_lab.py`, `bridge_autocallselect_runtime_lab.py`, `bridge_autocqhistory_runtime_lab.py`, `bridge_autocqlock_runtime_lab.py`, `bridge_autocqrepeat_runtime_lab.py`, `bridge_autolog_helpers_runtime_ab.py`, `bridge_callsign_confirmed_adif_ab.py`, `bridge_callsign_init_ab.py`, `bridge_callsign_url_ab.py`, `bridge_cat_backend_helpers_runtime_ab.py`, `bridge_cat_connections_ab.py`, `bridge_cat_connections_confirm_ab.py`, `bridge_cat_connections_connect_ab.py`, `bridge_cat_connections_frequency_ab.py`, `bridge_cat_connections_mode_ab.py`, `bridge_cat_connections_ptt_ab.py`, `bridge_cat_connections_rigctl_ab.py`, `bridge_cat_dispatch_runtime_ab.py`, `bridge_cat_dispatch_ssb_runtime_ab.py`, `bridge_cat_migration_startup_runtime_ab.py`, `bridge_cat_ptt_state_runtime_ab.py`, `bridge_cat_telemetry_ab.py`, `bridge_catqsy_runtime_lab.py`, `bridge_catrecovery_runtime_lab.py`, `bridge_catrigmode_runtime_lab.py`, `bridge_catsplitsync_runtime_lab.py`, `bridge_cleartxmessages_runtime_lab.py`, `bridge_complete_tx_guards_ab.py`, `bridge_complete_tx_hold_timer_ab.py`, `bridge_complete_tx_pcm_ab.py`, `bridge_complete_tx_runtime_ab.py`, `bridge_complete_tx_sink_ab.py`, `bridge_complete_tx_special_ab.py`, `bridge_complete_tx_sstv_legacy_ab.py`, `bridge_complete_tx_worker_error_ab.py`, `bridge_complete_tx_worker_thread_ab.py`, `bridge_constructor_runtime_ab.py`, `bridge_core_timers_init_ab.py`, `bridge_correctedtime_runtime_lab.py`, `bridge_courtesy_runtime_lab.py`, `bridge_courtesy_success_runtime_ab.py`, `bridge_decode_adapters_runtime_ab.py`, `bridge_decode_cluster_connections_ab.py`, `bridge_decode_history_app_ab.py`, `bridge_decode_keys_runtime_ab.py`, `bridge_decode_model_init_ab.py`, `bridge_decode_visibility_app_ab.py`, `bridge_decodeflow_queue_runtime_ab.py`, `bridge_decodetxwait_runtime_lab.py`, `bridge_decolog_init_ab.py`, `bridge_decolog_local_ab.py`, `bridge_decolog_tune_ab.py`, `bridge_decoportaudioinput_runtime_lab.py`, `bridge_decoportdriver_runtime_lab.py`, `bridge_decoportptt_runtime_lab.py`, `bridge_decoporttxaudio_runtime_lab.py`, `bridge_decoporttxframes_runtime_lab.py`, `bridge_deferredseq_runtime_lab.py`, `bridge_destructor_runtime_ab.py`, `bridge_diagnostic_connections_ab.py`, `bridge_diagnostics_runtime_ab.py`, `bridge_directaudiotx_runtime_lab.py`, `bridge_directory_dialog_ab.py`, `bridge_display_frequency_runtime_ab.py`, `bridge_dxcall_runtime_lab.py`, `bridge_dxcluster_init_ab.py`, `bridge_dxcluster_init_local_ab.py`, `bridge_environment_rx_policy_runtime_ab.py`, `bridge_feeddecoder_fst4_ab.py`, `bridge_feeddecoder_fst4_reference_ab.py`, `bridge_feeddecoder_ft2_ab.py`, `bridge_feeddecoder_ft4_ab.py`, `bridge_feeddecoder_ft8_ab.py`, `bridge_feeddecoder_jt4_ab.py`, `bridge_feeddecoder_jt65_ab.py`, `bridge_feeddecoder_jt65_reference_ab.py`, `bridge_feeddecoder_jt9_reference_ab.py`, `bridge_feeddecoder_msk144_ab.py`, `bridge_feeddecoder_q65_ab.py`, `bridge_feeddecoder_wspr_reference_ab.py`, `bridge_file_dialog_runtime_ab.py`, `bridge_file_paths_runtime_ab.py`, `bridge_frequency_calibration_app_ab.py`, `bridge_fst4_clean_pcm_integrity_probe.py`, `bridge_fst4_clean_request_parameters_probe.py`, `bridge_fst4_clean_wide_probe_ab.py`, `bridge_ft2_async_ready_audio_ab.py`, `bridge_ft2_async_ready_guards_ab.py`, `bridge_ft2_async_timer_adaptive_ab.py`, `bridge_ft2_async_timer_audio_ab.py`, `bridge_ft2_async_timer_guards_ab.py`, `bridge_ft2_async_timer_ranges_ab.py`, `bridge_ft2_async_timer_sequential_ab.py`, `bridge_ft2_sync_ready_audio_ab.py`, `bridge_ft2_sync_ready_cache_ab.py`, `bridge_ft2audiogates_runtime_lab.py`, `bridge_ft4_legacy_revision_diagnostic_ab.py`, `bridge_ft4early_runtime_lab.py`, `bridge_ft8_ready_audio_ab.py`, `bridge_ft8_ready_cq_filter_ab.py`, `bridge_ft8_ready_dedup_ab.py`, `bridge_ft8_ready_fst4_reference_ab.py`, `bridge_ft8_ready_ft2_audio_ab.py`, `bridge_ft8_ready_ft4_audio_ab.py`, `bridge_ft8_ready_guards_ab.py`, `bridge_ft8_ready_jt4_audio_ab.py`, `bridge_ft8_ready_jt65_reference_ab.py`, `bridge_ft8_ready_jt9_reference_ab.py`, `bridge_ft8_ready_legacy_immediate_ab.py`, `bridge_ft8_ready_legacy_mirror_ab.py`, `bridge_ft8_ready_legacy_publish_ab.py`, `bridge_ft8_ready_msk144_audio_ab.py`, `bridge_ft8_ready_mycall_filter_ab.py`, `bridge_ft8_ready_q65_audio_ab.py`, `bridge_ft8bridgegate_runtime_lab.py`, `bridge_ft8early_decode_ab.py`, `bridge_ft8early_gates_ab.py`, `bridge_ft8queue_decode_ab.py`, `bridge_ft8queue_params_ab.py`, `bridge_ft_scheduler_runtime_ab.py`, `bridge_ft_thread_budget_helpers_runtime_ab.py`, `bridge_hamdrm_deferred_gallery_ab.py`, `bridge_hamdrm_gallery_archive_ab.py`, `bridge_houndtxprepare_runtime_lab.py`, `bridge_jt65_deep_probe.py`, `bridge_jt65_pcm_integrity_probe.py`, `bridge_jt65_request_parameters_probe.py`, `bridge_jt65_tolerance_probe.py`, `bridge_jtdecodefallback_runtime_lab.py`, `bridge_layout_runtime_ab.py`, `bridge_legacy_bridge_audio_guard_ab.py`, `bridge_legacy_bridge_audio_loopback_ab.py`, `bridge_legacy_bridge_audio_stop_ab.py`, `bridge_legacy_filter_connections_runtime_ab.py`, `bridge_legacy_palette_backend_runtime_ab.py`, `bridge_legacy_preflight_abort_ab.py`, `bridge_legacy_preflight_loopback_ab.py`, `bridge_legacy_presentation_helpers_runtime_ab.py`, `bridge_legacy_rig_policy_runtime_ab.py`, `bridge_legacy_sync_blacklist_ab.py`, `bridge_legacy_sync_cq_filter_ab.py`, `bridge_legacy_sync_guards_ab.py`, `bridge_legacy_sync_queued_ab.py`, `bridge_legacyaudiopolicy_runtime_lab.py`, `bridge_legacyaudiostart_runtime_lab.py`, `bridge_legacyaugment_runtime_ab.py`, `bridge_legacyhash_app_ab.py`, `bridge_legacyinit_runtime_lab.py`, `bridge_legacyptttransition_runtime_lab.py`, `bridge_legacystatesync_runtime_lab.py`, `bridge_log_prompt_cluster_offline_ab.py`, `bridge_log_prompt_dialog_ab.py`, `bridge_log_prompt_dialog_add_ab.py`, `bridge_log_prompt_dialog_satellite_ab.py`, `bridge_log_qso_legacy_delegate_ab.py`, `bridge_log_qso_legacy_real_ab.py`, `bridge_log_qso_runtime_ab.py`, `bridge_log_routing_runtime_ab.py`, `bridge_logpreview_snapshot_runtime_ab.py`, `bridge_mac_legacy_start_ab.py`, `bridge_mac_standalone_start_ab.py`, `bridge_mamcaller_runtime_lab.py`, `bridge_mamprune_expiry_runtime_ab.py`, `bridge_manualhold_runtime_lab.py`, `bridge_map_init_ab.py`, `bridge_map_init_abort_ab.py`, `bridge_map_init_moon_ab.py`, `bridge_mode_period_ab.py`, `bridge_modecleanup_runtime_lab.py`, `bridge_modulator_cw_pending_ab.py`, `bridge_modulator_idle_ft2_ab.py`, `bridge_modulator_idle_legacy_ab.py`, `bridge_modulator_idle_pcm_ab.py`, `bridge_modulator_idle_runtime_ab.py`, `bridge_multianswer_mode_ab.py`, `bridge_partner_guard_old_roger_ab.py`, `bridge_partner_guard_reject_ab.py`, `bridge_partner_guard_repair_ab.py`, `bridge_partner_guard_retry_ab.py`, `bridge_partner_guard_roger_ab.py`, `bridge_period_timer_ft8_audio_ab.py`, `bridge_period_timer_jt65_audio_ab.py`, `bridge_period_timer_runtime_ab.py`, `bridge_period_timer_watchdog_ab.py`, `bridge_periodic_tx_cancel_ab.py`, `bridge_periodic_tx_dispatch_ab.py`, `bridge_periodic_tx_guards_ab.py`, `bridge_periodic_tx_late_ab.py`, `bridge_periodic_tx_mam_ab.py`, `bridge_periodic_tx_parity_ab.py`, `bridge_periodic_tx_retry_ab.py`, `bridge_periodtimerarm_runtime_lab.py`, `bridge_precomputed_wave_store_runtime_ab.py`, `bridge_processusage_runtime_lab.py`, `bridge_psk_cloudlog_adif_ab.py`, `bridge_psk_cloudlog_http_ab.py`, `bridge_psk_cloudlog_init_ab.py`, `bridge_psk_replay_app_ab.py`, `bridge_q65split_runtime_lab.py`, `bridge_qml_load_timestamp_ab.py`, `bridge_qrz_wspr_init_ab.py`, `bridge_qso_journal_runtime_ab.py`, `bridge_qsofinish_runtime_lab.py`, `bridge_qsoreset_runtime_lab.py`, `bridge_qsotransition_runtime_lab.py`, `bridge_ready_profile_apply_ab.py`, `bridge_ready_profile_init_ab.py`, `bridge_remote_activity_ws_ab.py`, `bridge_remote_web_commands_ab.py`, `bridge_remote_web_http_ab.py`, `bridge_remote_web_init_ab.py`, `bridge_remote_web_ws_auth_ab.py`, `bridge_remotefrequency_runtime_lab.py`, `bridge_remoteradiostate_runtime_lab.py`, `bridge_rigtelemetry_runtime_lab.py`, `bridge_rtl_init_ab.py`, `bridge_rtl_init_retune_ab.py`, `bridge_rtl_init_started_ab.py`, `bridge_rtlsdrcapture_runtime_lab.py`, `bridge_rtlsdrconfig_runtime_lab.py`, `bridge_rttyrigmode_runtime_lab.py`, `bridge_rxaudiotx_runtime_lab.py`, `bridge_satellite_init_ab.py`, `bridge_satellite_init_apply_ab.py`, `bridge_sendtx_ft8_deferred_ab.py`, `bridge_sendtx_ft8_guards_ab.py`, `bridge_sendtx_jtty_ab.py`, `bridge_sendtx_jtty_result_ab.py`, `bridge_sendtx_legacy_delegate_ab.py`, `bridge_sendtx_repair_partner_ab.py`, `bridge_sendtx_stale_partner_ab.py`, `bridge_services_runtime_ab.py`, `bridge_shared_runtime_ab.py`, `bridge_sharing_accessors_runtime_ab.py`, `bridge_shutdown_active_ab.py`, `bridge_shutdown_runtime_ab.py`, `bridge_smartft2_cancel_timer_ab.py`, `bridge_smartft2_hold_timer_ab.py`, `bridge_smartft2_mode_timer_ab.py`, `bridge_smartft2scheduler_runtime_lab.py`, `bridge_spectrum_timer_cpu_ab.py`, `bridge_spectrum_timer_fallback_frame_ab.py`, `bridge_spectrum_timer_gpu_handoff_ab.py`, `bridge_spectrum_timer_hidden_ab.py`, `bridge_spectrum_timer_visible_fallback_ab.py`, `bridge_ssb_unit_runtime_ab.py`, `bridge_sstv_audio_source_extraction_ab.py`, `bridge_sstv_auto_image_save_ab.py`, `bridge_sstv_calibration_tx_capture_ab.py`, `bridge_sstv_calibration_tx_extraction_ab.py`, `bridge_sstv_detected_mode_ab.py`, `bridge_sstv_diagnostics_snapshot_ab.py`, `bridge_sstv_fax480_tx_reject_ab.py`, `bridge_sstv_hamdrm_active_diagnostics_ab.py`, `bridge_sstv_hamdrm_diagnostics_ab.py`, `bridge_sstv_hamdrm_tx_cancel_ab.py`, `bridge_sstv_helpers_controls_ab.py`, `bridge_sstv_image_save_ab.py`, `bridge_sstv_image_save_guards_ab.py`, `bridge_sstv_image_status_ab.py`, `bridge_sstv_invalid_source_guard_ab.py`, `bridge_sstv_qso_association_ab.py`, `bridge_sstv_qso_association_extraction_ab.py`, `bridge_sstv_qso_choices_ab.py`, `bridge_sstv_qso_choices_async_ab.py`, `bridge_sstv_radiofax_tx_reject_ab.py`, `bridge_sstv_raw_audio_ab.py`, `bridge_sstv_raw_cancel_ab.py`, `bridge_sstv_raw_image_ab.py`, `bridge_sstv_redecode_ab.py`, `bridge_sstv_replay_source_guard_ab.py`, `bridge_sstv_reset_probe_ab.py`, `bridge_sstv_runtime_shutdown_extraction_ab.py`, `bridge_sstv_rx_controls_ab.py`, `bridge_sstv_rx_diagnostics_ab.py`, `bridge_sstv_rx_reset_ab.py`, `bridge_sstv_rx_source_route_extraction_ab.py`, `bridge_sstv_rx_start_extraction_ab.py`, `bridge_sstv_rx_start_legacy_worker_failure_ab.py`, `bridge_sstv_rx_start_rollback_ab.py`, `bridge_sstv_rx_start_worker_failure_ab.py`, `bridge_sstv_rx_stop_extraction_ab.py`, `bridge_sstv_source_signals_ab.py`, `bridge_sstv_stale_source_pcm_ab.py`, `bridge_sstv_status_ab.py`, `bridge_sstv_status_getters_ab.py`, `bridge_sstv_storage_queue_extraction_ab.py`, `bridge_sstv_storage_shutdown_extraction_ab.py`, `bridge_sstv_storage_startup_ab.py`, `bridge_sstv_studio_archive_ab.py`, `bridge_sstv_studio_status_ab.py`, `bridge_sstv_tx_init_capture_ab.py`, `bridge_sstv_tx_lifecycle_extraction_ab.py`, `bridge_sstv_tx_start_reject_ab.py`, `bridge_sstv_wav_replay_ab.py`, `bridge_sstv_wav_replay_cancel_ab.py`, `bridge_sstv_wav_replay_real_rebind_ab.py`, `bridge_sstv_workspace_extraction_ab.py`, `bridge_sstvqso_runtime_lab.py`, `bridge_start_tx_guards_ab.py`, `bridge_start_tx_preflight_notices_ab.py`, `bridge_start_tx_selection_runtime_ab.py`, `bridge_startupmode_runtime_lab.py`, `bridge_startupservices_runtime_lab.py`, `bridge_state_runtime_ab.py`, `bridge_stop_tx_routes_ab.py`, `bridge_stop_tx_runtime_ab.py`, `bridge_swr_admission_app_ab.py`, `bridge_targetcall_decode_runtime_ab.py`, `bridge_targetcall_timeout_runtime_ab.py`, `bridge_tci_complete_ab.py`, `bridge_tci_signals_ab.py`, `bridge_timesync_init_ab.py`, `bridge_timesync_init_rearm_ab.py`, `bridge_timesyncdispatch_decode_ab.py`, `bridge_timesyncdispatch_runtime_ab.py`, `bridge_tune_guards_ab.py`, `bridge_tune_rtl_toggle_ab.py`, `bridge_tunewatchdog_runtime_lab.py`, `bridge_tx_adapters_runtime_ab.py`, `bridge_tx_audio_startup_runtime_ab.py`, `bridge_tx_enable_runtime_ab.py`, `bridge_tx_output_fallback_ab.py`, `bridge_tx_output_resolve_runtime_ab.py`, `bridge_tx_pcm_offset_loopback_ab.py`, `bridge_tx_policy_adapters_runtime_ab.py`, `bridge_tx_slot_helpers_runtime_ab.py`, `bridge_tx_wave_helpers_runtime_ab.py`, `bridge_txaudiotelemetry_explicit_lab.py`, `bridge_txdispatchguard_notifications_ab.py`, `bridge_txdispatchguard_runtime_lab.py`, `bridge_txprecompute_explicit_lab.py`, `bridge_txprecompute_wave_hash_ab.py`, `bridge_txrecording_runtime_lab.py`, `bridge_udpinit_runtime_lab.py`, `bridge_udpinit_wire_runtime_lab.py`, `bridge_udprestart_runtime_lab.py`, `bridge_ui_audio_state_runtime_ab.py`, `bridge_uistall_monitor_runtime_ab.py`, `bridge_usstate_dxcc_refresh_ab.py`, `bridge_usstate_wspr_init_ab.py`, `bridge_visual_tx_time_runtime_ab.py`, `bridge_wav_init_ab.py`, `bridge_wav_init_timed_ab.py`, `bridge_worldmap_call_helpers_runtime_ab.py`, `bridge_worldmapreplay_synthetic_app_ab.py` | yes |
| `DecodiumBridgeSstvAudioSource.cpp` | `bridge_sstv_audio_source_extraction_ab.py` | no literal match |
| `DecodiumBridgeSstvCalibrationTx.cpp` | `bridge_sstv_calibration_tx_capture_ab.py`, `bridge_sstv_calibration_tx_extraction_ab.py` | no literal match |
| `DecodiumBridgeSstvControls.cpp` | No literal match | yes |
| `DecodiumBridgeSstvDiagnosticsSnapshot.cpp` | `bridge_sstv_diagnostics_snapshot_ab.py` | no literal match |
| `DecodiumBridgeSstvFax480Tx.cpp` | `bridge_sstv_fax480_tx_reject_ab.py` | no literal match |
| `DecodiumBridgeSstvHamDrmTx.cpp` | `bridge_sstv_hamdrm_tx_cancel_ab.py` | no literal match |
| `DecodiumBridgeSstvHelpers.cpp` | `bridge_sstv_helpers_controls_ab.py` | no literal match |
| `DecodiumBridgeSstvImageSave.cpp` | `bridge_sstv_auto_image_save_ab.py`, `bridge_sstv_image_save_ab.py`, `bridge_sstv_image_save_guards_ab.py`, `bridge_sstv_reset_probe_ab.py` | no literal match |
| `DecodiumBridgeSstvProducerTaps.cpp` | No literal match | no literal match |
| `DecodiumBridgeSstvQso.cpp` | `bridge_sstvqso_runtime_lab.py` | yes |
| `DecodiumBridgeSstvQsoAssociation.cpp` | `bridge_sstv_qso_association_extraction_ab.py` | no literal match |
| `DecodiumBridgeSstvQsoChoices.cpp` | `bridge_sstv_qso_association_ab.py`, `bridge_sstv_qso_choices_ab.py`, `bridge_sstv_qso_choices_async_ab.py` | no literal match |
| `DecodiumBridgeSstvRadiofaxTx.cpp` | `bridge_sstv_radiofax_tx_reject_ab.py` | no literal match |
| `DecodiumBridgeSstvRawAudio.cpp` | `bridge_sstv_raw_audio_ab.py`, `bridge_sstv_raw_cancel_ab.py`, `bridge_sstv_raw_image_ab.py` | no literal match |
| `DecodiumBridgeSstvRedecode.cpp` | `bridge_sstv_redecode_ab.py` | no literal match |
| `DecodiumBridgeSstvRuntimeShutdown.cpp` | `bridge_sstv_runtime_shutdown_extraction_ab.py` | no literal match |
| `DecodiumBridgeSstvRxDiagnostics.cpp` | `bridge_sstv_hamdrm_active_diagnostics_ab.py`, `bridge_sstv_hamdrm_diagnostics_ab.py`, `bridge_sstv_rx_diagnostics_ab.py` | no literal match |
| `DecodiumBridgeSstvRxReset.cpp` | `bridge_sstv_rx_controls_ab.py`, `bridge_sstv_rx_reset_ab.py` | no literal match |
| `DecodiumBridgeSstvRxSourceRoute.cpp` | `bridge_sstv_invalid_source_guard_ab.py`, `bridge_sstv_replay_source_guard_ab.py`, `bridge_sstv_rx_source_route_extraction_ab.py`, `bridge_sstv_source_signals_ab.py`, `bridge_sstv_stale_source_pcm_ab.py` | no literal match |
| `DecodiumBridgeSstvRxStart.cpp` | `bridge_sstv_rx_start_extraction_ab.py`, `bridge_sstv_rx_start_legacy_worker_failure_ab.py`, `bridge_sstv_rx_start_rollback_ab.py`, `bridge_sstv_rx_start_worker_failure_ab.py` | no literal match |
| `DecodiumBridgeSstvRxStop.cpp` | `bridge_sstv_rx_stop_extraction_ab.py` | no literal match |
| `DecodiumBridgeSstvStatus.cpp` | `bridge_sstv_detected_mode_ab.py`, `bridge_sstv_image_status_ab.py`, `bridge_sstv_status_ab.py`, `bridge_sstv_status_getters_ab.py`, `bridge_sstv_studio_status_ab.py` | no literal match |
| `DecodiumBridgeSstvStorageQueue.cpp` | `bridge_sstv_storage_queue_extraction_ab.py` | no literal match |
| `DecodiumBridgeSstvStorageShutdown.cpp` | `bridge_sstv_storage_shutdown_extraction_ab.py` | no literal match |
| `DecodiumBridgeSstvStorageStartup.cpp` | `bridge_sstv_storage_startup_ab.py` | no literal match |
| `DecodiumBridgeSstvStorageState.cpp` | No literal match | yes |
| `DecodiumBridgeSstvStudioArchive.cpp` | `bridge_hamdrm_deferred_gallery_ab.py`, `bridge_hamdrm_gallery_archive_ab.py`, `bridge_sstv_studio_archive_ab.py` | no literal match |
| `DecodiumBridgeSstvTiming.cpp` | No literal match | no literal match |
| `DecodiumBridgeSstvTxInit.cpp` | `bridge_sstv_tx_init_capture_ab.py` | no literal match |
| `DecodiumBridgeSstvTxLifecycle.cpp` | `bridge_sstv_tx_lifecycle_extraction_ab.py`, `bridge_stop_tx_routes_ab.py` | no literal match |
| `DecodiumBridgeSstvTxStart.cpp` | `bridge_sstv_tx_start_reject_ab.py` | no literal match |
| `DecodiumBridgeSstvTxState.cpp` | No literal match | yes |
| `DecodiumBridgeSstvWavReplay.cpp` | `bridge_sstv_wav_replay_ab.py`, `bridge_sstv_wav_replay_cancel_ab.py`, `bridge_sstv_wav_replay_real_rebind_ab.py` | no literal match |
| `DecodiumBridgeSstvWorkspace.cpp` | `bridge_sstv_workspace_extraction_ab.py` | no literal match |
| `DecodiumBridgeStartTx.cpp` | `bridge_periodic_tx_cancel_ab.py`, `bridge_periodic_tx_dispatch_ab.py`, `bridge_periodic_tx_late_ab.py`, `bridge_periodic_tx_mam_ab.py`, `bridge_periodic_tx_parity_ab.py`, `bridge_periodic_tx_retry_ab.py`, `bridge_start_tx_guards_ab.py`, `bridge_start_tx_loopback_ab.py`, `bridge_start_tx_preflight_notices_ab.py`, `bridge_start_tx_selection_runtime_ab.py`, `bridge_tx_pcm_offset_loopback_ab.py` | no literal match |
| `DecodiumBridgeStartTxPreflight.cpp` | No literal match | no literal match |
| `DecodiumBridgeStartTxSelection.cpp` | No literal match | no literal match |
| `DecodiumBridgeStartupMode.cpp` | `bridge_startupmode_runtime_lab.py` | yes |
| `DecodiumBridgeStartupQso.cpp` | No literal match | no literal match |
| `DecodiumBridgeStartupServices.cpp` | `bridge_startupservices_runtime_lab.py` | yes |
| `DecodiumBridgeState.cpp` | `bridge_state_runtime_ab.py`, `bridge_ui_audio_state_runtime_ab.py` | no literal match |
| `DecodiumBridgeStation.cpp` | `bridge_map_init_moon_ab.py` | yes |
| `DecodiumBridgeStationFrequencies.cpp` | `bridge_stationfreq_restart_ab.py` | yes |
| `DecodiumBridgeStationTelemetry.cpp` | No literal match | no literal match |
| `DecodiumBridgeStationWeather.cpp` | No literal match | no literal match |
| `DecodiumBridgeStopTx.cpp` | `bridge_stop_tx_routes_ab.py`, `bridge_stop_tx_runtime_ab.py` | no literal match |
| `DecodiumBridgeSyncSchedule.cpp` | `bridge_periodic_tx_retry_ab.py` | yes |
| `DecodiumBridgeTargetCallSettings.cpp` | `bridge_targetcall_decode_runtime_ab.py`, `bridge_targetcall_timeout_runtime_ab.py` | yes |
| `DecodiumBridgeTciAudio.cpp` | `bridge_tci_complete_ab.py`, `bridge_tci_signals_ab.py` | yes |
| `DecodiumBridgeTeleprinter.cpp` | No literal match | yes |
| `DecodiumBridgeTimeSync.cpp` | `bridge_ntp_manual_runtime_lab.py`, `bridge_timesync_init_ab.py`, `bridge_timesync_init_rearm_ab.py` | yes |
| `DecodiumBridgeTimeSyncDispatch.cpp` | `bridge_feeddecoder_fst4_ab.py`, `bridge_feeddecoder_fst4_reference_ab.py`, `bridge_feeddecoder_ft2_ab.py`, `bridge_feeddecoder_ft4_ab.py`, `bridge_feeddecoder_ft8_ab.py`, `bridge_feeddecoder_jt4_ab.py`, `bridge_feeddecoder_jt65_ab.py`, `bridge_feeddecoder_jt65_reference_ab.py`, `bridge_feeddecoder_jt9_reference_ab.py`, `bridge_feeddecoder_msk144_ab.py`, `bridge_feeddecoder_q65_ab.py`, `bridge_feeddecoder_wspr_reference_ab.py`, `bridge_fst4_clean_pcm_integrity_probe.py`, `bridge_fst4_clean_request_parameters_probe.py`, `bridge_fst4_clean_wide_probe_ab.py`, `bridge_ft2_async_ready_audio_ab.py`, `bridge_ft2_async_timer_audio_ab.py`, `bridge_ft2_sync_ready_audio_ab.py`, `bridge_ft8_ready_audio_ab.py`, `bridge_ft8_ready_fst4_reference_ab.py`, `bridge_ft8_ready_ft2_audio_ab.py`, `bridge_ft8_ready_ft4_audio_ab.py`, `bridge_ft8_ready_jt4_audio_ab.py`, `bridge_ft8_ready_jt65_reference_ab.py`, `bridge_ft8_ready_jt9_reference_ab.py`, `bridge_ft8_ready_msk144_audio_ab.py`, `bridge_ft8_ready_q65_audio_ab.py`, `bridge_ft8queue_decode_ab.py`, `bridge_jt65_deep_probe.py`, `bridge_jt65_pcm_integrity_probe.py`, `bridge_jt65_request_parameters_probe.py`, `bridge_jt65_tolerance_probe.py`, `bridge_period_timer_ft8_audio_ab.py`, `bridge_period_timer_jt65_audio_ab.py`, `bridge_timesyncdispatch_decode_ab.py`, `bridge_timesyncdispatch_runtime_ab.py` | yes |
| `DecodiumBridgeTune.cpp` | `bridge_tune_guards_ab.py`, `bridge_tune_rtl_toggle_ab.py` | no literal match |
| `DecodiumBridgeTuneWatchdog.cpp` | `bridge_tunewatchdog_runtime_lab.py` | yes |
| `DecodiumBridgeTxAudioStartup.cpp` | `bridge_tx_audio_startup_loopback_ab.py`, `bridge_tx_audio_startup_runtime_ab.py` | no literal match |
| `DecodiumBridgeTxAudioTelemetry.cpp` | `bridge_txaudiotelemetry_explicit_lab.py` | yes |
| `DecodiumBridgeTxDispatchGuard.cpp` | `bridge_txdispatchguard_notifications_ab.py`, `bridge_txdispatchguard_runtime_lab.py` | yes |
| `DecodiumBridgeTxEnable.cpp` | `bridge_autocqrepeat_runtime_lab.py`, `bridge_tx_enable_runtime_ab.py` | yes |
| `DecodiumBridgeTxFinish.cpp` | `bridge_txboundary_runtime_lab.py`, `bridge_txfinish_runtime_lab.py` | yes |
| `DecodiumBridgeTxFormat.cpp` | No literal match | yes |
| `DecodiumBridgeTxMessages.cpp` | `bridge_cleartx_active_ab.py`, `bridge_cleartxmessages_runtime_lab.py` | yes |
| `DecodiumBridgeTxOutputDevice.cpp` | `bridge_tx_output_device_loopback_ab.py`, `bridge_tx_output_fallback_ab.py` | no literal match |
| `DecodiumBridgeTxOutputResolve.cpp` | `bridge_tx_output_resolve_runtime_ab.py` | no literal match |
| `DecodiumBridgeTxPartnerGuard.cpp` | `bridge_partner_guard_ft8_loopback_ab.py`, `bridge_partner_guard_old_roger_ab.py`, `bridge_partner_guard_reject_ab.py`, `bridge_partner_guard_repair_ab.py`, `bridge_partner_guard_retry_ab.py`, `bridge_partner_guard_roger_ab.py` | yes |
| `DecodiumBridgeTxPcm.cpp` | No literal match | yes |
| `DecodiumBridgeTxPolicy.cpp` | No literal match | yes |
| `DecodiumBridgeTxPrecompute.cpp` | `bridge_txprecompute_explicit_lab.py`, `bridge_txprecompute_wave_hash_ab.py` | yes |
| `DecodiumBridgeTxPrepare.cpp` | `bridge_tx_adapters_loopback_ab.py`, `bridge_tx_adapters_runtime_ab.py` | yes |
| `DecodiumBridgeTxProtection.cpp` | `bridge_swr_admission_app_ab.py` | yes |
| `DecodiumBridgeTxRecording.cpp` | `bridge_txrecording_runtime_lab.py` | yes |
| `DecodiumBridgeTxScheduling.cpp` | No literal match | yes |
| `DecodiumBridgeTxSelection.cpp` | No literal match | yes |
| `DecodiumBridgeTxSlot.cpp` | No literal match | yes |
| `DecodiumBridgeTxSlotHelpers.cpp` | `bridge_tx_slot_helpers_runtime_ab.py` | no literal match |
| `DecodiumBridgeTxTiming.cpp` | `bridge_tx_pcm_offset_loopback_ab.py` | yes |
| `DecodiumBridgeTxTimingMath.cpp` | No literal match | yes |
| `DecodiumBridgeTxWatchdog.cpp` | No literal match | no literal match |
| `DecodiumBridgeTxWave.cpp` | No literal match | yes |
| `DecodiumBridgeTxWaveHelpers.cpp` | `bridge_tx_wave_helpers_runtime_ab.py` | no literal match |
| `DecodiumBridgeUdpDecode.cpp` | No literal match | no literal match |
| `DecodiumBridgeUdpInit.cpp` | `bridge_udpinit_runtime_lab.py`, `bridge_udpinit_wire_runtime_lab.py` | yes |
| `DecodiumBridgeUdpRestart.cpp` | `bridge_udprestart_runtime_lab.py` | yes |
| `DecodiumBridgeUiActions.cpp` | `bridge_ui_actions_runtime_ab.py` | yes |
| `DecodiumBridgeUiPreferences.cpp` | No literal match | yes |
| `DecodiumBridgeUiStall.cpp` | `bridge_uistall_monitor_runtime_ab.py` | yes |
| `DecodiumBridgeUiStartupSettings.cpp` | `bridge_ui_startup_settings_runtime_ab.py` | no literal match |
| `DecodiumBridgeUpdates.cpp` | No literal match | yes |
| `DecodiumBridgeUsStateLotw.cpp` | `bridge_lotw_cache_app_ab.py`, `bridge_lotw_cache_runtime_ab.py`, `bridge_lotw_download_runtime_ab.py`, `bridge_lotw_empty_download_runtime_ab.py`, `bridge_lotw_existing_runtime_ab.py`, `bridge_lotw_migration_runtime_ab.py`, `bridge_lotw_network_empty_cache_ab.py`, `bridge_lotw_network_error_runtime_ab.py`, `bridge_lotw_newest_runtime_ab.py`, `bridge_usstate_dxcc_refresh_ab.py`, `bridge_usstate_wspr_init_ab.py` | yes |
| `DecodiumBridgeVhfUhfSetting.cpp` | `bridge_vhfuhfsetting_runtime_lab.py` | yes |
| `DecodiumBridgeVisualFeedScheduling.cpp` | No literal match | no literal match |
| `DecodiumBridgeVisualTxCoalesce.cpp` | No literal match | no literal match |
| `DecodiumBridgeVisualTxTime.cpp` | `bridge_visual_tx_time_runtime_ab.py` | no literal match |
| `DecodiumBridgeWaitPounce.cpp` | No literal match | no literal match |
| `DecodiumBridgeWavDecode.cpp` | No literal match | yes |
| `DecodiumBridgeWavIO.cpp` | No literal match | yes |
| `DecodiumBridgeWavInit.cpp` | `bridge_wav_init_ab.py`, `bridge_wav_init_timed_ab.py` | no literal match |
| `DecodiumBridgeWebServer.cpp` | No literal match | yes |
| `DecodiumBridgeWindowState.cpp` | No literal match | no literal match |
| `DecodiumBridgeWorkedHistory.cpp` | No literal match | yes |
| `DecodiumBridgeWorkingFrequencies.cpp` | No literal match | yes |
| `DecodiumBridgeWorldClock.cpp` | No literal match | no literal match |
| `DecodiumBridgeWorldMapCalls.cpp` | `bridge_worldmap_call_helpers_runtime_ab.py` | no literal match |
| `DecodiumBridgeWorldMapConsumers.cpp` | No literal match | yes |
| `DecodiumBridgeWorldMapEntry.cpp` | No literal match | no literal match |
| `DecodiumBridgeWorldMapGrid.cpp` | No literal match | no literal match |
| `DecodiumBridgeWorldMapQso.cpp` | No literal match | no literal match |
| `DecodiumBridgeWorldMapQueue.cpp` | No literal match | no literal match |
| `DecodiumBridgeWorldMapReplay.cpp` | `bridge_worldmapreplay_app_ab.py`, `bridge_worldmapreplay_synthetic_app_ab.py`, `bridge_worldmapreplay_wav_isolated_ab.py`, `bridge_worldmapreplay_wav_unfiltered_ab.py` | no literal match |
| `DecodiumBridgeWorldMapReset.cpp` | No literal match | no literal match |
| `DecodiumBridgeWorldMapSelection.cpp` | No literal match | no literal match |
| `DecodiumBridgeWsprCallback.cpp` | No literal match | no literal match |
