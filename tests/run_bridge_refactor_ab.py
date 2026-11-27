#!/usr/bin/env python3
"""Compare extracted bridge implementations against an explicit Git revision.

Requires a C++17 compiler, pkg-config, and Qt6Core/Qt6Multimedia development files.
Example: python3 tests/run_bridge_refactor_ab.py --base v1.0.737
"""
import argparse
import base64
import hashlib
import os
import re
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(*args, **kwargs):
    return subprocess.run(args, check=True, text=True, **kwargs)


def section(text, start, end=None):
    pos = text.index(start)
    return text[pos : text.index(end, pos) if end else len(text)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--base', default='v1.0.737')
    args = parser.parse_args()
    run(sys.executable, str(ROOT / 'tests/tools/bridge_build_manifest_check.py'), cwd=ROOT)
    run(sys.executable, str(ROOT / 'tests/tools/bridge_mam_slot_message_ab.py'), '--base', args.base, cwd=ROOT)
    run(sys.executable, str(ROOT / 'tests/tools/bridge_sstv_tap_disconnect_ab.py'), cwd=ROOT)
    baseline = run('git', 'show', f'{args.base}:src/bridge/DecodiumBridge.cpp',
                   cwd=ROOT, stdout=subprocess.PIPE).stdout
    # These fixtures preserve the immediate pre-extraction worktree, which may
    # contain changes made after the selected release baseline.
    model_init = (ROOT / 'src/bridge/DecodiumBridgeDecodeModelInit.cpp').read_text()
    model_body = model_init.split('void DecodiumBridge::initialiseDecodeModels()\n{\n', 1)[1]
    if model_body != (ROOT / 'tests/bridge_refactor_fixtures/decode_model_init_before.inc').read_text() + '}\n':
        raise SystemExit('decode model initialization: implementation differs from pre-extraction fixture')
    print('decode model initialization: pre-extraction source equivalence OK')
    rtl_init = (ROOT / 'src/bridge/DecodiumBridgeRtlInit.cpp').read_text()
    rtl_body = rtl_init.split('void DecodiumBridge::initialiseRtlSdr()\n{\n', 1)[1]
    if rtl_body != (ROOT / 'tests/bridge_refactor_fixtures/rtl_init_before.inc').read_text() + '}\n':
        raise SystemExit('RTL initialization: implementation differs from pre-extraction fixture')
    print('RTL initialization: pre-extraction source equivalence OK')
    audio_init = (ROOT / 'src/bridge/DecodiumBridgeAudioDeviceInit.cpp').read_text()
    audio_body = audio_init.split('void DecodiumBridge::initialiseAudioDeviceNotifications()\n{\n', 1)[1]
    if audio_body != (ROOT / 'tests/bridge_refactor_fixtures/audio_device_init_before.inc').read_text() + '}\n':
        raise SystemExit('audio device initialization: implementation differs from pre-extraction fixture')
    print('audio device initialization: pre-extraction source equivalence OK')
    satellite_init = (ROOT / 'src/bridge/DecodiumBridgeSatelliteInit.cpp').read_text()
    satellite_body = satellite_init.split('void DecodiumBridge::initialiseSatelliteTracking()\n{\n', 1)[1]
    if satellite_body != (ROOT / 'tests/bridge_refactor_fixtures/satellite_init_before.inc').read_text() + '}\n':
        raise SystemExit('satellite initialization: implementation differs from pre-extraction fixture')
    print('satellite initialization: pre-extraction source equivalence OK')
    map_init = (ROOT / 'src/bridge/DecodiumBridgeMapInit.cpp').read_text()
    map_body = map_init.split('void DecodiumBridge::initialiseMapIntelligence()\n{\n', 1)[1]
    if map_body != (ROOT / 'tests/bridge_refactor_fixtures/map_init_before.inc').read_text() + '}\n':
        raise SystemExit('map initialization: implementation differs from pre-extraction fixture')
    print('map initialization: pre-extraction source equivalence OK')
    wav_init = (ROOT / 'src/bridge/DecodiumBridgeWavInit.cpp').read_text()
    wav_body = wav_init.split('void DecodiumBridge::initialiseWavManager()\n{\n', 1)[1]
    if wav_body != (ROOT / 'tests/bridge_refactor_fixtures/wav_init_before.inc').read_text() + '}\n':
        raise SystemExit('WAV initialization: implementation differs from pre-extraction fixture')
    print('WAV initialization: pre-extraction source equivalence OK')
    profile_init = (ROOT / 'src/bridge/DecodiumBridgeFtxProfiles.cpp').read_text()
    profile_body = profile_init.split('void DecodiumBridge::initialiseReadyProfileConnections()\n{\n', 1)[1]
    if profile_body != (ROOT / 'tests/bridge_refactor_fixtures/ready_profile_init_before.inc').read_text() + '}\n':
        raise SystemExit('ready profile initialization: implementation differs from pre-extraction fixture')
    print('ready profile initialization: pre-extraction source equivalence OK')
    cluster_init = (ROOT / 'src/bridge/DecodiumBridgeDxClusterInit.cpp').read_text()
    cluster_body = cluster_init.split('void DecodiumBridge::initialiseDxClusterNotifications()\n{\n', 1)[1]
    if cluster_body != (ROOT / 'tests/bridge_refactor_fixtures/dxcluster_init_before.inc').read_text() + '}\n':
        raise SystemExit('DX Cluster initialization: implementation differs from pre-extraction fixture')
    print('DX Cluster initialization: pre-extraction source equivalence OK')
    ntp_init = (ROOT / 'src/bridge/DecodiumBridgeTimeSync.cpp').read_text()
    ntp_body = ntp_init.split('void DecodiumBridge::initialiseTimeSync()\n{\n', 1)[1]
    if ntp_body != (ROOT / 'tests/bridge_refactor_fixtures/ntp_init_before.inc').read_text() + '}\n#undef bridgeLog\n':
        raise SystemExit('time sync initialization: implementation differs from pre-extraction fixture')
    print('time sync initialization: pre-extraction source equivalence OK')
    psk_cloud = (ROOT / 'src/bridge/DecodiumBridgeOnlineLogbook.cpp').read_text()
    psk_cloud_body = psk_cloud.split('void DecodiumBridge::initialisePskAndCloudlog()\n{\n', 1)[1]
    if not psk_cloud_body.startswith((ROOT / 'tests/bridge_refactor_fixtures/psk_cloudlog_init_before.inc').read_text() + '}\n#undef bridgeLog\n'):
        raise SystemExit('PSK and Cloudlog initialization: implementation differs from pre-extraction fixture')
    print('PSK and Cloudlog initialization: pre-extraction source equivalence OK')
    qrz_wspr_body = psk_cloud.split('void DecodiumBridge::initialiseQrzAndWspr()\n{\n', 1)[1]
    if qrz_wspr_body != (ROOT / 'tests/bridge_refactor_fixtures/qrz_wspr_init_before.inc').read_text() + '}\n':
        raise SystemExit('QRZ and WSPR initialization: implementation differs from pre-extraction fixture')
    print('QRZ and WSPR initialization: pre-extraction source equivalence OK')
    decolog_init = (ROOT / 'src/bridge/DecodiumBridgeDecoLogInit.cpp').read_text()
    decolog_body = decolog_init.split('void DecodiumBridge::initialiseDecoLogLink()\n{\n', 1)[1]
    if decolog_body != (ROOT / 'tests/bridge_refactor_fixtures/decolog_init_before.inc').read_text() + '}\n':
        raise SystemExit('DecoLog initialization: implementation differs from pre-extraction fixture')
    print('DecoLog initialization: pre-extraction source equivalence OK')
    web_init = (ROOT / 'src/bridge/DecodiumBridgeRemoteWebInit.cpp').read_text()
    web_body = web_init.split('void DecodiumBridge::initialiseRemoteWebConsole()\n{\n', 1)[1]
    if web_body != (ROOT / 'tests/bridge_refactor_fixtures/remote_web_init_before.inc').read_text() + '}\n':
        raise SystemExit('remote web initialization: implementation differs from pre-extraction fixture')
    print('remote web initialization: pre-extraction source equivalence OK')
    callsign_init = (ROOT / 'src/bridge/DecodiumBridgeCallsignInit.cpp').read_text()
    callsign_body = callsign_init.split('void DecodiumBridge::initialiseCallsignIntelligence()\n{\n', 1)[1]
    if callsign_body != (ROOT / 'tests/bridge_refactor_fixtures/callsign_init_before.inc').read_text() + '}\n':
        raise SystemExit('callsign initialization: implementation differs from pre-extraction fixture')
    print('callsign initialization: pre-extraction source equivalence OK')
    usstate_init = (ROOT / 'src/bridge/DecodiumBridgeUsStateLotw.cpp').read_text()
    usstate_body = usstate_init.split('void DecodiumBridge::initialiseUsStateAndWsprNotifications()\n{\n', 1)[1]
    if usstate_body != (ROOT / 'tests/bridge_refactor_fixtures/usstate_wspr_init_before.inc').read_text() + '}\n#undef bridgeLog\n':
        raise SystemExit('US state and WSPR initialization: implementation differs from pre-extraction fixture')
    print('US state and WSPR initialization: pre-extraction source equivalence OK')
    mac_legacy_init = (ROOT / 'src/bridge/DecodiumBridgeLegacyInit.cpp').read_text()
    mac_legacy_body = mac_legacy_init.split('void DecodiumBridge::initialiseMacLegacyStartup()\n{\n', 1)[1]
    if mac_legacy_body != (ROOT / 'tests/bridge_refactor_fixtures/mac_legacy_start_before.inc').read_text() + '}\n#undef bridgeLog\n':
        raise SystemExit('macOS legacy startup: implementation differs from pre-extraction fixture')
    print('macOS legacy startup: pre-extraction source equivalence OK')
    cat_init = (ROOT / 'src/bridge/DecodiumBridgeCatConnections.cpp').read_text()
    cat_body = cat_init.split('void DecodiumBridge::initialiseCatConnections()\n{\n', 1)[1]
    if cat_body != (ROOT / 'tests/bridge_refactor_fixtures/cat_connections_before.inc').read_text() + '}\n':
        raise SystemExit('CAT connections: implementation differs from pre-extraction fixture')
    print('CAT connections: pre-extraction source equivalence OK')
    cat_telemetry = (ROOT / 'src/bridge/DecodiumBridgeCatTelemetry.cpp').read_text()
    telemetry_body = cat_telemetry.split('void DecodiumBridge::initialiseCatTelemetry()\n{\n', 1)[1]
    if telemetry_body != (ROOT / 'tests/bridge_refactor_fixtures/cat_telemetry_init_before.inc').read_text() + '}\n':
        raise SystemExit('CAT telemetry: implementation differs from pre-extraction fixture')
    print('CAT telemetry: pre-extraction source equivalence OK')
    diagnostics = (ROOT / 'src/bridge/DecodiumBridgeDiagnosticConnections.cpp').read_text()
    diagnostics_body = diagnostics.split('void DecodiumBridge::initialiseDiagnosticConnections()\n{\n', 1)[1]
    if diagnostics_body != (ROOT / 'tests/bridge_refactor_fixtures/diagnostic_connections_before.inc').read_text() + '}\n':
        raise SystemExit('diagnostic connections: implementation differs from pre-extraction fixture')
    print('diagnostic connections: pre-extraction source equivalence OK')
    decode_cluster = (ROOT / 'src/bridge/DecodiumBridgeDecodeClusterConnections.cpp').read_text()
    decode_cluster_body = decode_cluster.split('void DecodiumBridge::initialiseDecodeClusterConnections()\n{\n', 1)[1]
    if decode_cluster_body != (ROOT / 'tests/bridge_refactor_fixtures/decode_cluster_connections_before.inc').read_text() + '}\n':
        raise SystemExit('decode and cluster connections: implementation differs from pre-extraction fixture')
    print('decode and cluster connections: pre-extraction source equivalence OK')
    sstv_choices = (ROOT / 'src/bridge/DecodiumBridgeSstvQsoChoices.cpp').read_text()
    choices_body = section(sstv_choices, 'QVariantList DecodiumBridge::sstvExistingQsoChoices(')
    if choices_body != (ROOT / 'tests/bridge_refactor_fixtures/sstv_qso_choices_before.inc').read_text():
        raise SystemExit('SSTV QSO choices: implementation differs from pre-extraction fixture')
    print('SSTV QSO choices: pre-extraction source equivalence OK')
    sstv_reset = (ROOT / 'src/bridge/DecodiumBridgeSstvRxReset.cpp').read_text()
    reset_body = section(sstv_reset, 'bool DecodiumBridge::resetSstvRx()', 'bool DecodiumBridge::updateSstvRxControls(')
    expected_reset = (ROOT / 'tests/bridge_refactor_fixtures/sstv_rx_reset_before.inc').read_text() + (ROOT / 'tests/bridge_refactor_fixtures/sstv_rx_adjust_reset_before.inc').read_text()
    if reset_body != expected_reset:
        raise SystemExit('SSTV RX reset: implementation differs from pre-extraction fixture')
    print('SSTV RX reset: pre-extraction source equivalence OK')
    controls_body = section(sstv_reset, 'bool DecodiumBridge::updateSstvRxControls(')
    if controls_body != (ROOT / 'tests/bridge_refactor_fixtures/sstv_rx_controls_before.inc').read_text():
        raise SystemExit('SSTV RX controls: implementation differs from pre-extraction fixture')
    print('SSTV RX controls: pre-extraction source equivalence OK')
    sstv_replay = (ROOT / 'src/bridge/DecodiumBridgeSstvWavReplay.cpp').read_text()
    replay_body = section(sstv_replay, 'bool DecodiumBridge::startSstvWavReplay(')
    if replay_body != (ROOT / 'tests/bridge_refactor_fixtures/sstv_wav_replay_before.inc').read_text():
        raise SystemExit('SSTV WAV replay: implementation differs from pre-extraction fixture')
    print('SSTV WAV replay: pre-extraction source equivalence OK')
    sstv_raw = (ROOT / 'src/bridge/DecodiumBridgeSstvRawAudio.cpp').read_text()
    raw_body = section(sstv_raw, 'bool DecodiumBridge::saveSstvRxRawAudio()')
    if raw_body != (ROOT / 'tests/bridge_refactor_fixtures/sstv_raw_audio_before.inc').read_text():
        raise SystemExit('SSTV raw audio: implementation differs from pre-extraction fixture')
    print('SSTV raw audio: pre-extraction source equivalence OK')
    sstv_redecode = (ROOT / 'src/bridge/DecodiumBridgeSstvRedecode.cpp').read_text()
    redecode_body = section(sstv_redecode, 'bool DecodiumBridge::redecodeRecentSstv(')
    if redecode_body != (ROOT / 'tests/bridge_refactor_fixtures/sstv_redecode_before.inc').read_text():
        raise SystemExit('SSTV re-decode: implementation differs from pre-extraction fixture')
    print('SSTV re-decode: pre-extraction source equivalence OK')
    sstv_diagnostics = (ROOT / 'src/bridge/DecodiumBridgeSstvRxDiagnostics.cpp').read_text()
    diagnostics_body = section(sstv_diagnostics, 'QVariantMap DecodiumBridge::sstvRxDiagnostics() const')
    if diagnostics_body != (ROOT / 'tests/bridge_refactor_fixtures/sstv_rx_diagnostics_before.inc').read_text():
        raise SystemExit('SSTV RX diagnostics: implementation differs from pre-extraction fixture')
    print('SSTV RX diagnostics: pre-extraction source equivalence OK')
    sstv_snapshot = (ROOT / 'src/bridge/DecodiumBridgeSstvDiagnosticsSnapshot.cpp').read_text()
    snapshot_body = section(sstv_snapshot, 'void DecodiumBridge::refreshSstvDiagnosticsSnapshot()')
    if snapshot_body != (ROOT / 'tests/bridge_refactor_fixtures/sstv_diagnostics_snapshot_before.inc').read_text():
        raise SystemExit('SSTV diagnostics snapshot: implementation differs from pre-extraction fixture')
    print('SSTV diagnostics snapshot: pre-extraction source equivalence OK')
    sstv_status = (ROOT / 'src/bridge/DecodiumBridgeSstvStatus.cpp').read_text()
    status_body = section(sstv_status, 'bool DecodiumBridge::sstvAvailable() const', '#if DECODIUM_HAS_SSTV\nvoid DecodiumBridge::setSstvThumbnailProvider(')
    if status_body != (ROOT / 'tests/bridge_refactor_fixtures/sstv_status_before.inc').read_text():
        raise SystemExit('SSTV status: implementation differs from pre-extraction fixture')
    print('SSTV status: pre-extraction source equivalence OK')
    studio_status = section(sstv_status, '#if DECODIUM_HAS_SSTV\nvoid DecodiumBridge::setSstvThumbnailProvider(')
    if studio_status != (ROOT / 'tests/bridge_refactor_fixtures/sstv_studio_status_before.inc').read_text():
        raise SystemExit('SSTV Studio status: implementation differs from pre-extraction fixture')
    print('SSTV Studio status: pre-extraction source equivalence OK')
    sstv_save = (ROOT / 'src/bridge/DecodiumBridgeSstvImageSave.cpp').read_text()
    save_fixture = (ROOT / 'tests/bridge_refactor_fixtures/sstv_image_save_before.inc').read_text()
    if not sstv_save.endswith(save_fixture):
        raise SystemExit('SSTV image save: implementation differs from pre-extraction fixture')
    print('SSTV image save: pre-extraction source equivalence OK')
    sstv_studio_archive = (ROOT / 'src/bridge/DecodiumBridgeSstvStudioArchive.cpp').read_text()
    studio_fixture = (ROOT / 'tests/bridge_refactor_fixtures/sstv_studio_archive_before.inc').read_text()
    if sstv_studio_archive.count(studio_fixture) != 1:
        raise SystemExit('SSTV Studio archive: implementation differs from pre-extraction fixture')
    print('SSTV Studio archive: pre-extraction source equivalence OK')
    hamdrm_archive_fixture = (ROOT / 'tests/bridge_refactor_fixtures/hamdrm_gallery_archive_before.inc').read_text()
    if not sstv_studio_archive.endswith(hamdrm_archive_fixture):
        raise SystemExit('HAMDRM Gallery archive: implementation differs from pre-extraction fixture')
    print('HAMDRM Gallery archive: pre-extraction source equivalence OK')
    linux_audio = (ROOT / 'src/bridge/DecodiumBridgeLinuxAudioHelpers.cpp').read_text()
    linux_audio_fixture = (ROOT / 'tests/bridge_refactor_fixtures/linux_audio_helpers_before.inc').read_text()
    if not linux_audio.endswith(linux_audio_fixture):
        raise SystemExit('Linux audio helpers: implementation differs from pre-extraction fixture')
    print('Linux audio helpers: pre-extraction source equivalence OK')
    qso_assoc = (ROOT / 'src/bridge/DecodiumBridgeSstvQsoAssociation.cpp').read_text()
    if not qso_assoc.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_qso_association_before.inc').read_text()):
        raise SystemExit('SSTV QSO association differs from pre-extraction fixture')
    print('SSTV QSO association: pre-extraction source equivalence OK')
    queue_module = (ROOT / 'src/bridge/DecodiumBridgeSstvStorageQueue.cpp').read_text()
    if not queue_module.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_storage_queue_before.inc').read_text()):
        raise SystemExit('SSTV storage queue differs from pre-extraction fixture')
    print('SSTV storage queue: pre-extraction source equivalence OK')
    storage_shutdown = (ROOT / 'src/bridge/DecodiumBridgeSstvStorageShutdown.cpp').read_text()
    if not storage_shutdown.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_storage_shutdown_before.inc').read_text()):
        raise SystemExit('SSTV storage shutdown differs from pre-extraction fixture')
    print('SSTV storage shutdown: pre-extraction source equivalence OK')
    runtime_shutdown = (ROOT / 'src/bridge/DecodiumBridgeSstvRuntimeShutdown.cpp').read_text()
    if not runtime_shutdown.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_runtime_shutdown_before.inc').read_text()):
        raise SystemExit('SSTV runtime shutdown differs from pre-extraction fixture')
    print('SSTV runtime shutdown: pre-extraction source equivalence OK')
    workspace = (ROOT / 'src/bridge/DecodiumBridgeSstvWorkspace.cpp').read_text()
    if not workspace.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_workspace_before.inc').read_text()):
        raise SystemExit('SSTV workspace differs from pre-extraction fixture')
    print('SSTV workspace: pre-extraction source equivalence OK')
    rx_stop = (ROOT / 'src/bridge/DecodiumBridgeSstvRxStop.cpp').read_text()
    if not rx_stop.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_rx_stop_before.inc').read_text()):
        raise SystemExit('SSTV RX stop differs from pre-extraction fixture')
    print('SSTV RX stop: pre-extraction source equivalence OK')
    rx_start = (ROOT / 'src/bridge/DecodiumBridgeSstvRxStart.cpp').read_text()
    if not rx_start.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_rx_start_before.inc').read_text()):
        raise SystemExit('SSTV RX start differs from pre-extraction fixture')
    print('SSTV RX start: pre-extraction source equivalence OK')
    rx_route = (ROOT / 'src/bridge/DecodiumBridgeSstvRxSourceRoute.cpp').read_text()
    if not rx_route.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_rx_source_route_before.inc').read_text()):
        raise SystemExit('SSTV RX source routing differs from pre-extraction fixture')
    print('SSTV RX source routing: pre-extraction source equivalence OK')
    tx_lifecycle = (ROOT / 'src/bridge/DecodiumBridgeSstvTxLifecycle.cpp').read_text()
    if not tx_lifecycle.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_tx_lifecycle_before.inc').read_text()):
        raise SystemExit('SSTV TX lifecycle differs from pre-extraction fixture')
    print('SSTV TX lifecycle: pre-extraction source equivalence OK')
    tx_start = (ROOT / 'src/bridge/DecodiumBridgeSstvTxStart.cpp').read_text().replace('decodiumSstvOutputMatchesConfiguredSelection(', 'sstvOutputMatchesConfiguredSelection(')
    if not tx_start.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_tx_start_before.inc').read_text()):
        raise SystemExit('SSTV TX start differs from pre-extraction fixture')
    print('SSTV TX start: pre-extraction source equivalence OK')
    radiofax_tx = (ROOT / 'src/bridge/DecodiumBridgeSstvRadiofaxTx.cpp').read_text().replace('decodiumSstvOutputMatchesConfiguredSelection(', 'sstvOutputMatchesConfiguredSelection(')
    if not radiofax_tx.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_radiofax_tx_before.inc').read_text()):
        raise SystemExit('SSTV Radiofax TX differs from pre-extraction fixture')
    print('SSTV Radiofax TX: pre-extraction source equivalence OK')
    fax480_tx = (ROOT / 'src/bridge/DecodiumBridgeSstvFax480Tx.cpp').read_text().replace('decodiumSstvOutputMatchesConfiguredSelection(', 'sstvOutputMatchesConfiguredSelection(')
    if not fax480_tx.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_fax480_tx_before.inc').read_text()):
        raise SystemExit('SSTV FAX480 TX differs from pre-extraction fixture')
    print('SSTV FAX480 TX: pre-extraction source equivalence OK')
    calibration_tx = (ROOT / 'src/bridge/DecodiumBridgeSstvCalibrationTx.cpp').read_text().replace('decodiumSstvOutputMatchesConfiguredSelection(', 'sstvOutputMatchesConfiguredSelection(')
    if not calibration_tx.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_calibration_tx_before.inc').read_text()):
        raise SystemExit('SSTV calibration TX differs from pre-extraction fixture')
    print('SSTV calibration TX: pre-extraction source equivalence OK')
    hamdrm_tx = (ROOT / 'src/bridge/DecodiumBridgeSstvHamDrmTx.cpp').read_text().replace('decodiumSstvOutputMatchesConfiguredSelection(', 'sstvOutputMatchesConfiguredSelection(')
    if not hamdrm_tx.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_hamdrm_tx_before.inc').read_text()):
        raise SystemExit('SSTV HAMDRM TX differs from pre-extraction fixture')
    print('SSTV HAMDRM TX: pre-extraction source equivalence OK')
    tx_init = (ROOT / 'src/bridge/DecodiumBridgeSstvTxInit.cpp').read_text().replace('decodiumSstvTerminalTxState(', 'terminalSstvTxState(')
    if not tx_init.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_tx_init_before.inc').read_text()):
        raise SystemExit('SSTV TX initialization differs from pre-extraction fixture')
    print('SSTV TX initialization: pre-extraction source equivalence OK')
    sstv_source = (ROOT / 'src/bridge/DecodiumBridgeSstvAudioSource.cpp').read_text()
    if not sstv_source.endswith((ROOT / 'tests/bridge_refactor_fixtures/sstv_audio_source_before.inc').read_text()) or (ROOT / 'tests/bridge_refactor_fixtures/sstv_stream_id_before.inc').read_text() not in sstv_source:
        raise SystemExit('SSTV audio source differs from pre-extraction fixture')
    print('SSTV audio source: pre-extraction source equivalence OK')
    timers_init = (ROOT / 'src/bridge/DecodiumBridgeCoreTimers.cpp').read_text()
    timers_body = timers_init.split('void DecodiumBridge::initialiseCoreTimers()\n{\n', 1)[1]
    if timers_body != (ROOT / 'tests/bridge_refactor_fixtures/core_timers_init_before.inc').read_text() + '}\n':
        raise SystemExit('core timers: implementation differs from pre-extraction fixture')
    print('core timers: pre-extraction source equivalence OK')
    for label, source_name, fixture_name, signature in (
        ('auto-sequencer', 'DecodiumBridgeAutoSequence.cpp', 'auto_sequence_before.inc',
         'void DecodiumBridge::autoSequenceStep('),
        ('display frequency', 'DecodiumBridgeDisplayFrequency.cpp', 'display_frequency_before.inc',
         'double DecodiumBridge::frequency()'),
        ('Fox OTP', 'DecodiumBridgeFoxVerifier.cpp', 'verify_otp_before.inc',
         'void DecodiumBridge::verifyOtp('),
        ('QSO logging', 'DecodiumBridgeLogQso.cpp', 'log_qso_now_before.inc',
         'void DecodiumBridge::logQsoNow()'),
        ('shutdown', 'DecodiumBridgeShutdown.cpp', 'bridge_shutdown_before.inc',
         'void DecodiumBridge::shutdown()'),
        ('modulator finish', 'DecodiumBridgeModulatorFinish.cpp', 'modulator_idle_before.inc',
         'void DecodiumBridge::finishModulatorIdlePlayback('),
        ('legacy bridge audio', 'DecodiumBridgeLegacyBridgeAudio.cpp', 'legacy_bridge_audio_before.inc',
         'bool DecodiumBridge::startBridgeAudioForLegacyDigitalTx('),
        ('legacy preflight', 'DecodiumBridgeLegacyPreflight.cpp', 'legacy_bridge_preflight_before.inc',
         'bool DecodiumBridge::preflightLegacyBridgeTxBeforePtt('),
        ('complete TX playback', 'DecodiumBridgeCompleteTxPlayback.cpp', 'complete_tx_playback_before.inc',
         'void DecodiumBridge::completeTxPlayback('),
        ('start TX', 'DecodiumBridgeStartTx.cpp', 'start_tx_before.inc',
         'void DecodiumBridge::startTx()'),
        ('stop TX', 'DecodiumBridgeStopTx.cpp', 'stop_tx_before.inc',
         'void DecodiumBridge::stopTx()'),
        ('TX output device', 'DecodiumBridgeTxOutputDevice.cpp', 'tx_output_device_before.inc',
         'void DecodiumBridge::updateSoundOutputDevice()'),
        ('Tune', 'DecodiumBridgeTune.cpp', 'tune_before.inc',
         'void DecodiumBridge::startTune()'),
    ):
        actual = section((ROOT / 'src/bridge' / source_name).read_text(), signature)
        expected = (ROOT / 'tests/bridge_refactor_fixtures' / fixture_name).read_text()
        if label == 'modulator finish':
            cw_fix = '''    // CW is bridge-owned even when the legacy backend supplies other modes.
    // Clear its ownership before publishing TX=false, otherwise legacy state
    // synchronisation can immediately reassert TX after playback has ended.
    if (m_cwTxActive) {
        m_cwTxActive = false;
        m_pendingCwText.clear();
    }
'''
            anchor = '    noteTxPlaybackFinished(reason, false);\n'
            if expected.count(anchor) != 1:
                raise SystemExit('modulator finish CW correction anchor missing')
            expected = expected.replace(anchor, cw_fix + anchor, 1)
        if label == 'start TX':
            original_preflight = (ROOT / 'tests/bridge_refactor_fixtures/start_tx_preflight_before.inc').read_text()
            helper = (ROOT / 'src/bridge/DecodiumBridgeStartTxPreflight.cpp').read_text()
            helper_body = section(helper, 'bool DecodiumBridge::startTxPreflight()\n{')
            expected_helper = 'bool DecodiumBridge::startTxPreflight()\n{\n' + original_preflight.replace('return;', 'return false;') + '    return true;\n}\n'
            if helper_body != expected_helper:
                raise SystemExit('start TX preflight differs from pre-extraction fixture')
            actual = actual.replace('    if (!startTxPreflight()) return;\n\n', original_preflight, 1)
            selection_lambda = (ROOT / 'tests/bridge_refactor_fixtures/start_tx_selection_lambda_before.inc').read_text()
            selection_guard = (ROOT / 'tests/bridge_refactor_fixtures/start_tx_selection_guard_before.inc').read_text()
            selection_source = (ROOT / 'src/bridge/DecodiumBridgeStartTxSelection.cpp').read_text()
            selection_body = section(selection_source, 'bool DecodiumBridge::prepareStartTxSelection(bool customAudioTxActive)\n{')
            expected_selection = 'bool DecodiumBridge::prepareStartTxSelection(bool customAudioTxActive)\n{\n' + selection_lambda + selection_guard.replace('return;', 'return false;') + '    return true;\n}\n'
            if selection_body != expected_selection:
                raise SystemExit('start TX selection differs from pre-extraction fixture')
            actual = actual.replace('    if (!prepareStartTxSelection(customAudioTxActive)) return;\n\n', selection_guard, 1)
            actual = actual.replace('    bridgeLog("startTx() called:', selection_lambda + '    bridgeLog("startTx() called:', 1)
        if actual != expected:
            raise SystemExit(f'{label}: implementation differs from pre-extraction fixture')
        print(f'{label}: pre-extraction source equivalence OK')
    clock_start = 'static qint64 bridgeMonotonicMs()'
    old_clock = section(baseline, clock_start, '// Registro dei QSO:')
    metric_source = (ROOT / 'src/bridge/DecodiumBridgeDecodeMetrics.cpp').read_text()
    new_clock = section(metric_source, clock_start,
                        'namespace {\nstd::atomic<qint64> g_lastDecodeReadySlotStartMs')
    if old_clock != new_clock:
        raise SystemExit(f'metrics clock: implementation differs from {args.base}')
    utc_start = 'QTimeZone decodiumUtcTimeZone()'
    original_utc = section(baseline, utc_start, '\n}\n') + '\n}'
    world_clock_source = (ROOT / 'src/bridge/DecodiumBridgeWorldClock.cpp').read_text()
    extracted_utc = section(world_clock_source, utc_start, '\n}\n') + '\n}'
    if original_utc != extracted_utc:
        raise SystemExit(f'UTC helper: implementation differs from {args.base}')
    variants = {
        'metrics': (
            section(baseline, 'namespace {\nstd::atomic<qint64> g_lastDecodeReadySlotStartMs',
                    'static QStringList ctyDatDownloadUrls()'),
            section((ROOT / 'src/bridge/DecodiumBridgeDecodeMetrics.cpp').read_text(),
                    'namespace {\nstd::atomic<qint64> g_lastDecodeReadySlotStartMs',
                    '\nvoid DecodiumBridge::recordFrameTimestamp()')),
        'grid': (
            section(baseline, 'double DecodiumBridge::calcDistance(',
                    '// ============================================================\n// C15 — QSY:'),
            section((ROOT / 'src/bridge/DecodiumBridgeGrid.cpp').read_text(),
                    'double DecodiumBridge::calcDistance(')),
        'online_logbook': (
            section(baseline, 'void DecodiumBridge::setCloudlogEnabled(',
                    '// === ADV AUTO-MODE ==='),
            section((ROOT / 'src/bridge/DecodiumBridgeOnlineLogbook.cpp').read_text(),
                    'void DecodiumBridge::setCloudlogEnabled(',
                    'void decodiumBridgeDecodeFlowLog(const QString&);').removesuffix('\n')),
        'world_clock': (
            section(baseline, 'QVariantMap DecodiumBridge::worldClockSnapshot(',
                    '// ── catBackend switch'),
            section((ROOT / 'src/bridge/DecodiumBridgeWorldClock.cpp').read_text(),
                    'QVariantMap DecodiumBridge::worldClockSnapshot(')),
        'fileio': (
            section(baseline, 'QVariantMap DecodiumBridge::readTextFile(',
                    'QString DecodiumBridge::defaultFt2LinkReceivedFilesDirectory()'),
            section((ROOT / 'src/bridge/DecodiumBridgeFileIO.cpp').read_text(),
                    'QVariantMap DecodiumBridge::readTextFile(')),
    }
    if 'static constexpr bool kDecodiumUpdateCheckerEnabled = false;' not in baseline or 'static constexpr bool kDecodiumUpdateCheckerEnabled = false;' not in (ROOT / 'src/bridge/DecodiumBridgeUpdates.cpp').read_text():
        raise SystemExit('update checker build flag changed')
    variants['updates'] = (
        section(baseline, 'void DecodiumBridge::checkForUpdates()', '// ============================================================\n// C17'),
        section((ROOT / 'src/bridge/DecodiumBridgeUpdates.cpp').read_text(), 'void DecodiumBridge::checkForUpdates()'))
    old_web = section(baseline, 'namespace\n{\nQString generateWebServerAccessToken()', 'bool DecodiumBridge::entryBelongsToCurrentQso(')
    new_web = section((ROOT / 'src/bridge/DecodiumBridgeWebServer.cpp').read_text(), 'namespace\n{\nQString generateWebServerAccessToken()').replace('decodiumBridgeWebServerLog(', 'bridgeLog(')
    if old_web != new_web:
        raise SystemExit('web server bridge implementation differs from baseline')
    print('web server bridge: A/B source equivalence OK')
    old_cpu = (section(baseline, 'void DecodiumBridge::setFtThreads(int v)', 'qint64 DecodiumBridge::correctedUtcEpochMs()')
               + section(baseline, 'void DecodiumBridge::setLowCpuModeEnabled(bool enabled)', '// ---------------------------------------------------------------------------\n// Worked-before tracking'))
    new_cpu = (section((ROOT / 'src/bridge/DecodiumBridgeCpuProfile.cpp').read_text(), 'void DecodiumBridge::setFtThreads(int v)', '\n// 1.0.388 — priorità processo')
               .replace('decodiumBridgeMaxFtDecodeThreads()', 'kMaxFtDecodeThreads')
               .replace('decodiumBridgeFtAutoUiReserveForCores(', 'ftAutoUiReserveForCores(')
               .replace('decodiumBridgeAutoFtThreadCountForCores(', 'autoFtThreadCountForCores(')
               .replace('decodiumBridgeCpuProfileLog(', 'bridgeLog('))
    variants['cpuprofile'] = (old_cpu, new_cpu)
    for start, end in (('int ftAutoUiReserveForCores(int cores)\n{', 'int autoFtThreadCountForCores(int cores)\n{'),
                       ('int autoFtThreadCountForCores(int cores)\n{', 'int adaptiveInteractiveFtThreadCount(int normalLimit)')):
        if section(baseline, start, end) != section((ROOT / 'src/bridge/DecodiumBridgeFtThreadBudget.cpp').read_text(), start, end):
            raise SystemExit('CPU helper differs from baseline: ' + start)
    if 'constexpr int kMaxFtDecodeThreads {24};' not in baseline or 'constexpr int kMaxFtDecodeThreads {24};' not in (ROOT / 'src/core/FtDecodeThreadBudget.hpp').read_text():
        raise SystemExit('CPU thread limit differs from baseline')
    old_partner = section(baseline, 'void DecodiumBridge::setResumeQsoOnReply(bool v)', '// 1.0.179 — Smooth Decode Flow setter.')
    new_partner = section((ROOT / 'src/bridge/DecodiumBridgePartnerMemory.cpp').read_text(), 'void DecodiumBridge::setResumeQsoOnReply(bool v)').replace('decodiumBridgePartnerMemoryLog(', 'bridgeLog(')
    variants['partner'] = (old_partner, new_partner)
    canonical_start = 'static QString canonicalApplicationDecodeMode(QString mode)'
    current_bridge = (ROOT / 'src/bridge/DecodiumBridge.cpp').read_text()
    if section(baseline, canonical_start, '\n}\n') != section((ROOT / 'src/bridge/DecodiumBridgeApplicationModes.cpp').read_text(), canonical_start, '\n}\n'):
        raise SystemExit('canonical mode helper differs from baseline')
    old_workers = section(baseline, 'void DecodiumBridge::ensureDecodeWorkerForMode(', 'void DecodiumBridge::loadDxccLookupAsync()')
    new_workers = section((ROOT / 'src/bridge/DecodiumBridgeDecodeWorkers.cpp').read_text(), 'void DecodiumBridge::ensureDecodeWorkerForMode(').replace('decodiumBridgeCanonicalApplicationDecodeMode(', 'canonicalApplicationDecodeMode(').replace('decodiumBridgeWorkerLog(', 'bridgeLog(')
    if old_workers != new_workers:
        raise SystemExit('decode worker initialization differs from baseline')
    print('decode worker initialization: A/B source equivalence OK')
    if section(baseline, 'static bool isFtx77MessageMode(', 'static QString normalizedTxCallToken(') != section((ROOT / 'src/bridge/FtxMessageHelpers.cpp').read_text(), 'bool isFtx77MessageMode(', 'static QString normalizedTxCallToken(').replace('bool isFtx77MessageMode(', 'static bool isFtx77MessageMode(', 1):
        raise SystemExit('FTX mode helper differs from baseline')
    old_backlog = section(baseline, 'bool DecodiumBridge::ft8LiveDecodeBacklogActive(', 'void DecodiumBridge::requestRigFrequencyFromBridge(')
    new_backlog = section((ROOT / 'src/bridge/DecodiumBridgeDecodeBacklog.cpp').read_text(), 'bool DecodiumBridge::ft8LiveDecodeBacklogActive(').replace('decodiumBridgeDecodeBacklogLog(', 'bridgeLog(').replace('decodiumBridgeIsFtx77MessageMode(', 'isFtx77MessageMode(')
    if old_backlog != new_backlog:
        raise SystemExit('decode backlog implementation differs from baseline')
    print('decode backlog: A/B source equivalence OK')
    for helper in ('static QString decodeMirrorEntryKey(', 'static QString decodeRxClearEntryKey('):
        current_helper = section((ROOT / 'src/bridge/DecodiumBridgeDecodeMirrorKeys.cpp').read_text(), helper, '\n}\n').replace('decodiumBridgeNormalizedVisualTxTimeToken(', 'normalizedVisualTxTimeToken(')
        if section(baseline, helper, '\n}\n') != current_helper:
            raise SystemExit('decode clear key helper differs from baseline: ' + helper)
    old_clear = section(baseline, 'void DecodiumBridge::clearDecodeList()', 'void DecodiumBridge::clearRemoteActivityCache(')
    new_clear = section((ROOT / 'src/bridge/DecodiumBridgeDecodeClear.cpp').read_text(), 'void DecodiumBridge::clearDecodeList()', 'void DecodiumBridge::primeLegacyAllTxtCursor()').replace('decodiumBridgeDecodeClearLog(', 'bridgeLog(').replace('decodiumBridgeDecodeRxClearEntryKey(', 'decodeRxClearEntryKey(').replace('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey(')
    if old_clear != new_clear:
        raise SystemExit('decode clear implementation differs from baseline')
    print('decode clear: A/B source equivalence OK')
    if section(baseline, 'void rebuildWorkedCallsFromDocument(', '\n}\n') != section((ROOT / 'src/bridge/DecodiumAdifSupport.cpp').read_text(), 'void rebuildWorkedCallsFromDocument(', '\n}\n'):
        raise SystemExit('worked-call rebuild helper differs from baseline')
    old_history = section(baseline, 'void DecodiumBridge::loadDxccLookupAsync()', 'void DecodiumBridge::notifyMainQmlReady()')
    new_history = section((ROOT / 'src/bridge/DecodiumBridgeAsyncHistory.cpp').read_text(), 'void DecodiumBridge::loadDxccLookupAsync()').replace('decodiumBridgeAsyncHistoryLog(', 'bridgeLog(')
    if old_history != new_history:
        raise SystemExit('asynchronous DXCC/history implementation differs from baseline')
    print('asynchronous DXCC/history: A/B source equivalence OK')
    old_pressure = (section(baseline, 'bool DecodiumBridge::cpuPressureActive() const', 'int DecodiumBridge::effectiveFt4ThreadLimit() const')
                    + section(baseline, 'int DecodiumBridge::effectiveSpectrumTimerIntervalMs() const', 'int DecodiumBridge::effectiveDecodeDepth() const'))
    new_pressure = section((ROOT / 'src/bridge/DecodiumBridgeCpuPressure.cpp').read_text(), 'bool DecodiumBridge::cpuPressureActive() const')
    for new, old in (('decodiumBridgeMaxFtDecodeThreads()', 'kMaxFtDecodeThreads'), ('decodiumBridgeFtAutoUiReserveForCores(', 'ftAutoUiReserveForCores('), ('decodiumBridgeAutoFtThreadCountForCores(', 'autoFtThreadCountForCores('), ('decodiumBridgeAdaptiveInteractiveFtThreadCount(', 'adaptiveInteractiveFtThreadCount('), ('decodiumBridgeApplyFtOpenMpThreadLimit(', 'applyFtOpenMpThreadLimit('), ('decodiumBridgeCpuPressureLog(', 'bridgeLog(')):
        new_pressure = new_pressure.replace(new, old)
    if old_pressure != new_pressure:
        raise SystemExit('CPU pressure implementation differs from baseline')
    print('CPU pressure: A/B source equivalence OK')
    old_context = section(baseline, 'void DecodiumBridge::primeLegacyAllTxtCursor()', 'void DecodiumBridge::queueLegacyDecodeSecondaryWork(')
    new_context = section((ROOT / 'src/bridge/DecodiumBridgeDecodeClear.cpp').read_text(), 'void DecodiumBridge::primeLegacyAllTxtCursor()').replace('decodiumBridgeDecodeClearLog(', 'bridgeLog(').replace('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey(')
    if old_context != new_context:
        raise SystemExit('decode context reset differs from baseline')
    print('decode context reset: A/B source equivalence OK')
    old_flow = (section(baseline, 'void DecodiumBridge::setSmoothDecodeFlow(bool v)', 'QString DecodiumBridge::uiQuality() const')
                + section(baseline, 'void DecodiumBridge::trimDecodeListsIfNeeded()', 'void DecodiumBridge::persistTxWatchdogSettings()'))
    new_flow = section((ROOT / 'src/bridge/DecodiumBridgeDecodeFlow.cpp').read_text(), 'void DecodiumBridge::setSmoothDecodeFlow(bool v)')
    for new, old in (('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey('), ('decodiumBridgeDecodeRxClearEntryKey(', 'decodeRxClearEntryKey('), ('decodiumBridgeNormalizeDecodeEntries(', 'normalizeDecodeEntriesForDisplay('), ('decodiumBridgeCoalesceRxPaneTxRows(', 'coalesceRxPaneTxRows('), ('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')):
        new_flow = new_flow.replace(new, old)
    if old_flow != new_flow:
        raise SystemExit('decode flow implementation differs from baseline')
    old_trace = section(baseline, 'class MainThreadTraceScope\n', '\nnamespace {')
    new_trace = section((ROOT / 'src/bridge/DecodiumBridgeTraceScope.h').read_text(), 'class MainThreadTraceScope\n').replace('decodiumBridgeMainThreadTimelineLog(', 'mainThreadTimelineLog(')
    if old_trace.strip() != new_trace.strip():
        raise SystemExit('main thread trace scope differs from baseline')
    print('decode flow and shared trace scope: A/B source equivalence OK')
    old_teleprinter = section(baseline, 'void DecodiumBridge::aggiungiRigaRtty (', 'void DecodiumBridge::appendDecodeMapToList(')
    new_teleprinter = section((ROOT / 'src/bridge/DecodiumBridgeTeleprinter.cpp').read_text(), 'void DecodiumBridge::aggiungiRigaRtty (')
    if old_teleprinter != new_teleprinter:
        raise SystemExit('teleprinter implementation differs from baseline')
    print('teleprinter: A/B source equivalence OK')
    old_overlay = section(baseline, 'void DecodiumBridge::recordFrameTimestamp()', 'void DecodiumBridge::emitDecodeListChangedThrottled()')
    new_overlay = section(metric_source, 'void DecodiumBridge::recordFrameTimestamp()').replace('decodiumBridgeOverlayLog(', 'bridgeLog(')
    if old_overlay != new_overlay:
        raise SystemExit('overlay implementation differs from baseline')
    print('diagnostic overlay: A/B source equivalence OK')
    old_knowncq = section(baseline, 'void DecodiumBridge::moveLegacyAllTxtCursorToEnd()', '// 1.0.257 — Auto-clear a soglia')
    new_knowncq = section((ROOT / 'src/bridge/DecodiumBridgeKnownCq.cpp').read_text(), 'void DecodiumBridge::moveLegacyAllTxtCursorToEnd()').replace('decodiumBridgeKnownCqLog(', 'bridgeLog(')
    if old_knowncq != new_knowncq:
        raise SystemExit('known CQ seeding differs from baseline')
    print('known CQ seeding: A/B source equivalence OK')
    audio_math = (ROOT / 'src/bridge/DecodiumBridgeAudioLevelMath.cpp').read_text()
    if not audio_math.endswith((ROOT / 'tests/bridge_refactor_fixtures/audio_level_math_before.inc').read_text()):
        raise SystemExit('audio level math differs from pre-extraction fixture')
    print('audio level math: pre-extraction source equivalence OK')
    for name in ('static qreal txAttenuationFromSlider(', 'static float rxInputGainFromLevel(', 'static qreal txGainFromSlider(', 'static int legacyTxAttenuationFromLevel('):
        if section(baseline, name, '\n}\n') != section(audio_math, name, '\n}\n'):
            raise SystemExit('audio conversion helper differs: ' + name)
    if 'static constexpr qreal kTxAudioHeadroomGain = 0.9;' not in baseline or 'static constexpr qreal kTxAudioHeadroomGain = 0.9;' not in audio_math:
        raise SystemExit('audio headroom differs')
    old_levels = section(baseline, 'double DecodiumBridge::audioLevel() const', 'QStringList DecodiumBridge::audioInputDevices() const')
    new_levels = section((ROOT / 'src/bridge/DecodiumBridgeAudioLevels.cpp').read_text(), 'double DecodiumBridge::audioLevel() const')
    for new, old in (('decodiumBridgeAudioLevelLog(', 'bridgeLog('), ('decodiumBridgeTxAttenuationFromSlider(', 'txAttenuationFromSlider('), ('decodiumBridgeRxInputGainFromLevel(', 'rxInputGainFromLevel('), ('decodiumBridgeTxGainFromSlider(', 'txGainFromSlider('), ('decodiumBridgeLegacyTxAttenuationFromLevel(', 'legacyTxAttenuationFromLevel(')):
        new_levels = new_levels.replace(new, old)
    if old_levels != new_levels:
        raise SystemExit('audio level implementation differs from baseline')
    print('audio levels: A/B source equivalence OK')
    old_uistall = section(baseline, 'bool DecodiumBridge::isUiStallActive(', 'bool DecodiumBridge::checkSwrAllowsTransmission(')
    new_uistall = section((ROOT / 'src/bridge/DecodiumBridgeUiStall.cpp').read_text(), 'bool DecodiumBridge::isUiStallActive(')
    if old_uistall != new_uistall:
        raise SystemExit('UI stall diagnostics differ from baseline')
    print('UI stall diagnostics: A/B source equivalence OK')
    old_preview = (section(baseline, 'QVariantMap DecodiumBridge::pendingLogQsoPreview() const', 'bool DecodiumBridge::promptToLogEnabled() const')
                   + section(baseline, 'QString DecodiumBridge::defaultLogCommentForQso(', 'static QString satelliteCodeFromDisplayText('))
    new_preview = section((ROOT / 'src/bridge/DecodiumBridgeLogPreview.cpp').read_text(), 'QVariantMap DecodiumBridge::pendingLogQsoPreview() const').replace('decodiumBridgeLogPromptDateTimeText(', 'logPromptDateTimeText(')
    if old_preview != new_preview:
        raise SystemExit('QSO log preview differs from baseline')
    print('QSO log preview: A/B source equivalence OK')
    if section(baseline, 'static double rxInputLevelFromGain(', '\n}\n') != section(audio_math, 'static double rxInputLevelFromGain(', '\n}\n'):
        raise SystemExit('RX gain inverse differs from baseline')
    if 'static constexpr double kAutoRxInputMaxLevel = 100.0;' not in baseline or 'static constexpr double kAutoRxInputMaxLevel = 100.0;' not in audio_math:
        raise SystemExit('auto RX gain limit differs from baseline')
    old_autorx = section(baseline, 'void DecodiumBridge::updateAutoRxInputLevel(', 'void DecodiumBridge::handleAudioHealth(')
    new_autorx = section((ROOT / 'src/bridge/DecodiumBridgeAutoRxLevel.cpp').read_text(), 'void DecodiumBridge::updateAutoRxInputLevel(')
    for new, old in (('decodiumBridgeRxInputGainFromLevel(', 'rxInputGainFromLevel('), ('decodiumBridgeRxInputLevelFromGain(', 'rxInputLevelFromGain('), ('decodiumBridgeAutoRxInputMaxLevel()', 'kAutoRxInputMaxLevel'), ('decodiumBridgeAudioLevelLog(', 'bridgeLog(')):
        new_autorx = new_autorx.replace(new, old)
    if old_autorx != new_autorx:
        raise SystemExit('automatic RX level logic differs from baseline')
    print('automatic RX level: A/B source equivalence OK')
    identity = (ROOT / 'src/bridge/DecodiumBridgeAudioDeviceIdentity.cpp').read_text()
    for name, source in (('static QString audioDeviceIdSettingForLog(', identity), ('static bool isFt2LinkApplicationMode(', (ROOT / 'src/bridge/DecodiumBridgeApplicationModes.cpp').read_text())):
        if section(baseline, name, '\n}\n') != section(source, name, '\n}\n'):
            raise SystemExit('audio health helper differs: ' + name)
    identity_fixture = (ROOT / 'tests/bridge_refactor_fixtures/audio_device_identity_before.inc').read_text()
    if identity_fixture.strip() != identity[identity.index('static QString audioDeviceIdForLog('):identity.index('QString decodiumBridgeAudioDeviceIdForLog(')].strip():
        raise SystemExit('audio device identity helpers differ from pre-extraction source')
    print('audio device identity helpers: pre-extraction source equivalence OK')
    resolve = (ROOT / 'src/bridge/DecodiumBridgeAudioDeviceResolve.cpp').read_text()
    if not resolve.endswith((ROOT / 'tests/bridge_refactor_fixtures/audio_device_resolve_before.inc').read_text()):
        raise SystemExit('audio device resolution differs from pre-extraction fixture')
    print('audio device resolution: pre-extraction source equivalence OK')
    visual = (ROOT / 'src/bridge/DecodiumBridgeVisualTxCoalesce.cpp').read_text()
    visual = visual[visual.index('static bool shouldCoalesceVisualTxRows('):visual.index('QString decodiumBridgeNormalizedVisualTxTimeToken(')]
    if visual != (ROOT / 'tests/bridge_refactor_fixtures/visual_tx_coalesce_before.inc').read_text():
        raise SystemExit('visual TX coalescing differs from pre-extraction fixture')
    print('visual TX coalescing: pre-extraction source equivalence OK')
    mirror_keys = (ROOT / 'src/bridge/DecodiumBridgeDecodeMirrorKeys.cpp').read_text()
    if not mirror_keys.endswith((ROOT / 'tests/bridge_refactor_fixtures/decode_mirror_keys_before.inc').read_text()):
        raise SystemExit('decode mirror keys differ from pre-extraction fixture')
    print('decode mirror keys: pre-extraction source equivalence OK')
    settings_helpers = (ROOT / 'src/bridge/DecodiumBridgeSettingsHelpers.cpp').read_text()
    if not settings_helpers.endswith((ROOT / 'tests/bridge_refactor_fixtures/settings_helpers_before.inc').read_text()):
        raise SystemExit('settings helpers differ from pre-extraction fixture')
    print('settings helpers: pre-extraction source equivalence OK')
    application_modes = (ROOT / 'src/bridge/DecodiumBridgeApplicationModes.cpp').read_text()
    if not application_modes.endswith((ROOT / 'tests/bridge_refactor_fixtures/application_mode_helpers_before.inc').read_text()):
        raise SystemExit('application mode helpers differ from pre-extraction fixture')
    print('application mode helpers: pre-extraction source equivalence OK')
    destructor = (ROOT / 'src/bridge/DecodiumBridgeDestructor.cpp').read_text().replace('decodiumBridgeSharedActiveCatSetPtt(this, m_hamlibCat, m_catBackend, false,', 'activeCatSetPtt(this, m_hamlibCat, m_catBackend, false,')
    if not destructor.endswith((ROOT / 'tests/bridge_refactor_fixtures/bridge_destructor_before.inc').read_text()):
        raise SystemExit('bridge destructor differs from pre-extraction fixture')
    print('bridge destructor: pre-extraction source equivalence OK')
    gpu_init = (ROOT / 'src/bridge/DecodiumBridgePanadapterGpuInit.cpp').read_text()
    if not gpu_init.endswith('void DecodiumBridge::initialisePanadapterGpuPolicy()\n{\n' + (ROOT / 'tests/bridge_refactor_fixtures/panadapter_gpu_init_before.inc').read_text() + '}\n'):
        raise SystemExit('panadapter GPU initialization differs from pre-extraction fixture')
    print('panadapter GPU initialization: pre-extraction source equivalence OK')
    ui_startup = (ROOT / 'src/bridge/DecodiumBridgeUiStartupSettings.cpp').read_text().replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if not ui_startup.endswith('void DecodiumBridge::initialiseUiStartupSettings()\n{\n' + (ROOT / 'tests/bridge_refactor_fixtures/ui_startup_settings_before.inc').read_text() + '}\n'):
        raise SystemExit('UI startup settings differ from pre-extraction fixture')
    print('UI startup settings: pre-extraction source equivalence OK')
    retire = (ROOT / 'src/bridge/DecodiumBridgeAudioSinkRetire.cpp').read_text().replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(').replace('decodiumBridgeTxPrepareTimelineLog(', 'txTimelineLog(')
    if not retire.endswith((ROOT / 'tests/bridge_refactor_fixtures/audio_sink_retire_before.inc').read_text()):
        raise SystemExit('audio sink retirement differs from pre-extraction fixture')
    print('audio sink retirement: pre-extraction source equivalence OK')
    old_health = section(baseline, 'void DecodiumBridge::handleAudioHealth(', 'void DecodiumBridge::restartAudioCaptureForModeChange(')
    new_health = section((ROOT / 'src/bridge/DecodiumBridgeAudioHealth.cpp').read_text(), 'void DecodiumBridge::handleAudioHealth(')
    for new, old in (('decodiumBridgeAudioLevelLog(', 'bridgeLog('), ('decodiumBridgeAudioDeviceIdSettingForLog(', 'audioDeviceIdSettingForLog('), ('decodiumBridgeIsFt2LinkApplicationMode(', 'isFt2LinkApplicationMode(')):
        new_health = new_health.replace(new, old)
    if old_health != new_health:
        raise SystemExit('audio health logic differs from baseline')
    print('audio health: A/B source equivalence OK')
    if section(baseline, 'static bool isNativePcmKeyboardMode(', '\n}\n') != section((ROOT / 'src/bridge/DecodiumBridgeApplicationModes.cpp').read_text(), 'static bool isNativePcmKeyboardMode(', '\n}\n'):
        raise SystemExit('native PCM keyboard mode helper differs')
    old_recovery = (section(baseline, 'void DecodiumBridge::restartAudioCaptureForModeChange(', 'void DecodiumBridge::scheduleRttyExitRxRecovery()')
                    + section(baseline, 'void DecodiumBridge::scheduleMonitorRecovery(', 'void DecodiumBridge::restartAudioCaptureFromWatchdog('))
    new_recovery = section((ROOT / 'src/bridge/DecodiumBridgeAudioRecovery.cpp').read_text(), 'void DecodiumBridge::restartAudioCaptureForModeChange(', '\nvoid DecodiumBridge::restartAudioCaptureFromWatchdog(')
    for new, old in (('decodiumBridgeAudioLevelLog(', 'bridgeLog('), ('decodiumBridgeRxInputGainFromLevel(', 'rxInputGainFromLevel('), ('decodiumBridgeIsNativePcmKeyboardMode(', 'isNativePcmKeyboardMode(')):
        new_recovery = new_recovery.replace(new, old)
    if old_recovery != new_recovery:
        raise SystemExit('mode-change audio recovery differs from baseline')
    print('mode-change audio recovery: A/B source equivalence OK')
    old_watchdog = section(baseline, 'void DecodiumBridge::restartAudioCaptureFromWatchdog(', 'void DecodiumBridge::stopAudioCapture()')
    new_watchdog = section((ROOT / 'src/bridge/DecodiumBridgeAudioRecovery.cpp').read_text(), 'void DecodiumBridge::restartAudioCaptureFromWatchdog(', '\nvoid DecodiumBridge::scheduleRttyExitRxRecovery()').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(').replace('decodiumBridgeIsFt2LinkApplicationMode(', 'isFt2LinkApplicationMode(')
    if old_watchdog != new_watchdog:
        raise SystemExit('audio watchdog recovery differs from baseline')
    print('audio watchdog recovery: A/B source equivalence OK')
    old_rtty_exit = section(baseline, 'void DecodiumBridge::scheduleRttyExitRxRecovery()', 'void DecodiumBridge::scheduleMonitorRecovery(')
    new_rtty_exit = section((ROOT / 'src/bridge/DecodiumBridgeAudioRecovery.cpp').read_text(), 'void DecodiumBridge::scheduleRttyExitRxRecovery()', '#define bridgeLog decodiumBridgeAudioLevelLog').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(').replace('decodiumBridgeRxInputGainFromLevel(', 'rxInputGainFromLevel(')
    if old_rtty_exit != new_rtty_exit:
        raise SystemExit('RTTY exit recovery differs from baseline')
    print('RTTY exit recovery: A/B source equivalence OK')
    old_audio_stop = section(baseline, 'void DecodiumBridge::stopAudioCapture()', 'void DecodiumBridge::dispatchTimeSyncDecodeWhenReady(')
    new_audio_stop = section((ROOT / 'src/bridge/DecodiumBridgeAudioCaptureStop.cpp').read_text(), 'void DecodiumBridge::stopAudioCapture()').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_audio_stop != new_audio_stop:
        raise SystemExit('audio capture stop differs from baseline')
    print('audio capture stop: A/B source equivalence OK')
    old_buffers = section(baseline, 'void DecodiumBridge::invalidateTxAudioCache()', 'QAudioDevice DecodiumBridge::resolveTxOutputDevice(')
    new_buffers = section((ROOT / 'src/bridge/DecodiumBridgeAudioBuffers.cpp').read_text(), 'void DecodiumBridge::invalidateTxAudioCache()').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_buffers != new_buffers:
        raise SystemExit('audio buffer lifecycle differs from baseline')
    print('audio buffer lifecycle: A/B source equivalence OK')
    old_txscheduling = section(baseline, 'void DecodiumBridge::suspendNonAudioTxWork(', 'bool DecodiumBridge::noteTxPlaybackFinished(')
    new_txscheduling = section((ROOT / 'src/bridge/DecodiumBridgeTxScheduling.cpp').read_text(), 'void DecodiumBridge::suspendNonAudioTxWork(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_txscheduling != new_txscheduling:
        raise SystemExit('TX scheduling differs from baseline')
    print('TX scheduling: A/B source equivalence OK')
    tx_policy = (ROOT / 'src/bridge/DecodiumBridgeTxPolicy.cpp').read_text()
    for name in ('static int estimatedSyncPayloadMs(', 'static int minimumUsefulSyncPayloadMs(', 'static int ft2AsyncTxStartSafetyMs(', 'static int latestFt2AsyncTxStartMs('):
        old = section(baseline, name, '\n}\n').replace('static int ', 'int ', 1)
        if old != section(tx_policy, name.replace('static int ', 'int ', 1), '\n}\n'):
            raise SystemExit('TX timing helper differs: ' + name)
    for name in ('bool catSuppressedByEnvironment()', 'bool truthyEnvironmentFlag('):
        if section(baseline, name, '\n}\n') != section((ROOT / 'src/bridge/DecodiumBridgeEnvironmentRxPolicy.cpp').read_text(), name, '\n}\n'):
            raise SystemExit('TX timing helper differs: ' + name)
    old_timing = section(baseline, 'bool DecodiumBridge::shouldAlignTxAudioToCurrentSyncSlot() const', 'void DecodiumBridge::resetTxAudioTelemetry(')
    new_timing = section((ROOT / 'src/bridge/DecodiumBridgeTxTiming.cpp').read_text(), 'bool DecodiumBridge::shouldAlignTxAudioToCurrentSyncSlot() const')
    for new, old in (('decodiumBridgeAudioLevelLog(', 'bridgeLog('), ('decodiumBridgeCatSuppressedForTx()', 'catSuppressedByEnvironment()'), ('decodiumBridgeLatestFt2AsyncTxStartMs(', 'latestFt2AsyncTxStartMs(')):
        new_timing = new_timing.replace(new, old)
    if old_timing != new_timing:
        raise SystemExit('TX timing differs from baseline')
    print('TX timing: A/B source equivalence OK')
    if section(baseline, 'static QString inferPartnerFromDirectedMessage(', '\n}\n') != section((ROOT / 'src/bridge/DecodiumBridgeAutologHelpers.cpp').read_text(), 'static QString inferPartnerFromDirectedMessage(', '\n}\n'):
        raise SystemExit('autolog partner inference helper differs')
    if 'constexpr int kLateAutoLogGraceWindowSeconds {45};' not in baseline or 'constexpr int kLateAutoLogGraceWindowSeconds {45};' not in (ROOT / 'src/bridge/DecodiumBridgeAutologHelpers.cpp').read_text():
        raise SystemExit('autolog late snapshot grace differs')
    old_snapshot = (section(baseline, 'QString DecodiumBridge::inferredPartnerForAutolog() const', 'void DecodiumBridge::clearPendingAutoSeqTx(')
                    + section(baseline, 'void DecodiumBridge::armLateAutoLogSnapshot()', 'bool DecodiumBridge::shouldPreserveDeferredAutoSeqTxForRearm('))
    new_snapshot = section((ROOT / 'src/bridge/DecodiumBridgeAutoLogSnapshot.cpp').read_text(), 'QString DecodiumBridge::inferredPartnerForAutolog() const')
    for new, old in (('decodiumBridgeInferPartnerForAutolog(', 'inferPartnerFromDirectedMessage('), ('decodiumBridgeLateAutoLogGraceSeconds()', 'kLateAutoLogGraceWindowSeconds'), ('decodiumBridgeAudioLevelLog(', 'bridgeLog(')):
        new_snapshot = new_snapshot.replace(new, old)
    if old_snapshot != new_snapshot:
        raise SystemExit('autolog snapshot lifecycle differs from baseline')
    print('autolog snapshot lifecycle: A/B source equivalence OK')
    old_prompt_snapshot = section(baseline, 'void DecodiumBridge::capturePromptLogSnapshot(', 'QString DecodiumBridge::defaultLogCommentForQso(')
    new_prompt_snapshot = section((ROOT / 'src/bridge/DecodiumBridgePromptSnapshot.cpp').read_text(), 'void DecodiumBridge::capturePromptLogSnapshot(')
    if old_prompt_snapshot != new_prompt_snapshot:
        raise SystemExit('prompt snapshot differs from baseline')
    print('prompt snapshot: A/B source equivalence OK')
    old_prompt_lifecycle = section(baseline, 'bool DecodiumBridge::promptToLogEnabled() const', 'void DecodiumBridge::impostaModoRadioRtty(') + section(baseline, 'void DecodiumBridge::logQso()', 'void DecodiumBridge::capturePromptLogSnapshot(')
    new_prompt_lifecycle = section((ROOT / 'src/bridge/DecodiumBridgeLogPromptLifecycle.cpp').read_text(), 'bool DecodiumBridge::promptToLogEnabled() const').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_prompt_lifecycle != new_prompt_lifecycle:
        raise SystemExit('prompt lifecycle differs from baseline')
    print('prompt lifecycle: A/B source equivalence OK')
    old_audio_access = section(baseline, 'void DecodiumBridge::scheduleAudioDeviceRefresh(', 'void DecodiumBridge::refreshAudioDeviceCache(') + section(baseline, 'QList<QAudioDevice> DecodiumBridge::cachedAudioInputs(', 'QAudioDevice DecodiumBridge::resolveRxInputDevice(') + section(baseline, 'void DecodiumBridge::enumerateAudioDevices()', '// ============================================================')
    new_audio_access = section((ROOT / 'src/bridge/DecodiumBridgeAudioDeviceCacheAccess.cpp').read_text(), 'void DecodiumBridge::scheduleAudioDeviceRefresh(')
    if old_audio_access != new_audio_access:
        raise SystemExit('audio device cache access differs from baseline')
    print('audio device cache access: A/B source equivalence OK')
    old_input_selection = section(baseline, 'QAudioDevice DecodiumBridge::resolveRxInputDevice(', 'void DecodiumBridge::enumerateAudioDevices()')
    new_input_selection = section((ROOT / 'src/bridge/DecodiumBridgeAudioInputSelection.cpp').read_text(), 'QAudioDevice DecodiumBridge::resolveRxInputDevice(')
    for new, old in (('decodiumBridgeResolveAudioDeviceByIdentity(', 'tryResolveAudioDeviceByIdentity('), ('decodiumBridgeLinuxPulseSourceNameFromDisplay(', 'linuxPulseSourceNameFromDisplay('), ('decodiumBridgeLinuxSetDefaultPulseSource(', 'linuxSetDefaultPulseSource('), ('decodiumBridgeAudioLevelLog(', 'bridgeLog(')):
        new_input_selection = new_input_selection.replace(new, old)
    if old_input_selection != new_input_selection:
        raise SystemExit('audio input selection differs from baseline')
    print('audio input selection: A/B source equivalence OK')
    old_audio_refresh = section(baseline, 'void DecodiumBridge::refreshAudioDeviceCache(', 'QList<QAudioDevice> DecodiumBridge::cachedAudioInputs(')
    new_audio_refresh = section((ROOT / 'src/bridge/DecodiumBridgeAudioDeviceCacheRefresh.cpp').read_text(), 'void DecodiumBridge::refreshAudioDeviceCache(')
    for new, old in [('decodiumCacheDescriptions(', 'audioDeviceDescriptions('), ('decodiumCacheSignature(', 'audioDeviceCacheSignature('), ('decodiumCacheDeviceId(', 'audioDeviceIdForLog('), ('decodiumCacheAudioFormat(', 'audioFormatForLog('), ('decodiumCachePulseSources(', 'linuxPulseSourcesForLog('), ('decodiumCachePulseSinks(', 'linuxPulseSinksForLog('), ('decodiumCacheAlsaCards(', 'linuxAlsaCardsForLog('), ('decodiumCacheLooksUsb(', 'audioDeviceLooksUsb('), ('decodiumCacheSourceName(', 'linuxPulseSourceNameFromShortLine('), ('decodiumCacheSourceDisplay(', 'linuxPulseSourceDisplayName('), ('decodiumCacheSinkName(', 'linuxPulseSinkNameFromShortLine('), ('decodiumCacheSinkPrefix()', 'kLinuxPulseSinkPrefix'), ('decodiumBridgeAudioLevelLog(', 'bridgeLog(')]:
        new_audio_refresh = new_audio_refresh.replace(new, old)
    if old_audio_refresh != new_audio_refresh:
        raise SystemExit('audio device cache refresh differs from baseline')
    print('audio device cache refresh: A/B source equivalence OK')
    old_audio_helpers = section(baseline, 'static QStringList audioDeviceDescriptions(', 'static inline void activeCatSetFreq(')
    new_audio_helpers = section((ROOT / 'src/bridge/DecodiumBridgeAudioDeviceCacheRefresh.cpp').read_text(), 'static QStringList audioDeviceDescriptions(', 'void DecodiumBridge::refreshAudioDeviceCache(').replace('decodiumCacheDeviceId(', 'audioDeviceIdForLog(')
    if old_audio_helpers != new_audio_helpers:
        raise SystemExit('audio cache helpers differ from baseline')
    print('audio cache helpers: A/B source equivalence OK')
    old_model_filter = section(baseline, 'bool DecodiumBridge::entryBelongsToCurrentQso(', 'void DecodiumBridge::rebuildBandActivityModel()')
    new_model_filter = section((ROOT / 'src/bridge/DecodiumBridgeDecodeModelFilter.cpp').read_text(), 'bool DecodiumBridge::entryBelongsToCurrentQso(')
    for new, old in (('decodiumBridgeAudioLevelLog(', 'bridgeLog('), ('decodiumBridgeDecodeRxClearEntryKey(', 'decodeRxClearEntryKey('), ('decodiumBridgeCoalesceRxPaneTxRows(', 'coalesceRxPaneTxRows('), ('decodiumBridgeDecodeDisplaySortMsecs(', 'decodeDisplaySortMsecs(')):
        new_model_filter = new_model_filter.replace(new, old)
    if old_model_filter != new_model_filter:
        raise SystemExit('decode model filter differs from baseline')
    print('decode model filter: A/B source equivalence OK')
    old_decode_visibility = section(baseline, 'decodium::decode_ui::WorkedFilterOptions DecodiumBridge::workedFilterOptions() const', '// 1.0.145: detection ghost decode')
    new_decode_visibility = section((ROOT / 'src/bridge/DecodiumBridgeDecodeVisibility.cpp').read_text(), 'decodium::decode_ui::WorkedFilterOptions DecodiumBridge::workedFilterOptions() const').replace('decodiumBridgeDecodeTelemetryOnly(', 'decodeListModel_isTelemetryOnlyMessage(')
    if old_decode_visibility != new_decode_visibility:
        raise SystemExit('decode visibility differs from baseline')
    print('decode visibility: A/B source equivalence OK')
    old_decode_schedule = section(baseline, 'void DecodiumBridge::rebuildBandActivityModel()', 'void DecodiumBridge::startDecodeUiSnapshotRefresh()')
    new_decode_schedule = section((ROOT / 'src/bridge/DecodiumBridgeDecodeUiSchedule.cpp').read_text(), 'void DecodiumBridge::rebuildBandActivityModel()')
    if old_decode_schedule != new_decode_schedule:
        raise SystemExit('decode UI scheduling differs from baseline')
    print('decode UI scheduling: A/B source equivalence OK')
    old_full_spectrum = section(baseline, 'void DecodiumBridge::scheduleFullSpectrumModelRefresh()', '// 1.0.233 — DevOverlay metrics')
    new_full_spectrum = section((ROOT / 'src/bridge/DecodiumBridgeFullSpectrumSnapshot.cpp').read_text(), 'void DecodiumBridge::scheduleFullSpectrumModelRefresh()')
    if old_full_spectrum != new_full_spectrum:
        raise SystemExit('full spectrum snapshot differs from baseline')
    print('full spectrum snapshot: A/B source equivalence OK')
    old_decode_notifications = section(baseline, 'void DecodiumBridge::emitDecodeListChangedThrottled()', 'DecodiumBridge::DecodiumBridge(QObject* parent)')
    new_decode_notifications = section((ROOT / 'src/bridge/DecodiumBridgeDecodeUiNotifications.cpp').read_text(), 'void DecodiumBridge::emitDecodeListChangedThrottled()')
    if old_decode_notifications != new_decode_notifications:
        raise SystemExit('decode UI notifications differ from baseline')
    print('decode UI notifications: A/B source equivalence OK')
    old_ui_worker = (section(baseline, 'struct DecodeUiClassifiedSource', 'static QVariantMap decodeDeltaBoundaryEntry(') + section(baseline, 'static DecodeUiSnapshotResult prepareDecodeUiSnapshot(', '} // namespace'))
    old_ui_snapshot = section(baseline, 'void DecodiumBridge::startDecodeUiSnapshotRefresh()', 'void DecodiumBridge::scheduleFullSpectrumModelRefresh()')
    new_ui_file = (ROOT / 'src/bridge/DecodiumBridgeDecodeUiSnapshot.cpp').read_text()
    new_ui_worker = section(new_ui_file, 'struct DecodeUiClassifiedSource', '} // namespace')
    new_ui_snapshot = section(new_ui_file, 'void DecodiumBridge::startDecodeUiSnapshotRefresh()')
    for new, old in [('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey('), ('decodiumBridgeDecodeTelemetryOnly(', 'decodeListModel_isTelemetryOnlyMessage('), ('decodiumBridgeNormalizeDecodeEntries(', 'normalizeDecodeEntriesForDisplay('), ('decodiumBridgeDecodeRxClearEntryKey(', 'decodeRxClearEntryKey('), ('decodiumBridgeCoalesceRxPaneTxRows(', 'coalesceRxPaneTxRows(')]:
        new_ui_worker = new_ui_worker.replace(new, old)
        new_ui_snapshot = new_ui_snapshot.replace(new, old)
    if old_ui_worker != new_ui_worker or old_ui_snapshot != new_ui_snapshot:
        raise SystemExit('decode UI snapshot differs from baseline')
    print('decode UI snapshot: A/B source equivalence OK')
    old_ownership = section(baseline, 'bool DecodiumBridge::usingLegacyBackendForTx() const', 'void DecodiumBridge::ensureDecodeWorkerForMode(') + section(baseline, 'bool DecodiumBridge::useModernSpectrumFeedWithLegacy() const', 'bool DecodiumBridge::shouldIgnoreDecodeCallbacks() const')
    new_ownership = section((ROOT / 'src/bridge/DecodiumBridgeLegacyOwnership.cpp').read_text(), 'bool DecodiumBridge::usingLegacyBackendForTx() const')
    for new, old in (('decodiumBridgeIsFt2LinkApplicationMode(', 'isFt2LinkApplicationMode('), ('decodiumBridgeIsStreamingKeyboardMode(', 'isStreamingKeyboardMode('), ('decodiumBridgeOwnsLegacyJtRxWhenCatSuppressed(', 'bridgeOwnsLegacyJtRxWhenCatSuppressed('), ('decodiumBridgeOwnsFst4Audio(', 'bridgeOwnsFst4Audio(')):
        new_ownership = new_ownership.replace(new, old)
    if old_ownership != new_ownership:
        raise SystemExit('legacy ownership differs from baseline')
    print('legacy ownership: A/B source equivalence OK')
    old_legacy_refresh = section(baseline, 'void DecodiumBridge::syncLegacyBackendTxState()', 'void DecodiumBridge::migrateActiveMonitoringToLegacyBackend()')
    new_legacy_refresh = section((ROOT / 'src/bridge/DecodiumBridgeLegacyRefresh.cpp').read_text(), 'void DecodiumBridge::syncLegacyBackendTxState()').replace('decodiumBridgeLegacyRigControlFallback(', 'useLegacyRigControlFallback(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_legacy_refresh != new_legacy_refresh:
        raise SystemExit('legacy refresh differs from baseline')
    print('legacy refresh: A/B source equivalence OK')
    old_legacy_dialog = section(baseline, 'void DecodiumBridge::syncLegacyBackendDialogState()', 'void DecodiumBridge::syncLegacyBackendTxState()')
    new_legacy_dialog = section((ROOT / 'src/bridge/DecodiumBridgeLegacyDialogSync.cpp').read_text(), 'void DecodiumBridge::syncLegacyBackendDialogState()')
    for new, old in (('decodiumBridgeLegacyTxAttenuationFromLevel(', 'legacyTxAttenuationFromLevel('), ('decodiumBridgeIsFt2LinkApplicationMode(', 'isFt2LinkApplicationMode('), ('decodiumBridgeIsStreamingKeyboardMode(', 'isStreamingKeyboardMode('), ('decodiumBridgeAudioLevelLog(', 'bridgeLog('), ('decodiumBridgeSpecialOpHound()', 'kSpecialOpHound')):
        new_legacy_dialog = new_legacy_dialog.replace(new, old)
    if old_legacy_dialog != new_legacy_dialog:
        raise SystemExit('legacy dialog sync differs from baseline')
    print('legacy dialog sync: A/B source equivalence OK')
    old_legacy_handoff = section(baseline, 'void DecodiumBridge::migrateActiveMonitoringToLegacyBackend()', 'void DecodiumBridge::applyDirectVisualAudioCaptureMode(')
    new_legacy_handoff = section((ROOT / 'src/bridge/DecodiumBridgeLegacyRxHandoff.cpp').read_text(), 'void DecodiumBridge::migrateActiveMonitoringToLegacyBackend()').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_legacy_handoff != new_legacy_handoff:
        raise SystemExit('legacy RX handoff differs from baseline')
    print('legacy RX handoff: A/B source equivalence OK')
    old_legacy_pcm = section(baseline, 'void DecodiumBridge::applyDirectVisualAudioCaptureMode(', 'void DecodiumBridge::syncLegacyBackendState()')
    new_legacy_pcm = section((ROOT / 'src/bridge/DecodiumBridgeLegacyPcmRecovery.cpp').read_text(), 'void DecodiumBridge::applyDirectVisualAudioCaptureMode(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(').replace('decodiumBridgeIsFt2LinkApplicationMode(', 'isFt2LinkApplicationMode(')
    if old_legacy_pcm != new_legacy_pcm:
        raise SystemExit('legacy PCM recovery differs from baseline')
    print('legacy PCM recovery: A/B source equivalence OK')
    old_decode_shutdown = section(baseline, 'bool DecodiumBridge::shouldIgnoreDecodeCallbacks() const', 'bool DecodiumBridge::ensureLegacyBackendAvailable()')
    new_decode_shutdown = section((ROOT / 'src/bridge/DecodiumBridgeDecodeShutdown.cpp').read_text(), 'bool DecodiumBridge::shouldIgnoreDecodeCallbacks() const')
    if old_decode_shutdown != new_decode_shutdown:
        raise SystemExit('decode shutdown differs from baseline')
    print('decode shutdown: A/B source equivalence OK')
    old_qsy_guard = section(baseline, 'bool DecodiumBridge::shouldIgnoreCatFrequencyDuringLocalQsy(', 'void DecodiumBridge::applyRemoteDialFrequency(') + section(baseline, 'bool DecodiumBridge::shouldIgnoreLegacyAudioFrequencyDuringLocalQsy(', 'void DecodiumBridge::applyRemoteBandChange(')
    new_qsy_guard = section((ROOT / 'src/bridge/DecodiumBridgeQsyGuard.cpp').read_text(), 'bool DecodiumBridge::shouldIgnoreCatFrequencyDuringLocalQsy(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_qsy_guard != new_qsy_guard:
        raise SystemExit('QSY guard differs from baseline')
    print('QSY guard: A/B source equivalence OK')
    old_visual_schedule = section(baseline, 'bool DecodiumBridge::visualFeedsDeferredForTx() const', 'void DecodiumBridge::queueWorldMapEntryForReplay(') + section(baseline, 'void DecodiumBridge::scheduleDeferredWorldMapFeedFlush(', 'void DecodiumBridge::resetWorldMapDisplayFromCurrentDecodes()')
    new_visual_schedule = section((ROOT / 'src/bridge/DecodiumBridgeVisualFeedScheduling.cpp').read_text(), 'bool DecodiumBridge::visualFeedsDeferredForTx() const').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_visual_schedule != new_visual_schedule:
        raise SystemExit('visual feed scheduling differs from baseline')
    print('visual feed scheduling: A/B source equivalence OK')
    old_map_queue = section(baseline, 'void DecodiumBridge::queueWorldMapEntryForReplay(', 'void DecodiumBridge::scheduleDeferredWorldMapFeedFlush(')
    new_map_queue = section((ROOT / 'src/bridge/DecodiumBridgeWorldMapQueue.cpp').read_text(), 'void DecodiumBridge::queueWorldMapEntryForReplay(')
    new_map_queue = new_map_queue.replace('decodiumBridgeUnresolvedGridOnlyDecode(message, nullptr)', 'messageIsUnresolvedGridOnlyDecode(message)').replace('decodiumBridgeFst4wUnresolvedHashDecode(', 'isFst4wUnresolvedHashDecode(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_map_queue != new_map_queue:
        raise SystemExit('world map queue differs from baseline')
    print('world map queue: A/B source equivalence OK')
    old_map_reset = section(baseline, 'QString DecodiumBridge::worldMapFeedEntryKey(', 'bool DecodiumBridge::visualFeedsDeferredForTx() const') + section(baseline, 'void DecodiumBridge::resetWorldMapDisplayFromCurrentDecodes()', 'void DecodiumBridge::replayWorldMapEntry(')
    new_map_reset = section((ROOT / 'src/bridge/DecodiumBridgeWorldMapReset.cpp').read_text(), 'QString DecodiumBridge::worldMapFeedEntryKey(')
    for new, old in (('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey('), ('decodiumBridgeSecondsFromUtcDisplayToken(', 'secondsFromUtcDisplayToken('), ('decodiumBridgeSignedUtcSecondDelta(', 'signedUtcSecondDelta(')):
        new_map_reset = new_map_reset.replace(new, old)
    if old_map_reset != new_map_reset:
        raise SystemExit('world map reset differs from baseline')
    print('world map reset: A/B source equivalence OK')
    old_map_replay = section(baseline, 'void DecodiumBridge::replayWorldMapFeed()', 'void DecodiumBridge::processMapContactClick(')
    new_map_replay = section((ROOT / 'src/bridge/DecodiumBridgeWorldMapReplay.cpp').read_text(), 'void DecodiumBridge::replayWorldMapFeed()').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_map_replay != new_map_replay:
        raise SystemExit('world map replay differs from baseline')
    print('world map replay: A/B source equivalence OK')
    old_map_qso = section(baseline, 'void DecodiumBridge::markWorldMapQsoClosed(', 'bool DecodiumBridge::worldMapFeedEnabled() const') + section(baseline, 'void DecodiumBridge::emitCurrentWorldMapQsoPath()', 'void DecodiumBridge::replayWorldMapFeed()')
    new_map_qso = section((ROOT / 'src/bridge/DecodiumBridgeWorldMapQso.cpp').read_text(), 'void DecodiumBridge::markWorldMapQsoClosed(')
    for new, old in (('decodiumBridgeNormalizeWorldMapCall(', 'normalizeWorldMapCall('), ('decodiumBridgeWorldMapCallKey(', 'worldMapCallKey('), ('decodiumBridgeAudioLevelLog(', 'bridgeLog(')):
        new_map_qso = new_map_qso.replace(new, old)
    if old_map_qso != new_map_qso:
        raise SystemExit('world map QSO path differs from baseline')
    print('world map QSO path: A/B source equivalence OK')
    old_map_geo = section(baseline, 'static QString maidenheadFromLonLat(', 'static int decodeSnrOrDefault(')
    old_map_loader = section(baseline, 'struct WorldMapCall3LoadResult', '/* step A3:')
    old_map_loader = old_map_loader.replace(section(baseline, 'static QStringList worldMapCall3CandidatePaths()', 'static WorldMapCall3LoadResult loadWorldMapCall3CacheFromFiles('), 'QStringList worldMapCall3CandidatePaths();\n\n')
    old_map_grid = section(baseline, 'void DecodiumBridge::rememberWorldMapGrid(', 'void DecodiumBridge::markWorldMapQsoClosed(')
    map_grid_file = (ROOT / 'src/bridge/DecodiumBridgeWorldMapGrid.cpp').read_text()
    new_map_geo = section(map_grid_file, 'static QString maidenheadFromLonLat(', 'struct WorldMapCall3LoadResult')
    new_map_loader = section(map_grid_file, 'struct WorldMapCall3LoadResult', 'void DecodiumBridge::rememberWorldMapGrid(')
    new_map_grid = section(map_grid_file, 'void DecodiumBridge::rememberWorldMapGrid(')
    for new, old in (('decodiumBridgeWorldMapCallKey(', 'worldMapCallKey('), ('decodiumBridgeNormalizeWorldMapCall(', 'normalizeWorldMapCall('), ('decodiumBridgeAudioLevelLog(', 'bridgeLog(')):
        new_map_loader = new_map_loader.replace(new, old)
        new_map_grid = new_map_grid.replace(new, old)
    if old_map_geo != new_map_geo or old_map_loader != new_map_loader or old_map_grid != new_map_grid:
        raise SystemExit('world map grid differs from baseline')
    print('world map grid: A/B source equivalence OK')
    old_map_entry = section(baseline, 'void DecodiumBridge::replayWorldMapEntry(', 'void DecodiumBridge::emitCurrentWorldMapQsoPath()')
    new_map_entry = section((ROOT / 'src/bridge/DecodiumBridgeWorldMapEntry.cpp').read_text(), 'void DecodiumBridge::replayWorldMapEntry(')
    for new, old in (('decodiumBridgeNormalizeWorldMapCall(', 'normalizeWorldMapCall('), ('decodiumBridgeWorldMapCallKey(', 'worldMapCallKey('), ('decodiumBridgeLooksLikeCallsignToken(', 'looksLikeCallsignToken('), ('decodiumBridgeUnresolvedGridOnlyDecode(message, nullptr)', 'messageIsUnresolvedGridOnlyDecode(message)'), ('decodiumBridgeFst4wUnresolvedHashDecode(', 'isFst4wUnresolvedHashDecode(')):
        new_map_entry = new_map_entry.replace(new, old)
    if old_map_entry != new_map_entry:
        raise SystemExit('world map entry differs from baseline')
    print('world map entry: A/B source equivalence OK')
    old_map_selection = section(baseline, 'void DecodiumBridge::processMapContactClick(', 'void DecodiumBridge::enrichDecodeEntry(')
    new_map_selection = section((ROOT / 'src/bridge/DecodiumBridgeWorldMapSelection.cpp').read_text(), 'void DecodiumBridge::processMapContactClick(')
    for new, old in (('decodiumBridgeNormalizeWorldMapCall(', 'normalizeWorldMapCall('), ('decodiumBridgeWorldMapCallKey(', 'worldMapCallKey('), ('decodiumBridgeAudioLevelLog(', 'bridgeLog('), ('decodiumBridgeTruthyEnvironmentFlag(', 'truthyEnvironmentFlag('), ('decodiumBridgeTxPeriodIsEven(', 'bridgeTxPeriodIsEven(')):
        new_map_selection = new_map_selection.replace(new, old)
    if old_map_selection != new_map_selection:
        raise SystemExit('world map selection differs from baseline')
    print('world map selection: A/B source equivalence OK')
    old_legacy_adif = section(baseline, 'void DecodiumBridge::mirrorLegacyLoggedAdif(', 'using AdifImportProgressCallback =')
    new_legacy_adif = section((ROOT / 'src/bridge/DecodiumBridgeLegacyAdifMirror.cpp').read_text(), 'void DecodiumBridge::mirrorLegacyLoggedAdif(').replace('decodiumBridgeLogbookLog(', 'bridgeLog(')
    if old_legacy_adif != new_legacy_adif:
        raise SystemExit('legacy ADIF mirror differs from baseline')
    print('legacy ADIF mirror: A/B source equivalence OK')
    old_ft2_bridge = section(baseline, 'void DecodiumBridge::setFt2LogBridgeStatus(', 'void DecodiumBridge::armStationTelemetryForNextSlot(')
    new_ft2_bridge = section((ROOT / 'src/bridge/DecodiumBridgeFt2LogBridge.cpp').read_text(), 'void DecodiumBridge::setFt2LogBridgeStatus(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_ft2_bridge != new_ft2_bridge:
        raise SystemExit('FT2 log bridge differs from baseline')
    print('FT2 log bridge: A/B source equivalence OK')
    old_decode_alert = section(baseline, 'void DecodiumBridge::maybePlayDecodeAlert(', '// ============================================================\n// WAV decode:')
    new_decode_alert = section((ROOT / 'src/bridge/DecodiumBridgeDecodeAlerts.cpp').read_text(), 'void DecodiumBridge::maybePlayDecodeAlert(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_decode_alert != new_decode_alert:
        raise SystemExit('decode alerts differ from baseline')
    print('decode alerts: A/B source equivalence OK')
    old_station_timer = section(baseline, 'void DecodiumBridge::armStationTelemetryForNextSlot(', '// ── Decodium RX dal menu')
    new_station_timer = section((ROOT / 'src/bridge/DecodiumBridgeStationTelemetry.cpp').read_text(), 'void DecodiumBridge::armStationTelemetryForNextSlot(', 'using decodium::seq::isGridTokenStrict;').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(').replace('decodiumBridgeTxPeriodIsEven(', 'bridgeTxPeriodIsEven(')
    if old_station_timer != new_station_timer:
        raise SystemExit('station telemetry timer differs from baseline')
    print('station telemetry timer: A/B source equivalence OK')
    old_rx_terminal = section(baseline, '// ── Decodium RX dal menu')
    new_rx_terminal = section((ROOT / 'src/bridge/DecodiumBridgeRxTerminal.cpp').read_text(), '// ── Decodium RX dal menu').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_rx_terminal != new_rx_terminal:
        raise SystemExit('RX terminal launcher differs from baseline')
    print('RX terminal launcher: A/B source equivalence OK')
    old_cw_settings = section(baseline, 'bool DecodiumBridge::cwMonitorEnabled()', 'void DecodiumBridge::testCwMonitor()')
    new_cw_settings = section((ROOT / 'src/bridge/DecodiumBridgeCwMonitorSettings.cpp').read_text(), 'bool DecodiumBridge::cwMonitorEnabled()', 'void DecodiumBridge::testCwMonitor()')
    if old_cw_settings != new_cw_settings:
        raise SystemExit('CW monitor settings differ from baseline')
    print('CW monitor settings: A/B source equivalence OK')
    old_rx_recording = section(baseline, 'bool DecodiumBridge::recordRxEnabled()', 'QString DecodiumBridge::effectiveAdifLogPath()')
    new_rx_recording = section((ROOT / 'src/bridge/DecodiumBridgeRxRecording.cpp').read_text(), 'bool DecodiumBridge::recordRxEnabled()')
    if old_rx_recording != new_rx_recording:
        raise SystemExit('RX recording control differs from baseline')
    print('RX recording control: A/B source equivalence OK')
    old_cw_test = section(baseline, 'void DecodiumBridge::testCwMonitor()', '// SSB voice workspace.')
    new_cw_test = section((ROOT / 'src/bridge/DecodiumBridgeCwMonitorSettings.cpp').read_text(), 'void DecodiumBridge::testCwMonitor()', 'bool DecodiumBridge::playCwMonitor(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_cw_test != new_cw_test:
        raise SystemExit('CW monitor test differs from baseline')
    print('CW monitor test: A/B source equivalence OK')
    old_cw_play = section(baseline, 'bool DecodiumBridge::playCwMonitor(', '// Il PTT della radio remota')
    new_cw_play = section((ROOT / 'src/bridge/DecodiumBridgeCwMonitorSettings.cpp').read_text(), 'bool DecodiumBridge::playCwMonitor(')
    if old_cw_play != new_cw_play:
        raise SystemExit('CW monitor playback differs from baseline')
    print('CW monitor playback: A/B source equivalence OK')
    old_decode_indicators = section(baseline, 'void DecodiumBridge::resetLedStatus()', 'QString DecodiumBridge::diagnosticLogPath()')
    new_decode_indicators = section((ROOT / 'src/bridge/DecodiumBridgeDecodeIndicators.cpp').read_text(), 'void DecodiumBridge::resetLedStatus()')
    if old_decode_indicators != new_decode_indicators:
        raise SystemExit('decode indicators differ from baseline')
    print('decode indicators: A/B source equivalence OK')
    old_station_send = section(baseline, 'void DecodiumBridge::sendStationTelemetry()', 'void DecodiumBridge::fetchWeatherForGrid()')
    new_station_send = section((ROOT / 'src/bridge/DecodiumBridgeStationTelemetry.cpp').read_text(), 'void DecodiumBridge::sendStationTelemetry()').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_station_send != new_station_send:
        raise SystemExit('station telemetry send differs from baseline')
    print('station telemetry send: A/B source equivalence OK')
    old_station_weather = section(baseline, 'void DecodiumBridge::fetchWeatherForGrid()', 'bool DecodiumBridge::ft2LinkSatelliteHalfDuplexRequested()')
    new_station_weather = section((ROOT / 'src/bridge/DecodiumBridgeStationWeather.cpp').read_text(), 'void DecodiumBridge::fetchWeatherForGrid()').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_station_weather != new_station_weather:
        raise SystemExit('station weather differs from baseline')
    print('station weather: A/B source equivalence OK')
    old_spectrum_visibility = section(baseline, 'void DecodiumBridge::setSpectrumVisible(', 'bool DecodiumBridge::cpuPressureActive()')
    new_spectrum_visibility = section((ROOT / 'src/bridge/DecodiumBridgeSpectrumVisibility.cpp').read_text(), 'void DecodiumBridge::setSpectrumVisible(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_spectrum_visibility != new_spectrum_visibility:
        raise SystemExit('spectrum visibility differs from baseline')
    print('spectrum visibility: A/B source equivalence OK')
    old_alltxt_folder = section(baseline, 'QString DecodiumBridge::logAllTxtPath() const', '// === Shannon FDR')
    new_alltxt_folder = section((ROOT / 'src/bridge/DecodiumBridgeLegacyAllTxt.cpp').read_text(), 'QString DecodiumBridge::logAllTxtPath() const')
    if old_alltxt_folder != new_alltxt_folder:
        raise SystemExit('ALL.TXT folder handling differs from baseline')
    print('ALL.TXT folder handling: A/B source equivalence OK')
    old_cw_ports = section(baseline, 'bool DecodiumBridge::cwRigKeyerAvailable() const', 'bool DecodiumBridge::cwSendRigMorse(')
    new_cw_ports = section((ROOT / 'src/bridge/DecodiumBridgeCwPorts.cpp').read_text(), 'bool DecodiumBridge::cwRigKeyerAvailable() const')
    if old_cw_ports != new_cw_ports:
        raise SystemExit('CW port reservations differ from baseline')
    print('CW port reservations: A/B source equivalence OK')
    old_cw_rig = section(baseline, 'bool DecodiumBridge::cwSendRigMorse(', 'bool DecodiumBridge::cwSendAudio(')
    new_cw_rig = section((ROOT / 'src/bridge/DecodiumBridgeCwRig.cpp').read_text(), 'bool DecodiumBridge::cwSendRigMorse(', 'bool DecodiumBridge::cwTuneFrequency(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_cw_rig != new_cw_rig:
        raise SystemExit('CW rig control differs from baseline')
    print('CW rig control: A/B source equivalence OK')
    old_cw_remote = (section(baseline, 'bool DecodiumBridge::cwSendAudio(', 'bool DecodiumBridge::playCwMonitor(')
                     + section(baseline, '// Il PTT della radio remota', 'bool DecodiumBridge::registraQsoCw(')
                     + section(baseline, 'void DecodiumBridge::cwAbortAudio()', 'void DecodiumBridge::setSpectrumVisible('))
    new_cw_remote = section((ROOT / 'src/bridge/DecodiumBridgeCwRemote.cpp').read_text(), 'bool DecodiumBridge::cwSendAudio(', 'bool DecodiumBridge::cwCanTransmit()').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_cw_remote != new_cw_remote:
        raise SystemExit('CW remote handling differs from baseline')
    print('CW remote handling: A/B source equivalence OK')
    old_cw_admission = section(baseline, 'bool DecodiumBridge::cwCanTransmit()', 'bool DecodiumBridge::cwRigKeyerAvailable() const')
    new_cw_admission = section((ROOT / 'src/bridge/DecodiumBridgeCwRemote.cpp').read_text(), 'bool DecodiumBridge::cwCanTransmit()')
    if old_cw_admission != new_cw_admission:
        raise SystemExit('CW transmit admission differs from baseline')
    print('CW transmit admission: A/B source equivalence OK')
    old_cw_tune = section(baseline, 'bool DecodiumBridge::cwTuneFrequency(', 'bool DecodiumBridge::cwCanTransmit()')
    new_cw_tune = section((ROOT / 'src/bridge/DecodiumBridgeCwRig.cpp').read_text(), 'bool DecodiumBridge::cwTuneFrequency(').replace('decodiumBridgeCwSetCatFrequency(', 'activeCatSetFreq(')
    if old_cw_tune != new_cw_tune:
        raise SystemExit('CW tuning differs from baseline')
    print('CW tuning: A/B source equivalence OK')
    old_qso_completion = section(baseline, 'void DecodiumBridge::clearCompletedQsoTxFields(', '// Sonda delle pause')
    new_qso_completion = section((ROOT / 'src/bridge/DecodiumBridgeQsoCompletion.cpp').read_text(), 'void DecodiumBridge::clearCompletedQsoTxFields(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_qso_completion != new_qso_completion:
        raise SystemExit('QSO completion handling differs from baseline')
    print('QSO completion handling: A/B source equivalence OK')
    old_log_stall = section(baseline, '// Sonda delle pause', 'void DecodiumBridge::logQsoNow()')
    new_log_stall = section((ROOT / 'src/bridge/DecodiumBridgeLogStallProbe.cpp').read_text(), '// Sonda delle pause').replace('void decodiumBridgeStartLogUiStallProbe(', 'static void startLogUiStallProbe(').replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
    if old_log_stall != new_log_stall:
        raise SystemExit('log stall probe differs from baseline')
    print('log stall probe: A/B source equivalence OK')
    old_adif_append = (section(baseline, 'bool DecodiumBridge::appendAdifRecord(', 'QVariantList DecodiumBridge::logbookProfiles() const')
                       + section(baseline, 'bool DecodiumBridge::openActiveLogbookFolder() const', 'bool DecodiumBridge::exportAdif('))
    new_adif_append = section((ROOT / 'src/bridge/DecodiumBridgeAdifAppend.cpp').read_text(), 'bool DecodiumBridge::appendAdifRecord(')
    if old_adif_append != new_adif_append:
        raise SystemExit('ADIF append differs from baseline')
    print('ADIF append: A/B source equivalence OK')
    old_pan_registration = section(baseline, 'void DecodiumBridge::registerPanadapterItem(', 'int         DecodiumBridge::qsoCount()')
    new_pan_registration = section((ROOT / 'src/bridge/DecodiumBridgePanadapterRegistration.cpp').read_text(), 'void DecodiumBridge::registerPanadapterItem(')
    if old_pan_registration != new_pan_registration:
        raise SystemExit('panadapter registration differs from baseline')
    print('panadapter registration: A/B source equivalence OK')
    if section(baseline, 'static int secondsFromUtcDisplayToken(', '\n}\n') != section((ROOT / 'src/bridge/DecodeUtcTime.cpp').read_text(), 'static int secondsFromUtcDisplayToken(', '\n}\n'):
        raise SystemExit('UTC token parser differs from baseline')
    old_secondary = (section(baseline, 'void DecodiumBridge::queueLegacyDecodeSecondaryWork(', 'void DecodiumBridge::resetNativeDecodeDedupIndex(')
                     + section(baseline, 'void DecodiumBridge::queueNativeDecodeSecondaryWork(', 'void DecodiumBridge::invalidateLegacyDecodeModelDeltaFastPath('))
    new_secondary = section((ROOT / 'src/bridge/DecodiumBridgeDecodeSecondary.cpp').read_text(), 'void DecodiumBridge::queueLegacyDecodeSecondaryWork(').replace('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey(').replace('decodiumBridgeSecondsFromUtcDisplayToken(', 'secondsFromUtcDisplayToken(').replace('decodiumBridgeSecondaryDecodeLog(', 'bridgeLog(')
    if old_secondary != new_secondary:
        raise SystemExit('secondary decode queue differs from baseline')
    print('secondary decode queues: A/B source equivalence OK')
    old_delta = (section(baseline, 'static QVariantMap decodeDeltaBoundaryEntry(', 'static DecodeUiSnapshotResult prepareDecodeUiSnapshot(')
                 + section(baseline, 'void DecodiumBridge::invalidateLegacyDecodeModelDeltaFastPath()', 'void DecodiumBridge::syncLegacyBackendDecodeList()'))
    new_delta = section((ROOT / 'src/bridge/DecodiumBridgeLegacyDecodeDelta.cpp').read_text(), 'static QVariantMap decodeDeltaBoundaryEntry(')
    if old_delta != new_delta:
        raise SystemExit('legacy decode delta differs from baseline')
    print('legacy decode delta: A/B source equivalence OK')
    if section(baseline, 'static QDateTime approxUtcDateTimeForDisplayToken(', '\n}\n') != section((ROOT / 'src/bridge/DecodeUtcTime.cpp').read_text(), 'static QDateTime approxUtcDateTimeForDisplayToken(', '\n}\n'):
        raise SystemExit('ALL.TXT timestamp helper differs from baseline')
    old_alltxt = section(baseline, 'QString DecodiumBridge::legacyAllTxtPath() const', 'QString DecodiumBridge::resolveFt8HashPlaceholdersFromRecentAllTxt(')
    new_alltxt = section((ROOT / 'src/bridge/DecodiumBridgeLegacyAllTxt.cpp').read_text(), 'QString DecodiumBridge::legacyAllTxtPath() const', 'QString DecodiumBridge::logAllTxtPath() const').replace('decodiumBridgeApproxUtcDateTimeForDisplayToken(', 'approxUtcDateTimeForDisplayToken(')
    if old_alltxt != new_alltxt:
        raise SystemExit('legacy ALL.TXT path and append differ from baseline')
    print('legacy ALL.TXT path and append: A/B source equivalence OK')
    old_worked = section(baseline, 'static QString workedCallBandModeKey(', 'namespace {\nconst QStringList& decodeColorPropertyNames()')
    new_worked = section((ROOT / 'src/bridge/DecodiumBridgeWorkedHistory.cpp').read_text(), 'QString workedCallBandModeKey(').replace('QString workedCallBandModeKey(', 'static QString workedCallBandModeKey(', 1).replace('QString workedCallBandKey(', 'static QString workedCallBandKey(', 1)
    getter_marker = 'QStringList DecodiumBridge::workedCallsigns() const'
    worked_body, getters = new_worked.split(getter_marker, 1)
    new_worked = worked_body[:-1]
    if (getter_marker + getters).strip() not in baseline: raise SystemExit('worked history accessors differ from baseline')
    if old_worked != new_worked:
        raise SystemExit('worked history implementation differs from baseline')
    print('worked history: A/B source equivalence OK')
    old_psk = (section(baseline, 'bool DecodiumBridge::pskReporterConnected()', 'void DecodiumBridge::setFtThreads(int v)')
               + section(baseline, 'QString DecodiumBridge::pskReporterProgramInfo()', 'void DecodiumBridge::setCallsign('))
    new_psk = section((ROOT / 'src/bridge/DecodiumBridgePskReporter.cpp').read_text(), 'bool DecodiumBridge::pskReporterConnected()')
    for new, old in (('decodiumBridgeUnresolvedGridOnlyDecode(', 'messageIsUnresolvedGridOnlyDecode('), ('decodiumBridgeFst4wUnresolvedHashDecode(', 'isFst4wUnresolvedHashDecode('), ('decodiumBridgePskDebugEnabled(', 'decodiumPskDebugEnabled('), ('decodiumBridgeExtractRightCallsign(', 'extractRightCallsign('), ('decodiumBridgeSecondsFromUtcDisplayToken(', 'secondsFromUtcDisplayToken(')):
        new_psk = new_psk.replace(new, old)
    if old_psk != new_psk:
        raise SystemExit('PSK Reporter bridge implementation differs from baseline')
    print('PSK Reporter bridge: A/B source equivalence OK')
    old_station = section(baseline, 'void DecodiumBridge::setCallsign(const QString& v)', 'double DecodiumBridge::frequency() const')
    new_station = section((ROOT / 'src/bridge/DecodiumBridgeStation.cpp').read_text(), 'void DecodiumBridge::setCallsign(const QString& v)')
    for new, old in (('decodiumBridgeNormalizeWsprPowerDbm(', 'normalizeWsprPowerDbm('), ('decodiumBridgeWsprPowerDbmLabel(', 'wsprPowerDbmLabel('), ('decodiumBridgeCustomCqPrefix(', 'customCqPrefixFromMessage('), ('decodiumBridgeFtxCqWithPrefix(', 'ftxCqMessageWithCustomPrefix('), ('decodiumBridgeStandardFtxCq(', 'standardFtxCqMessage('), ('decodiumBridgeStandardFtxQso(', 'standardFtxQsoMessage('), ('decodiumBridgeStationLog(', 'bridgeLog(')):
        new_station = new_station.replace(new, old)
    if old_station != new_station:
        raise SystemExit('station identity and message generation differs from baseline')
    print('station identity and messages: A/B source equivalence OK')
    old_persist = section(baseline, 'void DecodiumBridge::appendDecodeMapToList(', 'void DecodiumBridge::moveLegacyAllTxtCursorToEnd()')
    new_persist = section((ROOT / 'src/bridge/DecodiumBridgeDecodePersistence.cpp').read_text(), 'void DecodiumBridge::appendDecodeMapToList(')
    for new, old in (('decodiumBridgeIsStreamingKeyboardMode(', 'isStreamingKeyboardMode('), ('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey('), ('decodiumBridgeUnresolvedGridOnlyDecode(', 'messageIsUnresolvedGridOnlyDecode('), ('decodiumBridgeFst4wUnresolvedHashDecode(', 'isFst4wUnresolvedHashDecode('), ('decodiumBridgeDecodePersistenceLog(', 'bridgeLog(')):
        new_persist = new_persist.replace(new, old)
    if old_persist != new_persist:
        raise SystemExit('decode persistence implementation differs from baseline')
    print('decode persistence: A/B source equivalence OK')
    received_source = (ROOT / 'src/bridge/DecodiumBridgeReceivedFiles.cpp').read_text()
    old_received = (section(baseline, 'static QString localDialogPath(', 'QVariantMap DecodiumBridge::readTextFile(')
                    + section(baseline, 'QString DecodiumBridge::defaultFt2LinkReceivedFilesDirectory()', 'bool DecodiumBridge::openExternalUrl('))
    if old_received != section(received_source, 'static QString localDialogPath('):
        raise SystemExit('received files and dialogs differ from baseline')
    variants['received'] = (
        section(old_received, 'static QString localDialogPath(', 'static QString dialogFilterString(')
        + section(old_received, 'QString DecodiumBridge::defaultFt2LinkReceivedFilesDirectory()'),
        section(received_source, 'static QString localDialogPath(', 'static QString dialogFilterString(')
        + section(received_source, 'QString DecodiumBridge::defaultFt2LinkReceivedFilesDirectory()'))
    access_reference = section(baseline, '#ifndef DECODIUM_FT2LINK_ACCESS_SALT_B64',
                               '// Registro dei QSO:')
    flags = shlex.split(run('pkg-config', '--cflags', '--libs', 'Qt6Core',
                            stdout=subprocess.PIPE).stdout)
    compiler = shlex.split(os.environ.get('CXX', 'c++'))
    with tempfile.TemporaryDirectory(prefix='decodium-bridge-ab-') as tmp:
        d = Path(tmp)
        for name, (old, new) in variants.items():
            if old != new:
                raise SystemExit(f'{name}: implementation differs from {args.base}')
            harness = (ROOT / f'tests/bridge_refactor_{name}_harness.cpp').read_text()
            results = []
            for label, implementation in (('A', old), ('B', new)):
                (d / 'bridge_refactor_under_test.inc').write_text(implementation)
                src = d / f'{name}_{label}.cpp'
                src.write_text(harness)
                exe = d / f'{name}_{label}{".exe" if os.name == "nt" else ""}'
                extra = [str(ROOT / 'src/core/helper_functions.cpp'), '-I' + str(ROOT / 'src/core')] if name == 'grid' else (shlex.split(run('pkg-config', '--cflags', '--libs', 'Qt6Network', stdout=subprocess.PIPE).stdout) if name == 'updates' else [])
                run(*compiler, '-std=c++17', str(src), *extra, *flags, '-o', str(exe))
                results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
            if results[0] != results[1]:
                raise SystemExit(f'{name}: A/B outputs differ')
            print(f'{name}: A/B OK\n{results[1]}', end='')
        old_cat_helpers = (section(baseline, 'static QString const kCatProfileRootGroup',
                                   '// "decoport" e\' una modalita\'')
                           + section(baseline, '// "decoport" e\' una modalita\'',
                                     '// Il backend "native" (QSerialPort) e\' stato rimosso:')
                           + section(baseline, 'static QString normalizedCatProfileName(',
                                     '#ifndef DECODIUM_FT2LINK_ACCESS_SALT_B64'))
        old_cat_methods = section(baseline, 'QString DecodiumBridge::activeCatProfile() const',
                                  'QVariantList DecodiumBridge::detectConnectedRigs() const')
        cat_source = (ROOT / 'src/bridge/DecodiumBridgeCatProfiles.cpp').read_text()
        new_cat_helpers = section(cat_source, 'static QString const kCatProfileRootGroup',
                                  'QString DecodiumBridge::activeCatProfile() const')
        new_cat_methods = section(cat_source, 'QString DecodiumBridge::activeCatProfile() const')
        if old_cat_helpers != new_cat_helpers or old_cat_methods != new_cat_methods.replace('decodiumBridgeCatProfileLog(', 'bridgeLog('):
            raise SystemExit('CAT profiles: unexpected implementation changes')
        cat_harness = ROOT / 'tests/bridge_refactor_cat_profiles_harness.cpp'
        cat_results = []
        for label, helpers, methods in (('A', old_cat_helpers, old_cat_methods),
                                        ('B', new_cat_helpers, new_cat_methods)):
            (d / 'bridge_refactor_cat_helpers.inc').write_text(helpers)
            (d / 'bridge_refactor_cat_methods.inc').write_text(methods)
            exe = d / f'cat_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(cat_harness), *flags, '-o', str(exe))
            cat_results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if cat_results[0] != cat_results[1]:
            raise SystemExit('CAT profiles: A/B outputs differ')
        print(f'CAT profiles: A/B OK\n{cat_results[1]}', end='')
        decode_old = section(baseline, 'static QString stripLegacyDecodeAppendage(',
                             'struct DecodeUserFilterConfig')
        decode_new = section((ROOT / 'src/bridge/DecodeMessageNormalization.cpp').read_text(),
                             'QString stripLegacyDecodeAppendage(',
                             '} // namespace decodium::decode_message')
        if decode_old.replace('static QString ', 'QString ').replace('QString* aptype = nullptr', 'QString* aptype') != decode_new:
            raise SystemExit('decode normalization: unexpected implementation changes')
        (d / 'bridge_refactor_decode_under_test.inc').write_text(decode_old)
        harness = ROOT / 'tests/bridge_refactor_decode_normalization_harness.cpp'
        results = []
        for label in ('A', 'B'):
            exe = d / f'decode_{label}{".exe" if os.name == "nt" else ""}'
            ref = ['-DBRIDGE_REFERENCE'] if label == 'A' else [str(ROOT / 'src/bridge/DecodeMessageNormalization.cpp')]
            run(*compiler, '-std=c++17', '-I' + str(d), '-I' + str(ROOT / 'src/bridge'),
                *ref, str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]:
            raise SystemExit('decode normalization: A/B outputs differ')
        print(f'decode normalization: A/B OK\n{results[1]}', end='')
        old_leadin = section(baseline, 'static int txSyncLeadInMsForMode(',
                             'static int legacyTxAttenuationFromLevel(')
        bridge_current = (ROOT / 'src/bridge/DecodiumBridgeTxPolicy.cpp').read_text()
        new_leadin = section(bridge_current, 'int txSyncLeadInMsForMode(',
                             'int decodiumBridgeTxSyncLeadInMsForMode(').replace('int txSyncLeadInMsForMode(', 'static int txSyncLeadInMsForMode(', 1)
        if old_leadin != new_leadin:
            raise SystemExit('TX timing: lead-in implementation changed')
        old_timing = section(baseline, 'qint64 DecodiumBridge::syncTxPcmStartOffsetBytes(',
                             'void DecodiumBridge::scheduleSyncTxBoundaryStop(')
        new_timing = section((ROOT / 'src/bridge/DecodiumBridgeTxTiming.cpp').read_text(),
                             'qint64 DecodiumBridge::syncTxPcmStartOffsetBytes(',
                             'bool DecodiumBridge::shouldAlignTxAudioToCurrentSyncSlot() const')
        if old_timing != new_timing.replace('decodiumBridgeTxSyncLeadInMsForMode(', 'txSyncLeadInMsForMode('):
            raise SystemExit('TX timing: unexpected implementation changes')
        (d / 'bridge_refactor_tx_leadin.inc').write_text(old_leadin)
        harness = ROOT / 'tests/bridge_refactor_tx_timing_harness.cpp'
        media_flags = shlex.split(run('pkg-config', '--cflags', '--libs', 'Qt6Multimedia', stdout=subprocess.PIPE).stdout)
        results = []
        for label, timing in (('A', old_timing), ('B', new_timing)):
            (d / 'bridge_refactor_tx_timing.inc').write_text(timing)
            exe = d / f'tx_timing_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, *media_flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]:
            raise SystemExit('TX timing: A/B outputs differ')
        print(f'TX timing: A/B OK\n{results[1]}', end='')
        old_keyboard_mode = section(baseline, 'static bool isStreamingKeyboardMode(',
                                    '// JTTY e CW tengono la cattura PCM')
        current_bridge = (ROOT / 'src/bridge/DecodiumBridge.cpp').read_text()
        new_keyboard_mode = section((ROOT / 'src/bridge/DecodiumBridgeApplicationModes.cpp').read_text(), 'static bool isStreamingKeyboardMode(',
                                    '// JTTY e CW tengono la cattura PCM')
        if old_keyboard_mode != new_keyboard_mode:
            raise SystemExit('keyboard mode: implementation changed')
        old_keyboard = section(baseline, 'bool DecodiumBridge::rttyCanTransmit()',
                               '// Il CW come audio parte')
        new_keyboard = section((ROOT / 'src/bridge/DecodiumBridgeKeyboardAudio.cpp').read_text(),
                               'bool DecodiumBridge::rttyCanTransmit()')
        if old_keyboard != new_keyboard.replace('decodiumBridgeIsStreamingKeyboardMode(', 'isStreamingKeyboardMode(').replace('decodiumBridgeKeyboardAudioLog(', 'bridgeLog('):
            raise SystemExit('keyboard audio: unexpected implementation changes')
        (d / 'bridge_refactor_keyboard_mode.inc').write_text(old_keyboard_mode)
        harness = ROOT / 'tests/bridge_refactor_keyboard_audio_harness.cpp'
        results = []
        for label, code in (('A', old_keyboard), ('B', new_keyboard)):
            (d / 'bridge_refactor_keyboard_audio.inc').write_text(code)
            exe = d / f'keyboard_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]:
            raise SystemExit('keyboard audio: A/B outputs differ')
        print(f'keyboard audio: A/B OK\n{results[1]}', end='')
        old_queue = section(baseline, 'void DecodiumBridge::processNextInQueue()',
                            '// ============================================================\n// B9 — Active Stations')
        new_queue = section((ROOT / 'src/bridge/DecodiumBridgeCallerQueue.cpp').read_text(),
                            'void DecodiumBridge::processNextInQueue()')
        if old_queue != new_queue.replace('decodiumBridgeCallerQueueLog(', 'bridgeLog('):
            raise SystemExit('caller queue: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_caller_queue_harness.cpp'
        results = []
        for label, code in (('A', old_queue), ('B', new_queue)):
            (d / 'bridge_refactor_caller_queue.inc').write_text(code)
            exe = d / f'queue_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), '-I' + str(ROOT), '-I' + str(ROOT / 'src/radio'), '-I' + str(ROOT / 'src/core'), str(harness), str(ROOT / 'src/radio/Radio.cpp'), str(ROOT / 'Sequencer/MessageTokenRules.cpp'), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]:
            raise SystemExit('caller queue: A/B outputs differ')
        print(f'caller queue: A/B OK\n{results[1]}', end='')
        old_windows = section(baseline, 'QVariantMap DecodiumBridge::loadWindowState(',
                              '// =====================================================================\n// === Phase 5.3')
        new_windows = section((ROOT / 'src/bridge/DecodiumBridgeWindowState.cpp').read_text(),
                              'QVariantMap DecodiumBridge::loadWindowState(')
        normalized_windows = new_windows.replace('decodiumBridgeSettingsWriteThreadPool()', 'settingsWriteThreadPool()').replace('decodiumBridgeWindowStateLog(', 'bridgeLog(')
        if old_windows != normalized_windows:
            raise SystemExit('window state: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_window_state_harness.cpp'
        results = []
        for label, code in (('A', old_windows), ('B', new_windows)):
            (d / 'bridge_refactor_window_state.inc').write_text(code)
            exe = d / f'windows_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]:
            raise SystemExit('window state: A/B outputs differ')
        print(f'window state: A/B OK\n{results[1]}', end='')
        old_colors = section(baseline, 'namespace {\nconst QStringList& decodeColorPropertyNames()', 'QString DecodiumBridge::pskReporterProgramInfo()')
        new_colors = section((ROOT / 'src/bridge/DecodiumBridgeDecodeColors.cpp').read_text(), 'namespace decodium::decode_colors {')
        normalized_colors = new_colors.replace('namespace decodium::decode_colors {', 'namespace {', 1).replace('using namespace decodium::decode_colors;\n\n', '', 1)
        if old_colors != normalized_colors:
            raise SystemExit('decode colors: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_decode_colors_harness.cpp'
        results = []
        for label, code in (('A', old_colors), ('B', normalized_colors)):
            (d / 'bridge_refactor_decode_colors.inc').write_text(code)
            exe = d / f'decode_colors_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]:
            raise SystemExit('decode colors: A/B outputs differ')
        print(f'decode colors: A/B OK\n{results[1]}', end='')
        old_dedup = section(baseline, 'void DecodiumBridge::resetNativeDecodeDedupIndex()', 'void DecodiumBridge::queueNativeDecodeSecondaryWork(')
        new_dedup = section((ROOT / 'src/bridge/DecodiumBridgeDecodeDedup.cpp').read_text(), 'void DecodiumBridge::resetNativeDecodeDedupIndex()')
        normalized_dedup = new_dedup.replace('decodiumBridgeFt2AsyncDedupKey(', 'decodeDedupKeyFt2Async(').replace('decodiumBridgeDedupKey(', 'decodeDedupKey(').replace('decodiumBridgeMirrorEntryKey(', 'decodeMirrorEntryKey(')
        if old_dedup != normalized_dedup:
            raise SystemExit('decode dedup: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_decode_dedup_harness.cpp'
        results = []
        for label, code in (('A', old_dedup), ('B', normalized_dedup)):
            (d / 'bridge_refactor_decode_dedup.inc').write_text(code)
            exe = d / f'decode_dedup_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]:
            raise SystemExit('decode dedup: A/B outputs differ')
        print(f'decode dedup: A/B OK\n{results[1]}', end='')
        def extract_void_method(source, name):
            start = source.index('void DecodiumBridge::' + name + '(')
            opening = source.index('{', start)
            depth = 0
            for end in range(opening, len(source)):
                if source[end] == '{': depth += 1
                elif source[end] == '}':
                    depth -= 1
                    if not depth: return source[start:end + 1] + '\n\n'
            raise ValueError('unterminated method ' + name)
        profile_methods = ('setFt2Conservative', 'applyReadyProfile', 'clearActiveReadyProfileOnManualChange', 'setAutoCqMaxCycles', 'setAutoCqPauseSec', 'setAutoCqBurstCalls', 'setAutoCqListenCycles')
        old_profiles = ''.join(extract_void_method(baseline, name) for name in profile_methods)
        profile_source = (ROOT / 'src/bridge/DecodiumBridgeFtxProfiles.cpp').read_text().replace('decodiumBridgeFtxSettingsLog(', 'bridgeLog(')
        new_profiles = ''.join(extract_void_method(profile_source, name) for name in profile_methods)
        if old_profiles != new_profiles: raise SystemExit('FTX profiles: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_ftx_profiles_harness.cpp'
        results = []
        for label, code in (('A', old_profiles), ('B', new_profiles)):
            (d / 'bridge_refactor_ftx_profiles.inc').write_text(code)
            exe = d / f'ftx_profiles_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]: raise SystemExit('FTX profiles: A/B outputs differ')
        print(f'FTX profiles: A/B OK\n{results[1]}', end='')
        old_tx_messages = section(baseline, 'QString DecodiumBridge::tx1() const', 'void DecodiumBridge::armExplicitManualTxRequest(') + section(baseline, 'QString DecodiumBridge::txMessage(int n) const', 'void DecodiumBridge::stopTx()')
        new_tx_messages = section((ROOT / 'src/bridge/DecodiumBridgeTxMessages.cpp').read_text(), 'QString DecodiumBridge::tx1() const', '\nvoid decodiumBridgeAudioLevelLog').replace('decodiumBridgeCustomCqPrefix(', 'customCqPrefixFromMessage(').replace('decodiumBridgeStandardFtxCq(', 'standardFtxCqMessage(').replace('decodiumBridgeGeneratedTxWaveMode(', 'bridgeGeneratedTxWaveMode(')
        if old_tx_messages != new_tx_messages: raise SystemExit('TX messages: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_tx_messages_harness.cpp'
        results = []
        for label, code in (('A', old_tx_messages), ('B', new_tx_messages)):
            (d / 'bridge_refactor_tx_messages.inc').write_text(code)
            exe = d / f'tx_messages_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]: raise SystemExit('TX messages: A/B outputs differ')
        print(f'TX messages: A/B OK\n{results[1]}', end='')
        old_logbook_settings = section(baseline, 'QString DecodiumBridge::effectiveAdifLogPath() const', 'void DecodiumBridge::reloadActiveLogbookState(')
        new_logbook_settings = section((ROOT / 'src/bridge/DecodiumBridgeLogbookProfiles.cpp').read_text(), 'QString DecodiumBridge::effectiveAdifLogPath() const', 'QVariantList DecodiumBridge::logbookProfiles() const').replace('decodiumBridgeLogbookLog(', 'bridgeLog(')
        if old_logbook_settings != new_logbook_settings: raise SystemExit('logbook settings: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_logbook_settings_harness.cpp'
        results = []
        for label, code in (('A', old_logbook_settings), ('B', new_logbook_settings)):
            (d / 'bridge_refactor_logbook_settings.inc').write_text(code)
            exe = d / f'logbook_settings_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]: raise SystemExit('logbook settings: A/B outputs differ')
        print(f'logbook settings: A/B OK\n{results[1]}', end='')
        old_logbooks = section(baseline, 'QVariantList DecodiumBridge::logbookProfiles() const', 'bool DecodiumBridge::openActiveLogbookFolder() const')
        new_logbooks = section((ROOT / 'src/bridge/DecodiumBridgeLogbookProfiles.cpp').read_text(), 'QVariantList DecodiumBridge::logbookProfiles() const', '\nextern QString decodiumBridgeBandFromFreqHz').replace('decodiumBridgeWriteAdifDocument(', 'writeAdifDocument(').replace('decodiumBridgeCountAdifRecords(', 'countAdifRecordsLightweight(')
        if old_logbooks != new_logbooks: raise SystemExit('logbook profiles: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_logbook_profiles_harness.cpp'
        results = []
        for label, code in (('A', old_logbooks), ('B', new_logbooks)):
            (d / 'bridge_refactor_logbook_profiles.inc').write_text(code)
            exe = d / f'logbook_profiles_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), '-I' + str(ROOT / 'src/bridge'), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]: raise SystemExit('logbook profiles: A/B outputs differ')
        print(f'logbook profiles: A/B OK\n{results[1]}', end='')
        old_timesync = section(baseline, 'void DecodiumBridge::applyNtpSettings()', 'void DecodiumBridge::resetStartupTransientQsoState()')
        new_timesync = section((ROOT / 'src/bridge/DecodiumBridgeTimeSync.cpp').read_text(), 'void DecodiumBridge::applyNtpSettings()', 'void DecodiumBridge::syncNtpNow()').replace('decodiumBridgeNtpLog(', 'bridgeLog(')
        if old_timesync != new_timesync: raise SystemExit('time sync: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_timesync_harness.cpp'
        results = []
        for label, code in (('A', old_timesync), ('B', new_timesync)):
            (d / 'bridge_refactor_timesync.inc').write_text(code)
            exe = d / f'timesync_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]: raise SystemExit('time sync: A/B outputs differ')
        print(f'time sync: A/B OK\n{results[1]}', end='')
        ntp_shared = d / f'ntp_shared{".exe" if os.name == "nt" else ""}'
        run(*compiler, '-std=c++17', '-I' + str(ROOT / 'src/core'),
            str(ROOT / 'tests/bridge_refactor_ntp_shared_writer.cpp'),
            str(ROOT / 'tests/bridge_refactor_ntp_shared_reader.cpp'), *flags, '-o', str(ntp_shared))
        print(run(str(ntp_shared), stdout=subprocess.PIPE).stdout, end='')
        old_adif_export = section(baseline, 'bool DecodiumBridge::exportAdif(', '// ============================================================\n// US state data')
        new_adif_export = section((ROOT / 'src/bridge/DecodiumBridgeLogbookProfiles.cpp').read_text(), 'bool DecodiumBridge::exportAdif(', '\nvoid DecodiumBridge::reloadActiveLogbookState(').replace('decodiumBridgeBandFromFreqHz(', 'bandFromFreqHz(').replace('decodiumBridgeUtcTimeZone()', 'decodiumUtcTimeZone()')
        if old_adif_export.rstrip() != new_adif_export.rstrip(): raise SystemExit('ADIF export: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_adif_export_harness.cpp'
        results = []
        for label, code in (('A', old_adif_export), ('B', new_adif_export)):
            (d / 'bridge_refactor_adif_export.inc').write_text(code)
            exe = d / f'adif_export_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]: raise SystemExit('ADIF export: A/B outputs differ')
        print(f'ADIF export: A/B OK\n{results[1]}', end='')
        old_sstv_storage_source = run('git', 'show', f'{args.base}:src/bridge/DecodiumBridgeSstv.cpp', cwd=ROOT, stdout=subprocess.PIPE).stdout
        old_sstv_storage = section(old_sstv_storage_source, 'bool DecodiumBridge::sstvRxAudioJobBusy() const noexcept', 'QVariantMap DecodiumBridge::sstvRxDiagnostics() const') + section(old_sstv_storage_source, 'bool DecodiumBridge::sstvStorageReady() const noexcept', 'void DecodiumBridge::initialiseSstvRuntime()')
        new_sstv_storage = section((ROOT / 'src/bridge/DecodiumBridgeSstvStorageState.cpp').read_text(), 'bool DecodiumBridge::sstvRxAudioJobBusy() const noexcept')
        if old_sstv_storage != new_sstv_storage: raise SystemExit('SSTV storage state: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_sstv_storage_harness.cpp'
        for enabled in (0, 1):
            results = []
            for label, code in (('A', old_sstv_storage), ('B', new_sstv_storage)):
                (d / 'bridge_refactor_sstv_storage.inc').write_text(code)
                exe = d / f'sstv_storage_{enabled}_{label}{".exe" if os.name == "nt" else ""}'
                run(*compiler, '-std=c++17', f'-DDECODIUM_HAS_SSTV={enabled}', '-I' + str(d), str(harness), *flags, '-o', str(exe))
                results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
            if results[0] != results[1]: raise SystemExit('SSTV storage state: A/B outputs differ')
            print(f'SSTV storage state enabled={enabled}: A/B OK\n{results[1]}', end='')
        old_autocq = section(baseline, 'bool DecodiumBridge::autoCqCanRestartTx6() const', 'bool DecodiumBridge::isRecentAutoCqDuplicate(')
        new_autocq = section((ROOT / 'src/bridge/DecodiumBridgeAutoCqCadence.cpp').read_text(), 'bool DecodiumBridge::autoCqCanRestartTx6() const').replace('decodiumBridgeAutoCqLog(', 'bridgeLog(')
        if old_autocq != new_autocq: raise SystemExit('AutoCQ cadence: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_autocq_cadence_harness.cpp'
        results = []
        for label, code in (('A', old_autocq), ('B', new_autocq)):
            (d / 'bridge_refactor_autocq_cadence.inc').write_text(code)
            exe = d / f'autocq_cadence_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), '-I' + str(ROOT), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]: raise SystemExit('AutoCQ cadence: A/B outputs differ')
        print(f'AutoCQ cadence: A/B OK\n{results[1]}', end='')
        old_adif_doc = section(baseline, 'constexpr int kMaxAdifRecords = 200000;', '// Map a frequency in Hz to the standard band token')
        new_adif_doc = section((ROOT / 'src/bridge/AdifDocumentInternal.cpp').read_text(), 'constexpr int kMaxAdifRecords = 200000;', 'QString adifDigitsOnly(')
        normalized_old_adif_doc = old_adif_doc.replace('static QList<ParsedAdifRecord> parseAdifRecordsBytes(', 'QList<ParsedAdifRecord> parseAdifRecordsBytes(').replace('static ParsedAdifDocument loadAdifDocument(', 'ParsedAdifDocument loadAdifDocument(').replace('static bool writeAdifDocument(', 'bool writeAdifDocument(')
        if normalized_old_adif_doc.rstrip() != new_adif_doc.rstrip(): raise SystemExit('ADIF document: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_adif_document_harness.cpp'
        (d / 'bridge_refactor_adif_document.inc').write_text(old_adif_doc)
        results = []
        for label in ('A', 'B'):
            exe = d / f'adif_document_{label}{".exe" if os.name == "nt" else ""}'
            sources = [str(harness), str(ROOT / 'src/bridge/AdifExportSanitizer.cpp')]
            if label == 'B': sources.append(str(ROOT / 'src/bridge/AdifDocumentInternal.cpp'))
            defs = ['-DADIF_BASELINE=1'] if label == 'A' else []
            run(*compiler, '-std=c++17', *defs, '-I' + str(d), '-I' + str(ROOT / 'src/bridge'), *sources, *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]: raise SystemExit('ADIF document: A/B outputs differ')
        print(f'ADIF document: A/B OK\n{results[1]}', end='')
        adif_normalization_names = ('adifDigitsOnly', 'normalizedAdifDateToken', 'normalizedAdifTimeToken', 'normalizeAdifModeForDisplay', 'normalizedAdifDateTime', 'adifFrequencyHzFromFields', 'adifBandFromFields', 'normalizeImportedAdifRecord', 'adifRecordDedupeKey', 'freqHzToBandToken')
        def extract_adif_function(source, name, old):
            pattern = (r'static ' if old else '') + r'(?:QString|quint64|ParsedAdifRecord) ' + name + r'\('
            match = None
            for candidate in re.finditer(pattern, source):
                opening = source.index('{', candidate.end())
                semicolon = source.find(';', candidate.end(), opening)
                if semicolon < 0:
                    match = candidate
                    break
            if not match: raise ValueError('missing ADIF function ' + name)
            start = match.start(); opening = source.index('{', match.end()); depth = 0
            for end in range(opening, len(source)):
                if source[end] == '{': depth += 1
                elif source[end] == '}':
                    depth -= 1
                    if not depth: return source[start:end + 1] + '\n\n'
            raise ValueError('unterminated ADIF function ' + name)
        normalization_source = (ROOT / 'src/bridge/AdifDocumentInternal.cpp').read_text()
        old_normalization = ''.join(extract_adif_function(baseline, name, True) for name in adif_normalization_names)
        new_normalization = ''.join(extract_adif_function(normalization_source, name, False) for name in adif_normalization_names)
        if old_normalization.replace('static ', '') != new_normalization: raise SystemExit('ADIF normalization: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_adif_normalization_harness.cpp'
        (d / 'bridge_refactor_adif_normalization.inc').write_text('static QString freqHzToBandToken(quint64);\n' + old_normalization)
        results = []
        for label in ('A', 'B'):
            exe = d / f'adif_normalization_{label}{".exe" if os.name == "nt" else ""}'
            sources = [str(harness)]
            if label == 'B': sources += [str(ROOT / 'src/bridge/AdifDocumentInternal.cpp'), str(ROOT / 'src/bridge/AdifExportSanitizer.cpp')]
            defs = ['-DADIF_BASELINE=1'] if label == 'A' else []
            run(*compiler, '-std=c++17', *defs, '-I' + str(d), '-I' + str(ROOT / 'src/bridge'), *sources, *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]: raise SystemExit('ADIF normalization: A/B outputs differ')
        print(f'ADIF normalization: A/B OK\n{results[1]}', end='')
        old_adif_import = section(baseline, 'using AdifImportProgressCallback =', 'void DecodiumBridge::importConfirmedAdifIntoLogbookAsync(')
        new_adif_import = section((ROOT / 'src/bridge/AdifImportInternal.cpp').read_text(), 'static bool copyFileAtomically(', '} // namespace decodium::adif_document')
        normalized_old_import = old_adif_import.split('\n\n', 1)[1].replace('static QVariantMap importAdifDocumentInBackground(', 'QVariantMap importAdifDocumentInBackground(').replace('static QVariantMap importConfirmedAdifDocumentInBackground(', 'QVariantMap importConfirmedAdifDocumentInBackground(')
        if normalized_old_import != new_adif_import: raise SystemExit('ADIF import: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_adif_import_harness.cpp'
        (d / 'bridge_refactor_adif_import.inc').write_text(old_adif_import)
        results = []
        for label in ('A', 'B'):
            exe = d / f'adif_import_{label}{".exe" if os.name == "nt" else ""}'
            sources = [str(harness), str(ROOT / 'src/bridge/AdifDocumentInternal.cpp'), str(ROOT / 'src/bridge/AdifExportSanitizer.cpp')]
            if label == 'B': sources.append(str(ROOT / 'src/bridge/AdifImportInternal.cpp'))
            defs = ['-DADIF_BASELINE=1'] if label == 'A' else []
            run(*compiler, '-std=c++17', *defs, '-I' + str(d), '-I' + str(ROOT / 'src/bridge'), *sources, *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]: raise SystemExit('ADIF import: A/B outputs differ')
        print(f'ADIF import: A/B OK\n{results[1]}', end='')

        old_async = section(baseline, 'void DecodiumBridge::importConfirmedAdifIntoLogbookAsync(', 'bool DecodiumBridge::exportSelectedToAdif(')
        new_async = section((ROOT / 'src/bridge/DecodiumBridgeAdifImport.cpp').read_text(), 'void DecodiumBridge::importConfirmedAdifIntoLogbookAsync(', None)
        if old_async.replace('bridgeLog(', 'decodiumBridgeLogbookLog(') != new_async:
            raise SystemExit('ADIF async bridge methods differ from baseline')
        print('ADIF async bridge methods: A/B source equivalence OK')

        old_editing = section(baseline, 'bool DecodiumBridge::exportSelectedToAdif(', 'void DecodiumBridge::setCloudlogEnabled(')
        new_editing = section((ROOT / 'src/bridge/DecodiumBridgeAdifEditing.cpp').read_text(), 'bool DecodiumBridge::exportSelectedToAdif(', None)
        if old_editing != new_editing:
            raise SystemExit('ADIF editing bridge methods differ from baseline')
        print('ADIF editing bridge methods: A/B source equivalence OK')
        for begin, end in (('static void rebuildWorkedCallsFromDocument(', 'static int countAdifRecordsLightweight('), ('static QString exportRecordFingerprint(', 'static QsoLogSnapshot buildQsoLogSnapshot('), ('static void setAdifModeFields(', '// 1.0.495')):
            old_helper = section(baseline, begin, end).replace('static ', '', 1)
            current_end = end.replace('static ', '', 1) if end == 'static QsoLogSnapshot buildQsoLogSnapshot(' else end
            new_helper = section((ROOT / ('src/bridge/DecodiumQsoSnapshot.cpp' if begin.startswith('static QString exportRecordFingerprint(') else 'src/bridge/DecodiumAdifSupport.cpp')).read_text(), begin.replace('static ', '', 1), None if begin.startswith('static void setAdifModeFields(') else current_end)
            if old_helper != new_helper:
                raise SystemExit(f'ADIF helper changed: {begin}')
        print('ADIF editing shared helpers: A/B source equivalence OK')

        old_advanced = section(baseline, '// === ADV AUTO-MODE ===', '// ── FT2 Log Bridge')
        new_advanced = section((ROOT / 'src/bridge/DecodiumBridgeAdvancedDecode.cpp').read_text(), '// === ADV AUTO-MODE ===', None)
        if old_advanced != new_advanced: raise SystemExit('advanced decode methods differ from baseline')
        print('Advanced decode methods: A/B source equivalence OK')

        old_panfft = section(baseline, 'void DecodiumBridge::setGpuPanadapterFftAvailable(', 'void DecodiumBridge::registerPanadapterItem(')
        new_panfft = section((ROOT / 'src/bridge/DecodiumBridgePanadapterFft.cpp').read_text(), 'void DecodiumBridge::setGpuPanadapterFftAvailable(', None)
        if old_panfft != new_panfft: raise SystemExit('panadapter FFT state methods differ from baseline')
        print('Panadapter FFT state methods: A/B source equivalence OK')

        old_uiprefs = section(baseline, 'QString DecodiumBridge::uiQuality() const', 'void DecodiumBridge::aggiungiRigaRtty')
        new_uiprefs = section((ROOT / 'src/bridge/DecodiumBridgeUiPreferences.cpp').read_text(), 'QString DecodiumBridge::uiQuality() const', None)
        if old_uiprefs.replace('bridgeLog(', 'decodiumBridgeLogbookLog(') != new_uiprefs: raise SystemExit('UI preference methods differ from baseline')
        print('UI preferences: A/B source equivalence OK')

        timing_file = (ROOT / 'src/bridge/DecodiumBridgeDecodeTiming.cpp').read_text()
        for start,end in (('bool DecodiumBridge::isTimeSyncDecodeMode(', 'bool DecodiumBridge::ft8LiveDecodeBacklogActive('), ('void DecodiumBridge::resetTimeSyncDecodeMetrics()', 'QString DecodiumBridge::mode() const')):
            old_timing = section(baseline,start,end)
            new_timing = section(timing_file,start, 'void DecodiumBridge::resetTimeSyncDecodeMetrics()' if start.startswith('bool ') else None)
            if old_timing != new_timing: raise SystemExit(f'decode timing changed: {start}')
        old_timing = section(baseline,'bool DecodiumBridge::isTimeSyncDecodeMode(', 'bool DecodiumBridge::ft8LiveDecodeBacklogActive(') + section(baseline,'void DecodiumBridge::resetTimeSyncDecodeMetrics()', 'QString DecodiumBridge::mode() const')
        new_timing = section(timing_file,'bool DecodiumBridge::isTimeSyncDecodeMode(',None)
        timing_results=[]
        for label, body in (('A', old_timing), ('B', new_timing)):
            (d / 'bridge_refactor_decode_timing.inc').write_text(body)
            exe=d / f'decode_timing_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I'+str(d), str(ROOT/'tests/bridge_refactor_decode_timing_harness.cpp'), *flags, '-o', str(exe))
            timing_results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if timing_results[0] != timing_results[1]: raise SystemExit('decode timing: A/B outputs differ')
        print(f'decode timing: A/B OK\n{timing_results[1]}',end='')

        old_samples=section(baseline,'int DecodiumBridge::minimumDecodeSamplesForMode(', 'void DecodiumBridge::applyNtpSettings()')
        new_samples=section(timing_file,'int DecodiumBridge::minimumDecodeSamplesForMode(', 'bool DecodiumBridge::isTimeSyncDecodeMode(')
        if old_samples!=new_samples:raise SystemExit('decode sample windows differ from baseline')
        sample_results=[]
        for label,body in (('A',old_samples),('B',new_samples)):
            (d/'bridge_refactor_decode_samples.inc').write_text(body)
            exe=d/f'decode_samples_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_decode_samples_harness.cpp'),*flags,'-o',str(exe))
            sample_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if sample_results[0]!=sample_results[1]:raise SystemExit('decode sample windows A/B differ')
        print(f'decode sample windows: A/B OK\n{sample_results[1]}',end='')

        old_period=section(baseline,'int DecodiumBridge::effectivePeriodMsForMode(', 'QString DecodiumBridge::utcTime() const')
        new_period=section(timing_file,'int DecodiumBridge::effectivePeriodMsForMode(', 'int DecodiumBridge::minimumDecodeSamplesForMode(')
        if old_period!=new_period:raise SystemExit('decode period methods differ from baseline')
        period_results=[]
        for label,body in (('A',old_period),('B',new_period)):
            (d/'bridge_refactor_decode_period.inc').write_text(body)
            exe=d/f'decode_period_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_decode_period_harness.cpp'),*flags,'-o',str(exe))
            period_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if period_results[0]!=period_results[1]:raise SystemExit('decode period A/B differ')
        print(f'decode period: A/B OK\n{period_results[1]}',end='')

        old_wav=section(baseline,'void DecodiumBridge::openWavForDecode(', '// ============================================================\n// B11 — Cabrillo')
        new_wav=section((ROOT/'src/bridge/DecodiumBridgeWavDecode.cpp').read_text(),'void DecodiumBridge::openWavForDecode(',None)
        if old_wav != new_wav: raise SystemExit('WAV decode methods differ from baseline')
        wav_results=[]
        for label,body in (('A',old_wav),('B',new_wav)):
            (d/'bridge_refactor_wav_decode.inc').write_text(body)
            exe=d/f'wav_decode_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_wav_decode_harness.cpp'),*flags,'-o',str(exe))
            wav_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if wav_results[0]!=wav_results[1]:raise SystemExit('WAV decode A/B outputs differ')
        print(f'WAV decode: A/B OK\n{wav_results[1]}',end='')

        font_source=(ROOT/'src/bridge/DecodiumBridgeFontSettings.cpp').read_text()
        for start, old_end, new_end in (('static QString resolveBridgeFontFamily(', 'static QString stripLegacyDecodeAppendage(', 'QFont DecodiumBridge::fontSettingFont('), ('QFont DecodiumBridge::fontSettingFont(', '// ---------------------------------------------------------------------------\n// Legacy INI sync helpers', '\nvoid DecodiumBridge::increaseFontScale()')):
            if section(baseline,start,old_end)!=section(font_source,start,new_end):raise SystemExit(f'font settings changed: {start}')
        print('Font settings: A/B source equivalence OK')

        old_prompt=section(baseline,'void DecodiumBridge::setNextLogClusterSpotEnabled(', 'void DecodiumBridge::requestSafeGraphicsNextLaunch(')
        new_prompt=section((ROOT/'src/bridge/DecodiumBridgeLogPrompt.cpp').read_text(),'void DecodiumBridge::setNextLogClusterSpotEnabled(',None)
        old_normalized=old_prompt.replace('static QString logPromptDateTimeText(', 'QString decodiumBridgeLogPromptDateTimeText(')
        # Accept only the deliberate UTC-suffix fix: strip the suffix before replacing the ISO separator.
        old_order="    clean.replace(QChar('T'), QChar(' '));\n    clean.replace(QRegularExpression(QStringLiteral(\"\\\\s*(UTC|Z)\\\\s*$\"),\n                                     QRegularExpression::CaseInsensitiveOption),\n                  QString());"
        new_order="    clean.replace(QRegularExpression(QStringLiteral(\"\\\\s*(UTC|Z)\\\\s*$\"),\n                                     QRegularExpression::CaseInsensitiveOption),\n                  QString());\n    clean.replace(QChar('T'), QChar(' '));"
        if old_normalized.replace(old_order,new_order)!=new_prompt: raise SystemExit('log prompt methods differ beyond UTC suffix fix')
        prompt_results=[]
        for label,body in (('A',old_prompt),('B',new_prompt)):
            (d/'bridge_refactor_log_prompt.inc').write_text(body)
            exe=d/f'log_prompt_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_log_prompt_harness.cpp'),*flags,'-o',str(exe))
            prompt_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if prompt_results[0]!=prompt_results[1]:raise SystemExit('log prompt A/B outputs differ')
        print(f'log prompt: A/B OK\n{prompt_results[1]}',end='')

        old_record=section(baseline,'static QString bandFromFreqHz(double hz)', 'bool DecodiumBridge::appendAdifRecord(')
        new_record=section((ROOT/'src/bridge/DecodiumBridgeAdifRecord.cpp').read_text(),'static QString bandFromFreqHz(double hz)',None)
        new_record=new_record.replace('QString decodiumBridgeBandFromFreqHz(double hz)\n{\n    return bandFromFreqHz(hz);\n}\n\n','').replace('QString bridgeAdifRecordText(', 'static QString bridgeAdifRecordText(').replace('decodiumBridgeParseFrequencyHzValue(', 'parseFrequencyHzValue(')
        if old_record != new_record: raise SystemExit('ADIF record serialization differs from baseline')
        print('ADIF record serialization: A/B source equivalence OK')
        (d/'Radio.hpp').write_text('#pragma once\n#include <QtGlobal>\nnamespace Radio { using Frequency=quint64; }\n')
        (d/'models').mkdir(exist_ok=True)
        (d/'models/Bands.hpp').write_text('#pragma once\n#include <QString>\n#include "Radio.hpp"\nclass Bands { public: QString find(Radio::Frequency f) const { if(f>=14000000 && f<=14350000)return "20m";if(f>=144000000 && f<=148000000)return "2m";return {}; } };\n')
        (d/'bridge_refactor_frequency.inc').write_text(section(baseline,'Radio::Frequency parseFrequencyHzValue(', 'Radio::FrequencyDelta parseFrequencyOffsetHzValue('))
        (d/'bridge_refactor_adif_record.inc').write_text(old_record)
        record_results=[]
        for label in ('A','B'):
            exe=d/f'adif_record_{label}{".exe" if os.name == "nt" else ""}'
            sources=['-DBRIDGE_REFERENCE'] if label=='A' else [str(ROOT/'src/bridge/DecodiumBridgeAdifRecord.cpp')]
            run(*compiler,'-std=c++17','-I'+str(d),'-I'+str(ROOT/'src/bridge'),*sources,str(ROOT/'tests/bridge_refactor_adif_record_harness.cpp'),str(ROOT/'src/bridge/AdifExportSanitizer.cpp'),*flags,'-o',str(exe))
            record_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if record_results[0]!=record_results[1]:raise SystemExit('ADIF record A/B output differs')
        print(f'ADIF record: A/B OK\n{record_results[1]}',end='')

        old_history=section(baseline,'// =====================================================================\n// === Phase 5.3', 'QVariantMap DecodiumBridge::primaryScreenAvailableGeometry() const')
        new_history=section((ROOT/'src/bridge/DecodiumBridgeDecodeHistory.cpp').read_text(),'// =====================================================================\n// === Phase 5.3',None)
        if old_history!=new_history.replace('decodiumBridgeLogbookLog(', 'bridgeLog('):raise SystemExit('decode history methods differ from baseline')
        history_results=[]
        sqlflags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Sql'],text=True))
        for label,body in (('A',old_history),('B',new_history)):
            (d/'bridge_refactor_decode_history.inc').write_text(body)
            exe=d/f'decode_history_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-DFORK_RELEASE_VERSION="1.0.737"','-I'+str(d),str(ROOT/'tests/bridge_refactor_decode_history_harness.cpp'),*sqlflags,'-o',str(exe))
            history_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if history_results[0]!=history_results[1]:raise SystemExit('decode history A/B results differ')
        print(f'decode history: A/B OK\n{history_results[1]}',end='')

        old_mamplan=section(baseline,'QVariantMap DecodiumBridge::digiPileupStatus() const','QVariantList DecodiumBridge::mamActiveSlots() const')
        new_mamplan=section((ROOT/'src/bridge/DecodiumBridgeMamFrequencyPlan.cpp').read_text(),'QVariantMap DecodiumBridge::digiPileupStatus() const',None)
        if old_mamplan!=new_mamplan:raise SystemExit('MAM frequency plan differs from baseline')
        mamplan_results=[]
        for label,body in (('A',old_mamplan),('B',new_mamplan)):
            (d/'bridge_refactor_mam_plan.inc').write_text(body)
            exe=d/f'mam_plan_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_mam_plan_harness.cpp'),*flags,'-o',str(exe))
            mamplan_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if mamplan_results[0]!=mamplan_results[1]:raise SystemExit('MAM frequency plan A/B results differ')
        print(f'MAM frequency plan: A/B OK\n{mamplan_results[1]}',end='')

        old_dedupe=section(baseline,'bool DecodiumBridge::isRecentAutoCqDuplicate(', 'bool DecodiumBridge::maybeStartCourtesySignoff(')
        new_dedupe=section((ROOT/'src/bridge/DecodiumBridgeAutoCqDedupe.cpp').read_text(),'bool DecodiumBridge::isRecentAutoCqDuplicate(',None)
        if old_dedupe!=new_dedupe.replace('decodiumBridge_isWithinRecent', 'isWithinRecent'):raise SystemExit('AutoCQ dedupe differs from baseline')
        dedupe_results=[]
        for label,body in (('A',old_dedupe),('B',new_dedupe)):
            (d/'bridge_refactor_autocq_dedupe.inc').write_text(body)
            exe=d/f'autocq_dedupe_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_autocq_dedupe_harness.cpp'),*flags,'-o',str(exe))
            dedupe_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if dedupe_results[0]!=dedupe_results[1]:raise SystemExit('AutoCQ dedupe A/B output differs')
        print(f'AutoCQ dedupe: A/B OK\n{dedupe_results[1]}',end='')

        old_sstv = run('git','show','v1.0.737:src/bridge/DecodiumBridgeSstv.cpp',stdout=subprocess.PIPE).stdout
        old_controls=section(old_sstv,'QVariantMap DecodiumBridge::sstvRxControls() const','bool DecodiumBridge::sstvRxAudioJobBusy() const noexcept')
        new_controls=section((ROOT/'src/bridge/DecodiumBridgeSstvControls.cpp').read_text(),'QVariantMap DecodiumBridge::sstvRxControls() const','bool DecodiumBridge::configureSstvInterferenceNotch(')
        for old_name,new_name in (('sstvRxModeControlName(', 'decodiumSstvRxModeControlName('),('sstvRxAfcModeName(', 'decodiumSstvRxAfcModeName('),('sstvRxSlantModeName(', 'decodiumSstvRxSlantModeName('),('boundedUnsigned(', 'decodiumSstvBoundedUnsigned(')):
            new_controls=new_controls.replace(new_name,old_name)
        if old_controls!=new_controls:raise SystemExit('SSTV controls differ from baseline')
        print('SSTV controls: A/B source equivalence OK')
        old_config=section(old_sstv,'bool DecodiumBridge::configureSstvInterferenceNotch(', 'bool DecodiumBridge::updateSstvRxControls(')
        new_config=section((ROOT/'src/bridge/DecodiumBridgeSstvControls.cpp').read_text(),'bool DecodiumBridge::configureSstvInterferenceNotch(',None)
        if old_config!=new_config:raise SystemExit('SSTV configuration differs from baseline')
        print('SSTV configuration: A/B source equivalence OK')

        old_call3_paths=section(baseline,'static QStringList worldMapCall3CandidatePaths()', 'static WorldMapCall3LoadResult loadWorldMapCall3CacheFromFiles(')
        local_data=(ROOT/'src/bridge/DecodiumBridgeLocalDataState.cpp').read_text()
        new_call3_paths=section(local_data,'QStringList worldMapCall3CandidatePaths()', 'QStringList DecodiumBridge::ctyDatSearchPaths() const')
        if old_call3_paths.replace('static QStringList worldMapCall3CandidatePaths()', 'QStringList worldMapCall3CandidatePaths()')!=new_call3_paths:raise SystemExit('CALL3 paths differ from baseline')
        old_local=section(baseline,'QStringList DecodiumBridge::ctyDatSearchPaths() const', 'void DecodiumBridge::reloadDxccLookupAsync(')
        new_local=section(local_data,'QStringList DecodiumBridge::ctyDatSearchPaths() const', None)
        if old_local!=new_local:raise SystemExit('local data state differs from baseline')
        print('Local data state and paths: A/B source equivalence OK')

        old_network=section(baseline,'int DecodiumBridge::remoteWebSocketPort() const', 'void DecodiumBridge::saveSettings()')
        new_network=section((ROOT/'src/bridge/DecodiumBridgeNetworkPreferences.cpp').read_text(),'int DecodiumBridge::remoteWebSocketPort() const', None)
        if old_network!=new_network:raise SystemExit('network preferences differ from baseline')
        netflags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Network'],text=True))
        network_results=[]
        for label,body in (('A',old_network),('B',new_network)):
            (d/'bridge_refactor_network_prefs.inc').write_text(body)
            exe=d/f'network_prefs_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_network_prefs_harness.cpp'),*netflags,'-o',str(exe))
            network_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if network_results[0]!=network_results[1]:raise SystemExit('network preferences A/B output differs')
        print(f'network preferences: A/B OK\n{network_results[1]}',end='')

        old_mapconsumers=section(baseline,'bool DecodiumBridge::worldMapFeedEnabled() const', 'QString DecodiumBridge::worldMapFeedEntryKey(')
        new_mapconsumers=section((ROOT/'src/bridge/DecodiumBridgeWorldMapConsumers.cpp').read_text(),'bool DecodiumBridge::worldMapFeedEnabled() const', None)
        if old_mapconsumers!=new_mapconsumers.replace('decodiumBridgeLogbookLog(', 'bridgeLog('):raise SystemExit('world map consumers differ from baseline')
        map_results=[]
        for label,body in (('A',old_mapconsumers),('B',new_mapconsumers)):
            (d/'bridge_refactor_map_consumers.inc').write_text(body)
            exe=d/f'map_consumers_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_map_consumers_harness.cpp'),*flags,'-o',str(exe))
            map_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if map_results[0]!=map_results[1]:raise SystemExit('world map consumers A/B differ')
        print(f'world map consumers: A/B OK\n{map_results[1]}',end='')
        old_rows=section(baseline,'QStringList DecodiumBridge::parseFt8Row(', 'bool DecodiumBridge::usingTciAudioInput()')
        new_rows=section((ROOT/'src/bridge/DecodiumBridgeDecodeRowParsers.cpp').read_text(),'QStringList DecodiumBridge::parseFt8Row(', None).replace('decodiumBridgeNormalizeRemoteActivityTime(', 'normalizeUtcDisplayToken(')
        if old_rows!=new_rows:raise SystemExit('decode row parsers differ from baseline')
        helper=section(baseline,'static QString normalizeUtcDisplayToken(', 'static int secondsFromUtcDisplayToken(')
        current_bridge=(ROOT/'src/bridge/DecodeUtcTime.cpp').read_text()
        if helper != section(current_bridge,'static QString normalizeUtcDisplayToken(', 'QString decodiumBridgeNormalizeRemoteActivityTime('):raise SystemExit('decode UTC helper differs from baseline')
        if 'return normalizeUtcDisplayToken(std::move(raw));' not in section(current_bridge,'QString decodiumBridgeNormalizeRemoteActivityTime(', 'static int secondsFromUtcDisplayToken('):raise SystemExit('decode UTC wrapper differs')
        (d/'bridge_refactor_decode_rows_helper.inc').write_text(helper)
        row_results=[]
        for label,body in (('A',old_rows),('B',new_rows)):
            (d/'bridge_refactor_decode_rows.inc').write_text(body)
            exe=d/f'decode_rows_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),'-I'+str(ROOT/'src/bridge'),str(ROOT/'tests/bridge_refactor_decode_rows_harness.cpp'),str(ROOT/'src/bridge/DecodeMessageNormalization.cpp'),*flags,'-o',str(exe))
            row_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if row_results[0]!=row_results[1]:raise SystemExit('decode row parsers A/B output differs')
        print(f'decode row parsers: A/B OK\n{row_results[1]}',end='')
        old_qso=section(baseline,'int         DecodiumBridge::qsoCount()', 'void DecodiumBridge::mirrorLegacyLoggedAdif(')
        new_qso=section((ROOT/'src/bridge/DecodiumBridgeQsoCache.cpp').read_text(),'int         DecodiumBridge::qsoCount()', None).replace('decodiumBridgeCountAdifRecords(', 'countAdifRecordsLightweight(').replace('decodiumBridgeLogbookLog(', 'bridgeLog(')
        if old_qso!=new_qso:raise SystemExit('QSO cache differs from baseline')
        for begin,end in (('static QVariantMap qsoStatsFromRows(', 'static QString exportRecordFingerprint('),('static QsoLogSnapshot buildQsoLogSnapshot(', 'static void setAdifModeFields(')):
            old_helper=section(baseline,begin,end).replace('static ', '', 1)
            new_helper=section((ROOT/'src/bridge/DecodiumQsoSnapshot.cpp').read_text(),begin.replace('static ', '', 1), None if end=='static void setAdifModeFields(' else end.replace('static ', '', 1))
            if old_helper!=new_helper:raise SystemExit(f'QSO cache helper differs from baseline: {begin}')
        qso_results=[]
        for label,body in (('A',old_qso),('B',new_qso)):
            (d/'bridge_refactor_qso_cache.inc').write_text(body)
            exe=d/f'qso_cache_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),'-I'+str(ROOT/'src/bridge'),str(ROOT/'tests/bridge_refactor_qso_cache_harness.cpp'),*flags,'-o',str(exe))
            qso_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if qso_results[0]!=qso_results[1]:raise SystemExit('QSO cache A/B output differs')
        print(f'QSO cache: A/B OK\n{qso_results[1]}',end='')
        baseline_sstv=run('git','show',f'{args.base}:src/bridge/DecodiumBridgeSstv.cpp',cwd=ROOT,stdout=subprocess.PIPE).stdout
        old_sstv_state=section(baseline_sstv,'bool DecodiumBridge::sstvTxCanStart()', 'int DecodiumBridge::sstvPttLeadMs()')
        new_sstv_state=section((ROOT/'src/bridge/DecodiumBridgeSstvTxState.cpp').read_text(),'bool DecodiumBridge::sstvTxCanStart()',None)
        if old_sstv_state!=new_sstv_state:raise SystemExit('SSTV TX state differs from baseline')
        for enabled in (0,1):
            state_results=[]
            for label,body in (('A',old_sstv_state),('B',new_sstv_state)):
                (d/'bridge_refactor_sstv_tx_state.inc').write_text(body)
                exe=d/f'sstv_tx_state_{enabled}_{label}{".exe" if os.name == "nt" else ""}'
                run(*compiler,'-std=c++17',f'-DDECODIUM_HAS_SSTV={enabled}','-I'+str(d),str(ROOT/'tests/bridge_refactor_sstv_tx_state_harness.cpp'),*flags,'-o',str(exe))
                state_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
            if state_results[0]!=state_results[1]:raise SystemExit(f'SSTV TX state A/B differs, SSTV={enabled}')
            print(f'SSTV TX state enabled={enabled}: A/B OK\n{state_results[1]}',end='')
        old_target=section(baseline,'void DecodiumBridge::setTargetCallSign(', 'void DecodiumBridge::startTargetCall()')
        target_source=(ROOT/'src/bridge/DecodiumBridgeTargetCallSettings.cpp').read_text()
        new_target=section(target_source,'void DecodiumBridge::setTargetCallSign(', 'void DecodiumBridge::startTargetCall()').replace('decodiumBridgeLogbookLog(', 'bridgeLog(').rstrip()+'\n\n'
        if old_target!=new_target:raise SystemExit('target call settings differ from baseline')
        target_results=[]
        for label,body in (('A',old_target),('B',new_target)):
            (d/'bridge_refactor_targetcall.inc').write_text(body)
            exe=d/f'targetcall_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_targetcall_harness.cpp'),*flags,'-o',str(exe))
            target_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if target_results[0]!=target_results[1]:raise SystemExit('target call settings A/B differ')
        print(f'target call settings: A/B OK\n{target_results[1]}',end='')
        old_lifecycle=section(baseline,'void DecodiumBridge::startTargetCall()', 'bool DecodiumBridge::targetCallRowsContainTarget(')
        new_lifecycle=section(target_source,'void DecodiumBridge::startTargetCall()','bool DecodiumBridge::targetCallRowsContainTarget(').replace('decodiumBridgeLogbookLog(', 'bridgeLog(')
        if old_lifecycle!=new_lifecycle:raise SystemExit('target call lifecycle differs from baseline')
        lifecycle_results=[]
        for label,body in (('A',old_lifecycle),('B',new_lifecycle)):
            (d/'bridge_refactor_targetcall_lifecycle.inc').write_text(body)
            exe=d/f'targetcall_lifecycle_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_targetcall_lifecycle_harness.cpp'),*flags,'-o',str(exe))
            lifecycle_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if lifecycle_results[0]!=lifecycle_results[1]:raise SystemExit('target call lifecycle A/B differs')
        print(f'target call lifecycle: A/B OK\n{lifecycle_results[1]}',end='')
        old_target_decode=section(baseline,'bool DecodiumBridge::targetCallRowsContainTarget(', '// === CAT ===')
        new_target_decode=section(target_source,'bool DecodiumBridge::targetCallRowsContainTarget(',None).replace('decodiumBridgeLogbookLog(', 'bridgeLog(')
        if old_target_decode!=new_target_decode:raise SystemExit('target call decode logic differs from baseline')
        target_decode_results=[]
        for label,body in (('A',old_target_decode),('B',new_target_decode)):
            (d/'bridge_refactor_targetcall_decode.inc').write_text(body)
            exe=d/f'targetcall_decode_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_targetcall_decode_harness.cpp'),*flags,'-o',str(exe))
            target_decode_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if target_decode_results[0]!=target_decode_results[1]:raise SystemExit('target call decode A/B differs')
        print(f'target call decode: A/B OK\n{target_decode_results[1]}',end='')
        old_autocall=section(baseline,'bool DecodiumBridge::autoCallModeSupported()', 'void DecodiumBridge::setTxPeriod(')
        new_autocall=section((ROOT/'src/bridge/DecodiumBridgeAutoCall.cpp').read_text(),'bool DecodiumBridge::autoCallModeSupported()', 'void DecodiumBridge::noteAutoCallQsoCompleted()').replace('decodiumBridgeLogbookLog(', 'bridgeLog(').rstrip()+'\n\n'
        if old_autocall!=new_autocall:raise SystemExit('Auto Call methods differ from baseline')
        autocall_results=[]
        for label,body in (('A',old_autocall),('B',new_autocall)):
            (d/'bridge_refactor_autocall.inc').write_text(body)
            exe=d/f'autocall_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_autocall_harness.cpp'),*flags,'-o',str(exe))
            autocall_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if autocall_results[0]!=autocall_results[1]:raise SystemExit('Auto Call A/B differs')
        print(f'Auto Call: A/B OK\n{autocall_results[1]}',end='')
        old_calibration=section(baseline,'double DecodiumBridge::frequencyCalibrationSlopePpm()', 'QStringList DecodiumBridge::satelliteOptions()')
        new_calibration=section((ROOT/'src/bridge/DecodiumBridgeFrequencyCalibration.cpp').read_text(),'double DecodiumBridge::frequencyCalibrationSlopePpm()',None).replace('decodiumBridgeLogbookLog(', 'bridgeLog(')
        if old_calibration!=new_calibration:raise SystemExit('frequency calibration differs from baseline')
        calibration_results=[]
        for label,body in (('A',old_calibration),('B',new_calibration)):
            (d/'bridge_refactor_frequency_calibration.inc').write_text(body)
            exe=d/f'frequency_calibration_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_frequency_calibration_harness.cpp'),*flags,'-o',str(exe))
            calibration_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if calibration_results[0]!=calibration_results[1]:raise SystemExit('frequency calibration A/B differs')
        print(f'frequency calibration: A/B OK\n{calibration_results[1]}',end='')
        old_protection=section(baseline,'bool DecodiumBridge::checkSwrAllowsTransmission(', '// === SETTINGS ===')
        new_protection=section((ROOT/'src/bridge/DecodiumBridgeTxProtection.cpp').read_text(),'bool DecodiumBridge::checkSwrAllowsTransmission(',None).replace('decodiumBridgeLogbookLog(', 'bridgeLog(').replace('decodiumBridgeWsprPowerDbmToWatts(', 'wsprPowerDbmToWatts(').replace('decodiumBridgeNormalizeWsprPowerDbm(', 'normalizeWsprPowerDbm(').replace('decodiumBridgeWsprPowerDbmLabel(', 'wsprPowerDbmLabel(')
        if old_protection!=new_protection:raise SystemExit('TX protection differs from baseline')
        helpers=section(baseline,'static int normalizeWsprPowerDbm(', 'static QString txWaveMetricsSummary(')
        current_helpers=section((ROOT/'src/bridge/DecodiumBridgeTxWaveHelpers.cpp').read_text(),'static int normalizeWsprPowerDbm(', 'int decodiumBridgeNormalizeWsprPowerDbm(')
        if helpers!=current_helpers:raise SystemExit('WSPR power helpers differ from baseline')
        (d/'bridge_refactor_txprotection_helpers.inc').write_text(helpers)
        protection_results=[]
        for label,body in (('A',old_protection),('B',new_protection)):
            (d/'bridge_refactor_txprotection.inc').write_text(body)
            exe=d/f'txprotection_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_txprotection_harness.cpp'),*flags,'-o',str(exe))
            protection_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if protection_results[0]!=protection_results[1]:raise SystemExit('TX protection A/B differs')
        print(f'TX protection: A/B OK\n{protection_results[1]}',end='')
        old_legacy_settings=section(baseline,'QString DecodiumBridge::legacyIniPath()', '// ---------------------------------------------------------------------------\n// Standalone UDP MessageClient')
        new_legacy_settings=section((ROOT/'src/bridge/DecodiumBridgeLegacySettings.cpp').read_text(),'QString DecodiumBridge::legacyIniPath()',None).replace('decodiumBridgeLogbookLog(', 'bridgeLog(').replace('decodiumBridgeAliasedSettingKey(', 'aliasedBridgeSettingKey(')
        if old_legacy_settings!=new_legacy_settings:raise SystemExit('legacy settings differ from baseline')
        legacy_results=[]
        for label,body in (('A',old_legacy_settings),('B',new_legacy_settings)):
            (d/'bridge_refactor_legacysettings.inc').write_text(body)
            exe=d/f'legacysettings_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_legacysettings_harness.cpp'),*flags,'-o',str(exe))
            legacy_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if legacy_results[0]!=legacy_results[1]:raise SystemExit('legacy settings A/B differs')
        print(f'legacy settings: A/B OK\n{legacy_results[1]}',end='')
        old_stations=section(baseline,'QVariantList DecodiumBridge::stationFrequencyRows()', 'double DecodiumBridge::frequencyCalibrationSlopePpm()')
        new_stations=section((ROOT/'src/bridge/DecodiumBridgeStationFrequencies.cpp').read_text(),'QVariantList DecodiumBridge::stationFrequencyRows()',None).replace('decodiumBridgeLogbookLog(', 'bridgeLog(').replace('decodiumBridgeParseFrequencyOffsetHzValue(', 'parseFrequencyOffsetHzValue(')
        if old_stations!=new_stations:raise SystemExit('station frequency methods differ from baseline')
        station_parser=section(baseline,'Radio::FrequencyDelta parseFrequencyOffsetHzValue(', 'void enforcePreferredWorkingFrequency(')
        if station_parser!=section((ROOT/'src/bridge/DecodiumBridgeWorkingFrequencies.cpp').read_text(),'Radio::FrequencyDelta parseFrequencyOffsetHzValue(', 'void enforcePreferredWorkingFrequency('):raise SystemExit('station offset parser differs from baseline')
        (d/'bridge_refactor_stationfreq_parser.inc').write_text(station_parser)
        station_results=[]
        for label,body in (('A',old_stations),('B',new_stations)):
            (d/'bridge_refactor_stationfreq.inc').write_text(body)
            exe=d/f'stationfreq_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_stationfreq_harness.cpp'),*flags,'-o',str(exe))
            station_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if station_results[0]!=station_results[1]:raise SystemExit('station frequency A/B differs')
        print(f'station frequencies: A/B OK\n{station_results[1]}',end='')
        old_satellite=section(baseline,'QStringList DecodiumBridge::satelliteOptions()', 'QFont DecodiumBridge::fontSettingFont(')
        new_satellite=section((ROOT/'src/bridge/DecodiumBridgeSatelliteOptions.cpp').read_text(),'QStringList DecodiumBridge::satelliteOptions()',None)
        if old_satellite!=new_satellite:raise SystemExit('satellite options differ from baseline')
        satellite_results=[]
        for label,body in (('A',old_satellite),('B',new_satellite)):
            (d/'bridge_refactor_satellite_options.inc').write_text(body)
            exe=d/f'satellite_options_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_satellite_options_harness.cpp'),*flags,'-o',str(exe))
            satellite_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if satellite_results[0]!=satellite_results[1]:raise SystemExit('satellite options A/B differs')
        print(f'satellite options: A/B OK\n{satellite_results[1]}',end='')
        old_tci=section(baseline,'bool DecodiumBridge::usingTciAudioInput()', 'void DecodiumBridge::onRtlSdrPcmSamplesReady(')
        new_tci=section((ROOT/'src/bridge/DecodiumBridgeTciAudio.cpp').read_text(),'bool DecodiumBridge::usingTciAudioInput()',None).replace('decodiumBridgeLogbookLog(', 'bridgeLog(').replace('decodiumBridgeStorePrecomputedTxWave(', 'storeBridgePrecomputedTxWave(').replace('decodiumBridgeIsFt2LinkApplicationMode(', 'isFt2LinkApplicationMode(')
        if old_tci!=new_tci:raise SystemExit('TCI audio differs from baseline')
        tci_results=[]
        for label,body in (('A',old_tci),('B',new_tci)):
            (d/'bridge_refactor_tciaudio.inc').write_text(body)
            exe=d/f'tciaudio_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_tciaudio_harness.cpp'),*flags,'-o',str(exe))
            tci_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if tci_results[0]!=tci_results[1]:raise SystemExit('TCI audio A/B differs')
        print(f'TCI audio: A/B OK\n{tci_results[1]}',end='')
        old_rtl=section(baseline,'void DecodiumBridge::onRtlSdrPcmSamplesReady(', 'void DecodiumBridge::startRtlSdrAudioPlayback(')
        rtl_source=(ROOT/'src/bridge/DecodiumBridgeRtlSdrSamples.cpp').read_text()
        new_rtl=section(rtl_source,'void DecodiumBridge::onRtlSdrPcmSamplesReady(', 'void DecodiumBridge::processRtlSdrSpectrum()').replace('decodiumBridgeLogbookLog(', 'bridgeLog(').replace('decodiumBridgeIsFt2LinkApplicationMode(', 'isFt2LinkApplicationMode(').rstrip()+'\n\n'
        if old_rtl!=new_rtl:raise SystemExit('RTL-SDR samples differ from baseline')
        rtl_results=[]
        for label,body in (('A',old_rtl),('B',new_rtl)):
            (d/'bridge_refactor_rtlsdr_samples.inc').write_text(body)
            exe=d/f'rtlsdr_samples_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_rtlsdr_samples_harness.cpp'),*flags,'-o',str(exe))
            rtl_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if rtl_results[0]!=rtl_results[1]:raise SystemExit('RTL-SDR samples A/B differs')
        print(f'RTL-SDR samples: A/B OK\n{rtl_results[1]}',end='')
        old_spectrum=section(baseline,'void DecodiumBridge::processRtlSdrSpectrum()', 'void DecodiumBridge::ensureAudioSink()')
        new_spectrum=section(rtl_source,'void DecodiumBridge::processRtlSdrSpectrum()',None)
        if old_spectrum!=new_spectrum:raise SystemExit('RTL-SDR spectrum differs from baseline')
        spectrum_results=[]
        for label,body in (('A',old_spectrum),('B',new_spectrum)):
            (d/'bridge_refactor_rtlsdr_spectrum.inc').write_text(body)
            exe=d/f'rtlsdr_spectrum_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),'-I'+str(ROOT),str(ROOT/'tests/bridge_refactor_rtlsdr_spectrum_harness.cpp'),str(ROOT/'src/rtl/RtlSdrRfSpectrum.cpp'),*flags,'-o',str(exe))
            spectrum_results.append(run(str(exe),stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout)
        if spectrum_results[0]!=spectrum_results[1]:raise SystemExit('RTL-SDR spectrum A/B differs')
        print(f'RTL-SDR spectrum: A/B OK\n{spectrum_results[1]}',end='')
        old_rtl_playback=section(baseline,'void DecodiumBridge::startRtlSdrAudioPlayback(', 'void DecodiumBridge::processRtlSdrSpectrum()')
        new_rtl_playback=section((ROOT/'src/bridge/DecodiumBridgeRtlSdrPlayback.cpp').read_text(),'void DecodiumBridge::startRtlSdrAudioPlayback(',None).replace('decodiumBridgeLogbookLog(', 'bridgeLog(').replace('decodiumBridgeResolveAudioDeviceByIdentity(', 'tryResolveAudioDeviceByIdentity(')
        if old_rtl_playback!=new_rtl_playback:raise SystemExit('RTL-SDR playback differs from baseline')
        playback_results=[]
        for label,body in (('A',old_rtl_playback),('B',new_rtl_playback)):
            (d/'bridge_refactor_rtlsdr_playback.inc').write_text(body)
            exe=d/f'rtlsdr_playback_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_rtlsdr_playback_harness.cpp'),*flags,'-o',str(exe))
            playback_results.append(run(str(exe),stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout)
        if playback_results[0]!=playback_results[1]:raise SystemExit('RTL-SDR playback A/B differs')
        print(f'RTL-SDR playback: A/B OK\n{playback_results[1]}',end='')
        old_uslotw=section(baseline,'void DecodiumBridge::setShowUsState(', 'void DecodiumBridge::reloadLotwUsers(')
        lotw_source=(ROOT/'src/bridge/DecodiumBridgeUsStateLotw.cpp').read_text()
        new_uslotw=section(lotw_source,'void DecodiumBridge::setShowUsState(', 'void DecodiumBridge::reloadLotwUsers(').rstrip()+'\n\n'
        if old_uslotw!=new_uslotw:raise SystemExit('US state and LotW controls differ from baseline')
        uslotw_results=[]
        for label,body in (('A',old_uslotw),('B',new_uslotw)):
            (d/'bridge_refactor_usstate_lotw.inc').write_text(body)
            exe=d/f'usstate_lotw_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_usstate_lotw_harness.cpp'),*flags,'-o',str(exe))
            uslotw_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if uslotw_results[0]!=uslotw_results[1]:raise SystemExit('US state and LotW controls A/B differs')
        print(f'US state and LotW controls: A/B OK\n{uslotw_results[1]}',end='')
        old_lotw_helpers=section(baseline,'constexpr qint64 kLotwCacheMaxAgeSeconds', '\n}\n\nstatic constexpr int kDefaultTxWatchdogMinutes')
        new_lotw_helpers=section(lotw_source,'constexpr qint64 kLotwCacheMaxAgeSeconds', '\n}\n\nvoid DecodiumBridge::setShowUsState(')
        if old_lotw_helpers!=new_lotw_helpers:raise SystemExit('LotW cache helpers differ from baseline')
        old_lotw=section(baseline,'void DecodiumBridge::reloadLotwUsers(', 'QStringList DecodiumBridge::workedCallsigns()')
        new_lotw=section(lotw_source,'void DecodiumBridge::reloadLotwUsers(', '#define bridgeLog decodiumBridgeLogbookLog').removesuffix('\n').replace('decodiumBridgeLogbookLog(', 'bridgeLog(')
        if old_lotw!=new_lotw:raise SystemExit('LotW loading differs from baseline')
        (d/'bridge_refactor_lotw_helpers.inc').write_text(old_lotw_helpers)
        lotwflags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Network','Qt6Concurrent'],text=True))
        lotw_results=[]
        for label,body in (('A',old_lotw),('B',new_lotw)):
            (d/'bridge_refactor_lotw_load.inc').write_text(body)
            exe=d/f'lotw_load_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_lotw_load_harness.cpp'),*lotwflags,'-o',str(exe))
            lotw_results.append(run(str(exe),stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout)
        if lotw_results[0]!=lotw_results[1]:raise SystemExit('LotW loading A/B differs')
        print(f'LotW cache loading: A/B OK\n{lotw_results[1]}',end='')
        (d/'bridge_refactor_lotw_mock_network.h').write_bytes((ROOT/'tests/bridge_refactor_lotw_mock_network.h').read_bytes())
        moc_dir=Path(subprocess.check_output(['pkg-config','--variable=libexecdir','Qt6Core'],text=True).strip())
        moc=moc_dir/('moc.exe' if os.name=='nt' else 'moc')
        moc_output=d/'moc_lotw_network.cpp'
        run(str(moc),str(d/'bridge_refactor_lotw_mock_network.h'),'-o',str(moc_output))
        lotw_network_results=[]
        for label,body in (('A',old_lotw),('B',new_lotw)):
            (d/'bridge_refactor_lotw_load.inc').write_text(body)
            exe=d/f'lotw_network_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_lotw_network_harness.cpp'),str(moc_output),*lotwflags,'-o',str(exe))
            lotw_network_results.append(run(str(exe),stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout)
        if lotw_network_results[0]!=lotw_network_results[1]:raise SystemExit('LotW network callback A/B differs')
        print(f'LotW network callbacks: A/B OK\n{lotw_network_results[1]}',end='')
        depth_source=(ROOT/'src/bridge/DecodiumBridgeDecodeDepth.cpp').read_text()
        depth_sections=[('static int modernDepthToLegacyBits(', 'static QDateTime approxUtcDateTimeForDisplayToken(', 'int DecodiumBridge::nfa()'),('int DecodiumBridge::nfa()', 'int DecodiumBridge::ncontest()', 'int DecodiumBridge::effectiveFt4ThreadLimit()'),('int DecodiumBridge::effectiveFt4ThreadLimit()', 'int DecodiumBridge::effectiveSpectrumTimerIntervalMs()', 'int DecodiumBridge::effectiveDecodeDepth()'),('int DecodiumBridge::effectiveDecodeDepth()', 'QVariantList DecodiumBridge::augmentLegacyMirrorWithAllTxt(', None)]
        old_depth='';new_depth=''
        for begin,old_end,new_end in depth_sections:
            old_depth+=section(baseline,begin,old_end)
            new_depth+=section(depth_source,begin,new_end)
        if old_depth!=new_depth:raise SystemExit('decode depth differs from baseline')
        depth_results=[]
        for label,body in (('A',old_depth),('B',new_depth)):
            (d/'bridge_refactor_decode_depth.inc').write_text(body)
            exe=d/f'decode_depth_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_decode_depth_harness.cpp'),*flags,'-o',str(exe))
            depth_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if depth_results[0]!=depth_results[1]:raise SystemExit('decode depth A/B differs')
        print(f'decode depth: A/B OK\n{depth_results[1]}',end='')
        old_mam_controls=section(baseline,'void DecodiumBridge::setMamMaxStreams(', 'QVariantMap DecodiumBridge::digiPileupStatus()')+section(baseline,'QVariantList DecodiumBridge::mamActiveSlots()', 'void DecodiumBridge::mamEnqueueClickedStation(')
        new_mam_controls=section((ROOT/'src/bridge/DecodiumBridgeMamControls.cpp').read_text(),'void DecodiumBridge::setMamMaxStreams(',None).replace('decodiumBridgeLogbookLog(', 'bridgeLog(')
        if old_mam_controls!=new_mam_controls:raise SystemExit('MAM controls differ from baseline')
        mam_controls_results=[]
        for label,body in (('A',old_mam_controls),('B',new_mam_controls)):
            (d/'bridge_refactor_mam_controls.inc').write_text(body)
            exe=d/f'mam_controls_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),'-I'+str(ROOT/'src/bridge'),str(ROOT/'tests/bridge_refactor_mam_controls_harness.cpp'),*flags,'-o',str(exe))
            mam_controls_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if mam_controls_results[0]!=mam_controls_results[1]:raise SystemExit('MAM controls A/B differs')
        print(f'MAM controls: A/B OK\n{mam_controls_results[1]}',end='')
        old_external_url=section(baseline,'bool DecodiumBridge::openExternalUrl(', 'void DecodiumBridge::openWavForDecode(')
        new_external_url=section((ROOT/'src/bridge/DecodiumBridgeExternalUrl.cpp').read_text(),'bool DecodiumBridge::openExternalUrl(',None)
        if old_external_url!=new_external_url:raise SystemExit('external URL handling differs from baseline')
        external_results=[]
        for label,body in (('A',old_external_url),('B',new_external_url)):
            (d/'bridge_refactor_external_url.inc').write_text(body)
            exe=d/f'external_url_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_external_url_harness.cpp'),*flags,'-o',str(exe))
            external_results.append(run(str(exe),stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout)
        if external_results[0]!=external_results[1]:raise SystemExit('external URL A/B differs')
        print(f'external URL: A/B OK\n{external_results[1]}',end='')
        working_source=(ROOT/'src/bridge/DecodiumBridgeWorkingFrequencies.cpp').read_text()
        working_begin='namespace\n{\nusing WorkingFrequencyItem'
        working_methods='QVariantList DecodiumBridge::workingFrequencyRows()'
        old_working_helpers=section(baseline,working_begin,working_methods)
        new_working_helpers=section(working_source,working_begin,'Radio::FrequencyDelta decodiumBridgeParseFrequencyOffsetHzValue(')
        old_working_methods=section(baseline,working_methods,'QVariantList DecodiumBridge::stationFrequencyRows()')
        new_working_methods=section(working_source,working_methods,None).replace('decodiumBridgeLogbookLog(', 'bridgeLog(').replace('decodiumBridgeCanonicalApplicationDecodeMode(', 'canonicalApplicationDecodeMode(').replace('decodiumBridgeIsRadioOnlyModeLabel(', 'isRadioOnlyModeLabel(')
        if old_working_helpers!=new_working_helpers or old_working_methods!=new_working_methods:raise SystemExit('working frequency code differs from baseline')
        print('working frequencies: A/B source equivalence OK (real-model runtime: bridge_workingfreq_real_ab.py)')
        old_spectrum_audio=section(baseline,'// Simple radix-2 in-place FFT', '// ─── Alta risoluzione:')
        spectrum_source=(ROOT/'src/bridge/DecodiumBridgeSpectrum.cpp').read_text()
        new_spectrum_audio=section(spectrum_source,'// Simple radix-2 in-place FFT','// ─── Alta risoluzione:').rstrip()+'\n\n'
        if old_spectrum_audio!=new_spectrum_audio:raise SystemExit('audio spectrum differs from baseline')
        spectrum_audio_results=[]
        for label,body in (('A',old_spectrum_audio),('B',new_spectrum_audio)):
            (d/'bridge_refactor_spectrum.inc').write_text(body)
            exe=d/f'spectrum_audio_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_spectrum_harness.cpp'),*flags,'-o',str(exe))
            spectrum_audio_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if spectrum_audio_results[0]!=spectrum_audio_results[1]:raise SystemExit('audio spectrum A/B differs')
        print(f'audio spectrum: A/B OK\n{spectrum_audio_results[1]}',end='')
        old_panfft=section(baseline,'// ─── Alta risoluzione:', 'void DecodiumBridge::initTxDevices()')
        new_panfft=section(spectrum_source,'// ─── Alta risoluzione:',None)
        if old_panfft!=new_panfft:raise SystemExit('panadapter FFT differs from baseline')
        (d/'bridge_refactor_panadapter_fft_helper.inc').write_text(section(baseline,'// Simple radix-2 in-place FFT','QVector<float> DecodiumBridge::computeSpectrum()'))
        fftw_query=subprocess.run(['pkg-config','--cflags','--libs','fftw3f'],text=True,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
        fftwflags=shlex.split(fftw_query.stdout) if fftw_query.returncode==0 else []
        engines=('fallback','fftw') if fftwflags else ('fallback',)
        if not fftwflags:print('panadapter FFT fftw: SKIPPED (fftw3f development package unavailable)')
        for engine in engines:
            panfft_results=[]
            for label,body in (('A',old_panfft),('B',new_panfft)):
                (d/'bridge_refactor_panadapter_fft.inc').write_text(body)
                exe=d/f'panadapter_fft_{engine}_{label}{".exe" if os.name == "nt" else ""}'
                defines=['-DFFTW3_SINGLE_FOUND','-DFFTW_HAS_THREADS=0'] if engine=='fftw' else []
                run(*compiler,'-std=c++17','-I'+str(d),'-I'+str(ROOT),*defines,str(ROOT/'tests/bridge_refactor_panadapter_fft_harness.cpp'),*flags,*(fftwflags if engine=='fftw' else []),'-o',str(exe))
                panfft_results.append(run(str(exe),stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout)
            if panfft_results[0]!=panfft_results[1]:raise SystemExit(f'panadapter FFT A/B differs: {engine}')
            print(f'panadapter FFT {engine}: A/B OK\n{panfft_results[1]}',end='')
        cabrillo_helpers=section(baseline,'static QString expandedLocalFilePath(', 'static bool isProbablyCall3TxtPayload(')
        if cabrillo_helpers!=section((ROOT/'src/bridge/DecodiumBridgeFilePaths.cpp').read_text(),'static QString expandedLocalFilePath(', 'QString decodiumBridgeExpandedLocalFilePath('):raise SystemExit('Cabrillo path helpers differ from baseline')
        old_cabrillo=section(baseline,'bool DecodiumBridge::exportCabrillo(', '// ============================================================\n// C16')
        new_cabrillo=section((ROOT/'src/bridge/DecodiumBridgeCabrillo.cpp').read_text(),'bool DecodiumBridge::exportCabrillo(',None).replace('decodiumBridgeExpandedLocalFilePath(', 'expandedLocalFilePath(').replace('decodiumBridgeEnsureParentDirectoryForFile(', 'ensureParentDirectoryForFile(')
        if old_cabrillo!=new_cabrillo:raise SystemExit('Cabrillo export differs from baseline')
        (d/'bridge_refactor_cabrillo_helpers.inc').write_text(cabrillo_helpers)
        cabrillo_results=[]
        for label,body in (('A',old_cabrillo),('B',new_cabrillo)):
            (d/'bridge_refactor_cabrillo.inc').write_text(body)
            exe=d/f'cabrillo_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler,'-std=c++17','-I'+str(d),str(ROOT/'tests/bridge_refactor_cabrillo_harness.cpp'),*flags,'-o',str(exe))
            cabrillo_results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
        if cabrillo_results[0]!=cabrillo_results[1]:raise SystemExit('Cabrillo A/B differs')
        print(f'Cabrillo export: A/B OK\n{cabrillo_results[1]}',end='')
        old_ftx_settings = section(baseline, 'void DecodiumBridge::setFt2SignoffRetryCap(', 'void DecodiumBridge::setAutoCqMaxCycles(')
        new_ftx_settings = section((ROOT / 'src/bridge/DecodiumBridgeFtxSettings.cpp').read_text(), 'void DecodiumBridge::setFt2SignoffRetryCap(', 'void DecodiumBridge::setCallerRetriesAlwaysHard(').replace('decodiumBridgeFtxSettingsLog(', 'bridgeLog(')
        if old_ftx_settings != new_ftx_settings:
            raise SystemExit('FTX settings: unexpected implementation changes')
        old_ftx_flags = section(baseline, 'void DecodiumBridge::setCallerRetriesAlwaysHard(', 'void DecodiumBridge::setResumeQsoOnReply(')
        new_ftx_flags = section((ROOT / 'src/bridge/DecodiumBridgeFtxSettings.cpp').read_text(), 'void DecodiumBridge::setCallerRetriesAlwaysHard(').replace('decodiumBridgeFtxSettingsLog(', 'bridgeLog(')
        if old_ftx_flags != new_ftx_flags:
            raise SystemExit('FTX flags: unexpected implementation changes')
        old_ftx_settings += old_ftx_flags
        new_ftx_settings += new_ftx_flags
        harness = ROOT / 'tests/bridge_refactor_ftx_settings_harness.cpp'
        results = []
        for label, code in (('A', old_ftx_settings), ('B', new_ftx_settings)):
            (d / 'bridge_refactor_ftx_settings.inc').write_text(code)
            exe = d / f'ftx_settings_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]:
            raise SystemExit('FTX settings: A/B outputs differ')
        print(f'FTX settings: A/B OK\n{results[1]}', end='')
        old_watchdog = section(baseline, 'void DecodiumBridge::persistTxWatchdogSettings()',
                               'void DecodiumBridge::qmlDebugLog(')
        new_watchdog = section((ROOT / 'src/bridge/DecodiumBridgeTxWatchdog.cpp').read_text(),
                               'void DecodiumBridge::persistTxWatchdogSettings()')
        if old_watchdog != new_watchdog:
            raise SystemExit('TX watchdog: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_tx_watchdog_harness.cpp'
        results = []
        for label, code in (('A', old_watchdog), ('B', new_watchdog)):
            (d / 'bridge_refactor_tx_watchdog.inc').write_text(code)
            exe = d / f'watchdog_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]:
            raise SystemExit('TX watchdog: A/B outputs differ')
        print(f'TX watchdog: A/B OK\n{results[1]}', end='')
        old_sstv = run('git', 'show', f'{args.base}:src/bridge/DecodiumBridgeSstv.cpp',
                       cwd=ROOT, stdout=subprocess.PIPE).stdout
        old_timing = section(old_sstv, 'int DecodiumBridge::sstvPttLeadMs() const',
                             '#if DECODIUM_HAS_SSTV\nvoid DecodiumBridge::setSstvThumbnailProvider(')
        new_timing = section((ROOT / 'src/bridge/DecodiumBridgeSstvTiming.cpp').read_text(),
                             'int DecodiumBridge::sstvPttLeadMs() const')
        if old_timing != new_timing:
            raise SystemExit('SSTV timing: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_sstv_timing_harness.cpp'
        for enabled in (0, 1):
            results = []
            for label, code in (('A', old_timing), ('B', new_timing)):
                (d / 'bridge_refactor_sstv_timing.inc').write_text(code)
                exe = d / f'sstv_timing_{enabled}_{label}{".exe" if os.name == "nt" else ""}'
                run(*compiler, '-std=c++17', '-I' + str(d), f'-DDECODIUM_HAS_SSTV={enabled}',
                    str(harness), *flags, '-o', str(exe))
                results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
            if results[0] != results[1]:
                raise SystemExit(f'SSTV timing {enabled}: A/B outputs differ')
            print(f'SSTV timing enabled={enabled}: A/B OK\n{results[1]}', end='')
        old_modes = section(baseline, 'QString DecodiumBridge::configuredCatRigMode() const',
                            'QString DecodiumBridge::rttyCatContext() const')
        new_modes = section((ROOT / 'src/bridge/DecodiumBridgeCatModes.cpp').read_text(),
                            'QString DecodiumBridge::configuredCatRigMode() const')
        if old_modes != new_modes:
            raise SystemExit('CAT modes: unexpected implementation changes')
        harness = ROOT / 'tests/bridge_refactor_cat_modes_harness.cpp'
        results = []
        for label, code in (('A', old_modes), ('B', new_modes)):
            (d / 'bridge_refactor_cat_modes.inc').write_text(code)
            exe = d / f'cat_modes_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]:
            raise SystemExit('CAT modes: A/B outputs differ')
        print(f'CAT modes: A/B OK\n{results[1]}', end='')
        old_time = section(baseline, 'static QString normalizeUtcDisplayToken(',
                           'static int secondsFromUtcDisplayToken(')
        bridge_current = (ROOT / 'src/bridge/DecodeUtcTime.cpp').read_text()
        new_time = section(bridge_current, 'static QString normalizeUtcDisplayToken(',
                           'QString decodiumBridgeNormalizeRemoteActivityTime(')
        if old_time != new_time:
            raise SystemExit('remote time: unexpected implementation changes')
        old_remote = section(baseline, 'void DecodiumBridge::clearRemoteActivityCache(',
                             'QVariantMap DecodiumBridge::worldClockSnapshot(')
        new_remote = section((ROOT / 'src/bridge/DecodiumBridgeRemoteActivity.cpp').read_text(),
                             'void DecodiumBridge::clearRemoteActivityCache(')
        if old_remote != new_remote.replace('decodiumBridgeNormalizeRemoteActivityTime(', 'normalizeUtcDisplayToken('):
            raise SystemExit('remote activity: unexpected implementation changes')
        (d / 'bridge_refactor_remote_time.inc').write_text(old_time)
        harness = ROOT / 'tests/bridge_refactor_remote_activity_harness.cpp'
        results = []
        for label, code in (('A', old_remote), ('B', new_remote)):
            (d / 'bridge_refactor_remote_activity.inc').write_text(code)
            exe = d / f'remote_{label}{".exe" if os.name == "nt" else ""}'
            run(*compiler, '-std=c++17', '-I' + str(d), str(harness), *flags, '-o', str(exe))
            results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
        if results[0] != results[1]:
            raise SystemExit('remote activity: A/B outputs differ')
        print(f'remote activity: A/B OK\n{results[1]}', end='')
        # Compile the real extracted translation unit for B, not a copy of it.
        (d / 'bridge_refactor_under_test.inc').write_text(access_reference)
        harness = ROOT / 'tests/bridge_refactor_ft2_access_harness.cpp'
        for config in ('default', 'synthetic'):
            definitions = [] if config == 'default' else [
                '-DSYNTHETIC_CONFIG', '-DDECODIUM_FT2LINK_ACCESS_SALT_B64="c2FsdA=="',
                '-DDECODIUM_FT2LINK_ACCESS_HASH_B64="aGFzaA=="',
                '-DDECODIUM_FT2LINK_ACCESS_ITERATIONS=12345']
            results = []
            for label in ('A', 'B'):
                exe = d / f'access_{config}_{label}{".exe" if os.name == "nt" else ""}'
                ref = ['-DBRIDGE_REFERENCE'] if label == 'A' else [str(ROOT / 'src/bridge/Ft2LinkAccessCrypto.cpp')]
                run(*compiler, '-std=c++17', '-I' + str(d), '-I' + str(ROOT / 'src/bridge'),
                    *definitions, *ref, str(harness), *flags, '-o', str(exe))
                results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
            if results[0] != results[1]:
                raise SystemExit(f'FT2 access {config}: A/B outputs differ')
            print(f'FT2 access {config}: A/B OK\n{results[1]}', end='')
        old_gate = section(baseline, 'bool DecodiumBridge::ft2LinkAccessUnlocked() const',
                           'void DecodiumBridge::setMode(')
        bridge = (ROOT / 'src/bridge/DecodiumBridgeFt2Access.cpp').read_text()
        new_gate = section(bridge, 'bool DecodiumBridge::ft2LinkAccessUnlocked() const', None)
        if old_gate != new_gate.replace('decodium::ft2_access::', '').replace('decodiumBridgeLogbookLog(', 'bridgeLog('):
            raise SystemExit('FT2 gate: unexpected code changes')
        salt = b'0123456789abcdef'
        digest = hashlib.pbkdf2_hmac('sha256', b'synthetic-password', salt, 10000)
        b64salt = base64.b64encode(salt).decode()
        b64hash = base64.b64encode(digest).decode()
        gate_configs = {
            'missing': [],
            'valid': [f'-DDECODIUM_FT2LINK_ACCESS_SALT_B64="{b64salt}"',
                      f'-DDECODIUM_FT2LINK_ACCESS_HASH_B64="{b64hash}"',
                      '-DDECODIUM_FT2LINK_ACCESS_ITERATIONS=10000', '-DVALID_GATE'],
            'weak-iterations': [f'-DDECODIUM_FT2LINK_ACCESS_SALT_B64="{b64salt}"',
                                f'-DDECODIUM_FT2LINK_ACCESS_HASH_B64="{b64hash}"',
                                '-DDECODIUM_FT2LINK_ACCESS_ITERATIONS=9999'],
            'short-salt': ['-DDECODIUM_FT2LINK_ACCESS_SALT_B64="c2FsdA=="',
                           f'-DDECODIUM_FT2LINK_ACCESS_HASH_B64="{b64hash}"',
                           '-DDECODIUM_FT2LINK_ACCESS_ITERATIONS=10000'],
        }
        gate_harness = ROOT / 'tests/bridge_refactor_ft2_gate_harness.cpp'
        for config, definitions in gate_configs.items():
            results = []
            for label, gate in (('A', old_gate), ('B', new_gate)):
                (d / 'bridge_refactor_gate_under_test.inc').write_text(gate)
                exe = d / f'gate_{config}_{label}{".exe" if os.name == "nt" else ""}'
                ref = ['-DBRIDGE_REFERENCE'] if label == 'A' else [str(ROOT / 'src/bridge/Ft2LinkAccessCrypto.cpp')]
                run(*compiler, '-std=c++17', '-I' + str(d), '-I' + str(ROOT / 'src/bridge'),
                    *definitions, *ref, str(gate_harness), *flags, '-o', str(exe))
                results.append(run(str(exe), stdout=subprocess.PIPE).stdout)
            if results[0] != results[1]:
                raise SystemExit(f'FT2 gate {config}: A/B outputs differ')
            print(f'FT2 gate {config}: A/B OK\n{results[1]}', end='')


if __name__ == '__main__':
    main()
