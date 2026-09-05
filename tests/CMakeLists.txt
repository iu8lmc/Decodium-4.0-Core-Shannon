find_package (Qt6Test 5 REQUIRED)

#
# Compiler options
#
set (CMAKE_CXX_STANDARD 11)
add_compile_options ("$<$<COMPILE_LANGUAGE:Fortran>:-Wall;-Wno-conversion;-fno-second-underscore;-fno-f2c>")
add_compile_options ("$<$<AND:$<COMPILE_LANGUAGE:Fortran>,$<CONFIG:Debug>>:-fbounds-check>")
add_compile_options ("$<$<AND:$<COMPILE_LANGUAGE:Fortran>,$<NOT:$<CONFIG:Debug>>>:-funroll-all-loops>")
add_compile_options ("$<$<AND:$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>,$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>>>:-Wall;-Wextra>")
add_compile_options ("$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU>>:-Wno-pragmas>")
add_compile_options ("$<$<AND:$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>>,$<NOT:$<CONFIG:Debug>>>:-fdata-sections;-ffunction-sections>")
if (${OPENMP_FOUND} OR APPLE)
  add_compile_options ("$<$<AND:$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>,$<C_COMPILER_ID:GNU>>:${OpenMP_C_FLAGS}>")
endif ()

# Tell CMake to run moc when necessary
set (CMAKE_AUTOMOC ON)
include_directories (
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}
  ${DECODIUM_SOURCE_DIRS}
  )

if (TARGET decodium_sstv_core)
  add_subdirectory (sstv)
endif ()

add_executable (test_qt_helpers
  test_qt_helpers.cpp
  ../Detector/MSK144DecodeWorker.cpp
  ../Sequencer/MessageTokenRules.cpp)
target_include_directories (test_qt_helpers PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries (test_qt_helpers wsjt_qt wsjt_cxx wsjt_fort Qt6::Test)
add_test (NAME test_qt_helpers COMMAND $<TARGET_FILE:test_qt_helpers>)
set_tests_properties (test_qt_helpers PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable (test_qso_sequencer_rules
  test_qso_sequencer_rules.cpp
  ../Sequencer/QsoSequencerRules.cpp
  ../Sequencer/MessageTokenRules.cpp
  ../src/radio/Radio.cpp)
target_link_libraries (test_qso_sequencer_rules Qt6::Test Qt6::Core)
add_test (NAME test_qso_sequencer_rules COMMAND $<TARGET_FILE:test_qso_sequencer_rules>)

add_executable (test_radio_frequency_delta test_radio_frequency_delta.cpp)
target_link_libraries (test_radio_frequency_delta
  wsjt_qt wsjt_cxx wsjt_fort Qt6::Test Qt6::Core Qt6::Network)
add_test (NAME test_radio_frequency_delta COMMAND $<TARGET_FILE:test_radio_frequency_delta>)

add_executable (test_wsprnet_post
  test_wsprnet_post.cpp
  ../Network/wsprnet.cpp)
target_link_libraries (test_wsprnet_post Qt6::Test Qt6::Core Qt6::Network)
add_test (NAME test_wsprnet_post COMMAND $<TARGET_FILE:test_wsprnet_post>)

add_executable (ft4_stage_compare ft4_stage_compare.cpp)
target_link_libraries (ft4_stage_compare wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)

add_executable (ft8_stage_compare ft8_stage_compare.cpp)
target_link_libraries (ft8_stage_compare wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
if (MINGW)
  target_link_options (ft8_stage_compare PRIVATE -Wl,--stack,67108864)
endif ()

# Riproduce fuori dall'app la corruzione di heap che si vede quando i thread
# di decodifica muoiono: senza GUI e senza radio, cosi' Dr. Memory ci gira
# sopra in pochi secondi invece di non arrivare nemmeno a decodificare.
add_executable (thread_churn_downsample thread_churn_downsample.cpp)
target_link_libraries (thread_churn_downsample wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
if (MINGW)
  target_link_options (thread_churn_downsample PRIVATE -Wl,--stack,67108864)
endif ()

add_executable (ft2_stage_compare ft2_stage_compare.cpp)
target_link_libraries (ft2_stage_compare wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_dependencies (test_qt_helpers ft2_stage_compare)

# Collaudo isolato dell'elenco delle stazioni sentite e dei bit del mittente:
# non tocca il decoder, verifica solo che i bit dell'ipotesi coincidano con
# quelli che la catena gia' produce. Un a priori sbagliato non e' neutro,
# e' dannoso, quindi va dimostrato prima di collegarlo.
add_executable (ap_storico_test ap_storico_test.cpp)
target_link_libraries (ap_storico_test wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_dependencies (test_qt_helpers ap_storico_test)

add_executable (q65_stage_compare q65_stage_compare.cpp ../Detector/Q65DecodeWorker.cpp)
target_include_directories (q65_stage_compare PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries (q65_stage_compare wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)

add_executable (ft2_equalized_compare ft2_equalized_compare.cpp)
target_link_libraries (ft2_equalized_compare wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_dependencies (test_qt_helpers ft2_equalized_compare)

add_executable (ft2_make_test_wav ft2_make_test_wav.cpp)
target_link_libraries (ft2_make_test_wav wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_dependencies (test_qt_helpers ft2_make_test_wav)

# Raccolta dati per il riaddestramento del gate appreso di fastldpc
# (FASTLDPC-AI-SPEC-001 SS2b): messaggi FT2 noti attraverso la catena di
# decodifica vera, con verita' calcolata a mano (vedi ftx_ft2_rvec_c in
# FtxFt2Stage7.cpp) invece che sul canale AWGN sintetico del pacchetto di
# ricerca originale. Produce il dataset che train_gate.py si aspetta.
add_executable (ft2_gate_dump ft2_gate_dump.cpp)
target_link_libraries (ft2_gate_dump wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_dependencies (test_qt_helpers ft2_gate_dump)

# Stesso banco ma per FT8: pesi del gate separati (GATE_*_FT8 in
# gate_weights.hpp), canale e tipi di messaggio diversi da FT2.
add_executable (ft8_gate_dump ft8_gate_dump.cpp)
target_link_libraries (ft8_gate_dump wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
if (MINGW)
  target_link_options (ft8_gate_dump PRIVATE -Wl,--stack,67108864)
endif ()
add_dependencies (test_qt_helpers ft8_gate_dump)

add_executable (test_tx_pipeline test_tx_pipeline.cpp)
target_link_libraries (test_tx_pipeline wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_test (NAME test_tx_pipeline COMMAND $<TARGET_FILE:test_tx_pipeline>)

add_executable (test_ptt_transition_policy test_ptt_transition_policy.cpp)
target_link_libraries (test_ptt_transition_policy Qt6::Test Qt6::Core)
add_test (NAME test_ptt_transition_policy COMMAND $<TARGET_FILE:test_ptt_transition_policy>)

add_executable (test_audio_sink_tx_gate
  test_audio_sink_tx_gate.cpp
  ../Audio/AudioDevice.cpp
  ../src/bridge/DecodiumAudioSink.h)
target_link_libraries (test_audio_sink_tx_gate Qt6::Test Qt6::Core)
add_test (NAME test_audio_sink_tx_gate COMMAND $<TARGET_FILE:test_audio_sink_tx_gate>)

add_executable (test_amplifier_telemetry test_amplifier_telemetry.cpp
                ../src/radio/DecodiumAmplifier.cpp)
target_include_directories (test_amplifier_telemetry PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries (test_amplifier_telemetry Qt6::Test Qt6::Core Qt6::SerialPort)
add_test (NAME test_amplifier_telemetry COMMAND $<TARGET_FILE:test_amplifier_telemetry>)

add_executable (test_station_telemetry_codec test_station_telemetry_codec.cpp
                ../src/radio/StationTelemetryCodec.cpp)
target_include_directories (test_station_telemetry_codec PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries (test_station_telemetry_codec Qt6::Test Qt6::Core)
add_test (NAME test_station_telemetry_codec COMMAND $<TARGET_FILE:test_station_telemetry_codec>)

add_executable (test_qmx_telemetry test_qmx_telemetry.cpp)
target_link_libraries (test_qmx_telemetry Qt6::Test Qt6::Core)
add_test (NAME test_qmx_telemetry COMMAND $<TARGET_FILE:test_qmx_telemetry>)

add_executable (test_decodium_update_support test_decodium_update_support.cpp)
target_link_libraries (test_decodium_update_support Qt6::Test Qt6::Core)
add_test (NAME test_decodium_update_support COMMAND $<TARGET_FILE:test_decodium_update_support>)

add_executable (test_adif_export_sanitizer
  test_adif_export_sanitizer.cpp
  ../src/bridge/AdifExportSanitizer.cpp
  ../src/bridge/AdifExportSanitizer.h)
target_link_libraries (test_adif_export_sanitizer Qt6::Test Qt6::Core)
add_test (NAME test_adif_export_sanitizer COMMAND $<TARGET_FILE:test_adif_export_sanitizer>)

add_executable (test_jt4_roundtrip test_jt4_roundtrip.cpp)
target_link_libraries (test_jt4_roundtrip wsjt_qt wsjt_cxx Qt6::Core)
add_test (NAME test_jt4_roundtrip COMMAND $<TARGET_FILE:test_jt4_roundtrip>)

add_executable (test_dx_cluster_band test_dx_cluster_band.cpp ../src/services/DecodiumDxCluster.cpp)
target_link_libraries (test_dx_cluster_band Qt6::Test Qt6::Network Qt6::Core)
add_test (NAME test_dx_cluster_band COMMAND $<TARGET_FILE:test_dx_cluster_band>)

add_executable (test_map_layer_service
  test_map_layer_service.cpp
  ../src/services/MapIntelligenceService.cpp
  ../src/services/MapIntelligenceService.h
  ../src/services/MapBaseMapService.cpp
  ../src/services/MapBaseMapService.h
  ../src/services/MapPskFeedService.cpp
  ../src/services/MapPskFeedService.h
  ../src/services/MapOperationsService.cpp
  ../src/services/MapOperationsService.h
  ../src/services/RotatorService.cpp
  ../src/services/RotatorService.h
  ../src/services/MapExternalOverlayService.cpp
  ../src/services/MapExternalOverlayService.h
  ../src/services/DxccLookup.cpp
  ../src/services/DxccLookup.h
  ../src/services/MapLayerModel.cpp
  ../src/services/MapLayerModel.h)
target_link_libraries (test_map_layer_service Qt6::Test Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Concurrent)
qt_add_resources (test_map_layer_service test_map_layer_service_award_resources
  PREFIX "/"
  BASE "${CMAKE_SOURCE_DIR}/resources/map"
  FILES
    "${CMAKE_SOURCE_DIR}/resources/map/decodium-awards-catalog.json"
  )
add_test (NAME test_map_layer_service COMMAND $<TARGET_FILE:test_map_layer_service>)
# MapIntelligenceService resolves the bundled DXCC database next to the
# executable at runtime.  Make the test self-contained when CI builds only the
# regression-test aggregate (rather than decodium_qml, which normally copies
# this runtime file into the build tree).
add_custom_command (TARGET test_map_layer_service POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/resources/runtime/cty.dat"
    "$<TARGET_FILE_DIR:test_map_layer_service>/cty.dat"
  COMMENT "Copying cty.dat beside test_map_layer_service")

add_executable (test_sgp4
  test_sgp4.cpp
  ../src/services/satellite/sgp4/SGP4.cpp
  ../src/services/satellite/sgp4/SGP4.h)
target_link_libraries (test_sgp4 Qt6::Test Qt6::Core)
add_test (NAME test_sgp4 COMMAND $<TARGET_FILE:test_sgp4>)

add_executable (test_rotator_service
  test_rotator_service.cpp
  ../src/services/RotatorService.cpp
  ../src/services/RotatorService.h)
target_link_libraries (test_rotator_service Qt6::Test Qt6::Core Qt6::Gui Qt6::Network)
add_test (NAME test_rotator_service COMMAND $<TARGET_FILE:test_rotator_service>)

add_executable (test_cat4om_manager
  test_cat4om_manager.cpp
  ../src/radio/DecodiumCat4OmManager.cpp
  ../src/radio/DecodiumCat4OmManager.h)
target_link_libraries (test_cat4om_manager Qt6::Test Qt6::Core Qt6::Network Qt6::WebSockets)
add_test (NAME test_cat4om_manager COMMAND $<TARGET_FILE:test_cat4om_manager>)

add_executable (test_civ_address_settings
  test_civ_address_settings.cpp
  ../src/radio/DecodiumCatManager.cpp
  ../src/radio/DecodiumCatManager.h)
target_link_libraries (test_civ_address_settings Qt6::Test Qt6::Core Qt6::SerialPort)
add_test (NAME test_civ_address_settings COMMAND $<TARGET_FILE:test_civ_address_settings>)

add_executable (test_decoport_rig_driver_thread
  test_decoport_rig_driver_thread.cpp
  ../src/net/DecoPortRigDriver.cpp
  ../src/net/DecoPortRigDriver.h)
target_link_libraries (test_decoport_rig_driver_thread
  Qt6::Test Qt6::Core Hamlib::Hamlib)
add_test (NAME test_decoport_rig_driver_thread
  COMMAND $<TARGET_FILE:test_decoport_rig_driver_thread>)

add_executable (test_udp_client_id test_udp_client_id.cpp)
target_link_libraries (test_udp_client_id wsjt_qt Qt6::Test Qt6::Network Qt6::Core)
add_test (NAME test_udp_client_id COMMAND $<TARGET_FILE:test_udp_client_id>)

add_executable (test_ftx_weak_decode test_ftx_weak_decode.cpp)
target_link_libraries (test_ftx_weak_decode wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_test (NAME test_ftx_weak_decode COMMAND $<TARGET_FILE:test_ftx_weak_decode>)
set_tests_properties (test_ftx_weak_decode PROPERTIES ENVIRONMENT "DECODIUM_WEAK_TEST_SNR=-15")
add_test (NAME test_ft8_weak_decode_deep COMMAND $<TARGET_FILE:test_ftx_weak_decode>)
set_tests_properties (test_ft8_weak_decode_deep PROPERTIES
  ENVIRONMENT "DECODIUM_WEAK_TEST_MODE=FT8;DECODIUM_WEAK_TEST_SNR=-26;DECODIUM_WEAK_TEST_TRIALS=1")
add_test (NAME test_ft8_hinted_decode_very_deep COMMAND $<TARGET_FILE:test_ftx_weak_decode>)
set_tests_properties (test_ft8_hinted_decode_very_deep PROPERTIES
  ENVIRONMENT "DECODIUM_WEAK_TEST_MODE=FT8;DECODIUM_WEAK_TEST_SNR=-27;DECODIUM_WEAK_TEST_TRIALS=1;DECODIUM_WEAK_TEST_ACCEPT_MISS=1"
  LABELS "accepted-limit")

add_executable (test_ft2_qso_sim test_ft2_qso_sim.cpp)
target_link_libraries (test_ft2_qso_sim wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_test (NAME test_ft2_qso_sim COMMAND $<TARGET_FILE:test_ft2_qso_sim>)

if (DECODIUM_FASTLDPC_ARM64_TARGET)
  add_executable (test_fastldpc_neon test_fastldpc_neon.cpp)
  set_target_properties (test_fastldpc_neon PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
  add_test (NAME test_fastldpc_neon COMMAND $<TARGET_FILE:test_fastldpc_neon>)
endif ()

if (DECODIUM_FASTLDPC_ARM64_TARGET OR DECODIUM_FASTLDPC_X86_TARGET)
  add_executable (test_fastldpc_dispatch
    test_fastldpc_dispatch.cpp
    ../Detector/fastldpc/decodium_dispatch.cpp)
  set_target_properties (test_fastldpc_dispatch PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
  if (DECODIUM_FASTLDPC_ARM64_TARGET)
    target_compile_definitions (test_fastldpc_dispatch
      PRIVATE DECODIUM_FASTLDPC_NEON_BUILT=1)
  else ()
    target_compile_definitions (test_fastldpc_dispatch
      PRIVATE DECODIUM_FASTLDPC_AVX2_BUILT=1)
  endif ()
  add_test (NAME test_fastldpc_dispatch
    COMMAND $<TARGET_FILE:test_fastldpc_dispatch>)
endif ()

add_executable (test_secure_settings test_secure_settings.cpp)
target_link_libraries (test_secure_settings wsjt_qt wsjt_cxx Qt6::Test)
add_test (NAME test_secure_settings COMMAND $<TARGET_FILE:test_secure_settings>)

add_executable (test_callsign_intelligence
  test_callsign_intelligence.cpp
  ../src/services/CallsignIntelligenceService.cpp
  ../src/services/CallsignIntelligenceService.h
  ../src/services/DxccLookup.cpp
  ../src/services/DxccLookup.h
  ../src/security/SecureSettings.cpp)
target_link_libraries (test_callsign_intelligence Qt6::Test Qt6::Core Qt6::Network Qt6::Sql Qt6::Gui)
add_test (NAME test_callsign_intelligence COMMAND $<TARGET_FILE:test_callsign_intelligence>)

add_executable (test_dxcc_lookup
  test_dxcc_lookup.cpp
  ../src/services/DxccLookup.cpp
  ../src/services/DxccLookup.h)
target_link_libraries (test_dxcc_lookup Qt6::Test Qt6::Core)
add_test (NAME test_dxcc_lookup COMMAND $<TARGET_FILE:test_dxcc_lookup>)

add_executable (test_filedownload test_filedownload.cpp)
target_link_libraries (test_filedownload wsjt_qt wsjt_cxx Qt6::Test)
add_test (NAME test_filedownload COMMAND $<TARGET_FILE:test_filedownload>)

# 1.0.234 Sprint 3 / Phase 2 performance roadmap:
# Test header-only per StreamingListModel<T> in lib/ui/. Nessuna dipendenza
# da wsjt_qt / wsjt_cxx -- solo Qt6::Test + Qt6::Concurrent (QtConcurrent::run
# per il caso thread-safety). include_directories aggiunge ${CMAKE_SOURCE_DIR}
# alla riga 19, quindi #include "lib/ui/StreamingListModel.h" risolve.
find_package (Qt6 COMPONENTS Concurrent REQUIRED)
add_executable (test_streaming_list_model test_streaming_list_model.cpp)
target_link_libraries (test_streaming_list_model Qt6::Test Qt6::Concurrent)
add_test (NAME test_streaming_list_model COMMAND $<TARGET_FILE:test_streaming_list_model>)

add_executable (test_decode_list_model
  test_decode_list_model.cpp
  ../src/models/DecodeListModel.cpp)
target_link_libraries (test_decode_list_model Qt6::Test Qt6::Core)
add_test (NAME test_decode_list_model COMMAND $<TARGET_FILE:test_decode_list_model>)

add_executable (test_ft_decode_thread_budget test_ft_decode_thread_budget.cpp)
target_link_libraries (test_ft_decode_thread_budget Qt6::Test Qt6::Core)
add_test (NAME test_ft_decode_thread_budget COMMAND $<TARGET_FILE:test_ft_decode_thread_budget>)

add_executable (test_legacy_decode_window test_legacy_decode_window.cpp)
target_link_libraries (test_legacy_decode_window Qt6::Test Qt6::Core)
add_test (NAME test_legacy_decode_window COMMAND $<TARGET_FILE:test_legacy_decode_window>)

add_executable (test_b4_worked_before test_b4_worked_before.cpp)
target_compile_definitions (test_b4_worked_before PRIVATE DECODIUM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries (test_b4_worked_before Qt6::Test Qt6::Core)
add_test (NAME test_b4_worked_before COMMAND $<TARGET_FILE:test_b4_worked_before>)

add_executable (test_ft2link test_ft2link.cpp)
target_link_libraries (test_ft2link ft2link_core Qt6::Test)
add_test (NAME test_ft2link COMMAND $<TARGET_FILE:test_ft2link>)

add_executable (test_ft2link_satellite_half_duplex
  test_ft2link_satellite_half_duplex.cpp)
target_link_libraries (test_ft2link_satellite_half_duplex Qt6::Test Qt6::Core)
add_test (NAME test_ft2link_satellite_half_duplex
  COMMAND $<TARGET_FILE:test_ft2link_satellite_half_duplex>)

add_executable (test_linux_drm_gpu_usage test_linux_drm_gpu_usage.cpp)
target_link_libraries (test_linux_drm_gpu_usage Qt6::Test Qt6::Core)
add_test (NAME test_linux_drm_gpu_usage COMMAND $<TARGET_FILE:test_linux_drm_gpu_usage>)

add_executable (test_rtlsdr_dsp
  test_rtlsdr_dsp.cpp
  ../src/rtl/RtlSdrDsp.cpp
  ../src/rtl/RtlSdrDsp.h)
target_link_libraries (test_rtlsdr_dsp Qt6::Test Qt6::Core)
add_test (NAME test_rtlsdr_dsp COMMAND $<TARGET_FILE:test_rtlsdr_dsp>)

add_executable (test_rtlsdr_rf_spectrum
  test_rtlsdr_rf_spectrum.cpp
  ../src/rtl/RtlSdrRfSpectrum.cpp
  ../src/rtl/RtlSdrRfSpectrum.h)
target_link_libraries (test_rtlsdr_rf_spectrum Qt6::Test Qt6::Core)
add_test (NAME test_rtlsdr_rf_spectrum COMMAND $<TARGET_FILE:test_rtlsdr_rf_spectrum>)

add_executable (test_rtlsdr_capabilities
  test_rtlsdr_capabilities.cpp
  ../src/rtl/RtlSdrCapabilities.h)
target_link_libraries (test_rtlsdr_capabilities Qt6::Test Qt6::Core)
add_test (NAME test_rtlsdr_capabilities COMMAND $<TARGET_FILE:test_rtlsdr_capabilities>)

add_executable (test_rtlsdr_tuning_plan
  test_rtlsdr_tuning_plan.cpp
  ../src/rtl/RtlSdrTuningPlan.h)
target_link_libraries (test_rtlsdr_tuning_plan Qt6::Test Qt6::Core)
add_test (NAME test_rtlsdr_tuning_plan COMMAND $<TARGET_FILE:test_rtlsdr_tuning_plan>)

# Intentionally not registered with CTest: it opens a real USB radio.  Run it
# explicitly with DECODIUM_RTLSDR_HARDWARE_TEST=1 on a host with an RTL-SDR.
if (DECODIUM_HAS_RTLSDR)
  add_executable (test_rtlsdr_input
    test_rtlsdr_input.cpp
    ../src/rtl/RtlSdrInput.cpp
    ../src/rtl/RtlSdrInput.h
    ../src/rtl/RtlSdrDsp.cpp
    ../src/rtl/RtlSdrDsp.h)
  target_compile_definitions (test_rtlsdr_input PRIVATE DECODIUM_HAS_RTLSDR=1)
  target_link_libraries (test_rtlsdr_input Qt6::Test Qt6::Core RtlSdr::RtlSdr)
endif ()

add_executable (test_ft2link_qml_adapter
  test_ft2link_qml_adapter.cpp
  ../controllers/FT2LinkQmlAdapter.cpp
  ../controllers/FT2LinkQmlAdapter.hpp)
target_link_libraries (test_ft2link_qml_adapter ft2link_core Qt6::Test Qt6::Core)
target_compile_definitions (test_ft2link_qml_adapter
  PRIVATE
    DECODIUM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    DECODIUM_FT2LINK_QML_ADAPTER_STANDALONE=1)
add_test (NAME test_ft2link_qml_adapter COMMAND $<TARGET_FILE:test_ft2link_qml_adapter>)

add_executable (ft2link_rflab_corpus
  ft2link_rflab_corpus.cpp
  ../controllers/FT2LinkQmlAdapter.cpp
  ../controllers/FT2LinkQmlAdapter.hpp)
target_link_libraries (ft2link_rflab_corpus ft2link_core Qt6::Core)
target_compile_definitions (ft2link_rflab_corpus
  PRIVATE
    DECODIUM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    DECODIUM_FT2LINK_QML_ADAPTER_STANDALONE=1)

if (WSJT_BUILD_UTILS)
  # CLI smoke tests for utility binaries that are built in the main project.
  foreach(_utility_usage_target IN ITEMS
      test_q65 test_snr testEchoCall encode77 hash22calc msk144code msk144sim
      fst4sim ldpcsim240_101 ldpcsim240_74 sfoxsim sfrx sftx)
    if(TARGET ${_utility_usage_target})
      set(_usage_regex "Usage:")
      if(_utility_usage_target STREQUAL "hash22calc")
        set(_usage_regex "Usage:|Given a valid callsign")
      endif()
      add_test (NAME ${_utility_usage_target}_usage COMMAND $<TARGET_FILE:${_utility_usage_target}>)
      set_tests_properties (${_utility_usage_target}_usage PROPERTIES PASS_REGULAR_EXPRESSION "${_usage_regex}")
    endif()
  endforeach()
endif ()

# Native SSTV CI selects this target before running ``ctest -LE sstv``.  The
# ordinary tests otherwise exist only as individual executable targets, so a
# label-excluded CTest invocation can silently turn into a collection of
# "Not Run" entries when the build step materialises SSTV tests only.  Keep
# hardware-only ``test_rtlsdr_input`` out: it is intentionally not registered
# with CTest and needs a real USB receiver.
add_custom_target (decodium_regression_test_binaries)
set (_decodium_regression_test_targets
  test_qt_helpers
  test_qso_sequencer_rules
  test_tx_pipeline
  test_ptt_transition_policy
  test_audio_sink_tx_gate
  test_amplifier_telemetry
  test_station_telemetry_codec
  test_qmx_telemetry
  test_decodium_update_support
  test_jt4_roundtrip
  test_dx_cluster_band
  test_map_layer_service
  test_sgp4
  test_rotator_service
  test_cat4om_manager
  test_civ_address_settings
  test_decoport_rig_driver_thread
  test_udp_client_id
  test_ftx_weak_decode
  test_ft2_qso_sim
  test_secure_settings
  test_callsign_intelligence
  test_dxcc_lookup
  test_filedownload
  test_streaming_list_model
  test_decode_list_model
  test_ft_decode_thread_budget
  test_legacy_decode_window
  test_b4_worked_before
  test_ft2link
  test_ft2link_satellite_half_duplex
  test_linux_drm_gpu_usage
  test_rtlsdr_dsp
  test_rtlsdr_rf_spectrum
  test_rtlsdr_capabilities
  test_rtlsdr_tuning_plan
  test_ft2link_qml_adapter
  )
if (TARGET test_fastldpc_neon)
  list (APPEND _decodium_regression_test_targets test_fastldpc_neon)
endif ()
if (TARGET test_fastldpc_dispatch)
  list (APPEND _decodium_regression_test_targets test_fastldpc_dispatch)
endif ()
foreach (_decodium_regression_test_target IN LISTS
    _decodium_regression_test_targets)
  add_dependencies (decodium_regression_test_binaries
    ${_decodium_regression_test_target})
endforeach ()
unset (_decodium_regression_test_target)
unset (_decodium_regression_test_targets)
