function (add_decodium_sstv_test test_name)
  add_executable (${test_name} ${ARGN})
  target_link_libraries (${test_name}
    PRIVATE
      Decodium::SstvCore
      Qt6::Core
      Qt6::Test
    )
  set_target_properties (${test_name} PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
  set_tests_properties (${test_name} PROPERTIES LABELS "sstv;unit")
endfunction ()

function (add_decodium_sstv_integration_test test_name)
  add_executable (${test_name} ${ARGN})
  target_link_libraries (${test_name}
    PRIVATE
      Decodium::SstvIntegration
      Qt6::Core
      Qt6::Test
    )
  set_target_properties (${test_name} PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
  set_tests_properties (${test_name} PROPERTIES LABELS "sstv;integration")
endfunction ()

add_decodium_sstv_test (test_sstv_mode_registry test_sstv_mode_registry.cpp)
add_decodium_sstv_test (test_sstv_mode_docs test_sstv_mode_docs.cpp)
target_compile_definitions (test_sstv_mode_docs PRIVATE
  DECODIUM_SSTV_MODE_CATALOG_PATH="${CMAKE_SOURCE_DIR}/docs/sstv/MODE_CATALOG.md"
  DECODIUM_SSTV_MODE_MATRIX_PATH="${CMAKE_SOURCE_DIR}/docs/sstv/MODE_MATRIX.md"
  )
add_executable (generate_sstv_mode_matrix EXCLUDE_FROM_ALL
  tools/generate_sstv_mode_matrix.cpp)
target_link_libraries (generate_sstv_mode_matrix
  PRIVATE Decodium::SstvCore Qt6::Core)
set_target_properties (generate_sstv_mode_matrix PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_decodium_sstv_test (test_sstv_timing test_sstv_timing.cpp)
add_decodium_sstv_test (test_sstv_vis test_sstv_vis.cpp)
add_decodium_sstv_test (test_sstv_narrow_vis test_sstv_narrow_vis.cpp)
target_compile_definitions (test_sstv_narrow_vis PRIVATE
  DECODIUM_SSTV_MMSSTV_EXTENDED_FIXTURE="${CMAKE_SOURCE_DIR}/tests/sstv/fixtures/mmsstv-8060b5-extended-mode-landmarks.json"
  )
add_decodium_sstv_test (test_sstv_mmsstv_vis test_sstv_mmsstv_vis.cpp)
target_compile_definitions (test_sstv_mmsstv_vis PRIVATE
  DECODIUM_SSTV_MMSSTV_EXTENDED_FIXTURE="${CMAKE_SOURCE_DIR}/tests/sstv/fixtures/mmsstv-8060b5-extended-mode-landmarks.json"
  )
add_decodium_sstv_test (test_sstv_mmsstv_extended
  test_sstv_mmsstv_extended.cpp)
target_compile_definitions (test_sstv_mmsstv_extended PRIVATE
  DECODIUM_SSTV_MMSSTV_EXTENDED_FIXTURE="${CMAKE_SOURCE_DIR}/tests/sstv/fixtures/mmsstv-8060b5-extended-mode-landmarks.json"
  )
add_decodium_sstv_test (test_sstv_narrow_vis_detector
  test_sstv_narrow_vis_detector.cpp)
add_decodium_sstv_test (test_sstv_fskid test_sstv_fskid.cpp)
add_decodium_sstv_test (test_sstv_fskid_tx_stream
  test_sstv_fskid_tx_stream.cpp)
add_decodium_sstv_test (test_sstv_fskid_detector
  test_sstv_fskid_detector.cpp)
add_decodium_sstv_test (test_sstv_resampler test_sstv_resampler.cpp)
add_decodium_sstv_test (test_sstv_audio_buffer test_sstv_audio_buffer.cpp)
add_decodium_sstv_test (test_sstv_tone_generator test_sstv_tone_generator.cpp)
add_decodium_sstv_test (test_sstv_tx_state_machine
  test_sstv_tx_state_machine.cpp)
add_decodium_sstv_test (test_sstv_rx_state_machine test_sstv_rx_state_machine.cpp)
add_decodium_sstv_test (test_sstv_rx_controls test_sstv_rx_controls.cpp)
add_decodium_sstv_integration_test (test_sstv_rx_audio_job_controller
  test_sstv_rx_audio_job_controller.cpp)
target_link_libraries (test_sstv_rx_audio_job_controller
  PRIVATE Qt6::Concurrent)
add_decodium_sstv_test (test_sstv_tone_detector test_sstv_tone_detector.cpp)
add_decodium_sstv_test (test_sstv_image_core test_sstv_image_core.cpp)
add_decodium_sstv_test (test_sstv_rx_frontend test_sstv_rx_frontend.cpp)
add_decodium_sstv_test (test_sstv_wav_stream test_sstv_wav_stream.cpp)
add_decodium_sstv_test (test_sstv_vis_detector test_sstv_vis_detector.cpp)
add_decodium_sstv_test (test_sstv_sync_tracker test_sstv_sync_tracker.cpp)
add_decodium_sstv_test (test_sstv_avt_sync test_sstv_avt_sync.cpp)
add_decodium_sstv_test (test_sstv_avt test_sstv_avt.cpp)
target_compile_definitions (test_sstv_avt PRIVATE
  DECODIUM_SSTV_AVT_FIXTURE="${CMAKE_SOURCE_DIR}/tests/sstv/fixtures/avt-handbook-qsstv-landmarks.json"
  )
add_decodium_sstv_test (test_sstv_avt_rx_session
  test_sstv_avt_rx_session.cpp)
add_decodium_sstv_test (test_sstv_martin_m1 test_sstv_martin_m1.cpp)
add_decodium_sstv_test (test_sstv_martin_m1_rx_session
  test_sstv_martin_m1_rx_session.cpp)
add_decodium_sstv_test (test_sstv_martin_family
  test_sstv_martin_family.cpp)
target_compile_definitions (test_sstv_martin_family PRIVATE
  DECODIUM_SSTV_MARTIN_LIBSSTV_FIXTURE="${CMAKE_SOURCE_DIR}/tests/sstv/fixtures/libsstv-193157-martin-m2-m3-m4-landmarks.json"
  )
add_decodium_sstv_test (test_sstv_martin_family_rx_session
  test_sstv_martin_family_rx_session.cpp)
add_decodium_sstv_test (test_sstv_scottie test_sstv_scottie.cpp)
target_compile_definitions (test_sstv_scottie PRIVATE
  DECODIUM_SSTV_SCOTTIE_LIBSSTV_FIXTURE="${CMAKE_SOURCE_DIR}/tests/sstv/fixtures/libsstv-193157-scottie-s3-s4-landmarks.json"
  )
add_decodium_sstv_test (test_sstv_scottie_rx_session
  test_sstv_scottie_rx_session.cpp)
add_decodium_sstv_test (test_sstv_sequential_rgb
  test_sstv_sequential_rgb.cpp)
target_compile_definitions (test_sstv_sequential_rgb PRIVATE
  DECODIUM_SSTV_SEQUENTIAL_RGB_FIXTURE="${CMAKE_SOURCE_DIR}/tests/sstv/fixtures/pysstv-d998fad-sequential-rgb-landmarks.json"
  )
add_decodium_sstv_test (test_sstv_sequential_rgb_rx_session
  test_sstv_sequential_rgb_rx_session.cpp)
add_decodium_sstv_test (test_sstv_pd test_sstv_pd.cpp)
target_compile_definitions (test_sstv_pd PRIVATE
  DECODIUM_SSTV_PD_PYSSTV_FIXTURE="${CMAKE_SOURCE_DIR}/tests/sstv/fixtures/pysstv-d998fad-pd-landmarks.json"
  DECODIUM_SSTV_PD_LIBSSTV_FIXTURE="${CMAKE_SOURCE_DIR}/tests/sstv/fixtures/libsstv-193157-pd-landmarks.json"
  )
add_decodium_sstv_test (test_sstv_pd_rx_session
  test_sstv_pd_rx_session.cpp)
add_decodium_sstv_test (test_sstv_robot test_sstv_robot.cpp)
target_compile_definitions (test_sstv_robot PRIVATE
  DECODIUM_SSTV_ROBOT_LIBSSTV_FIXTURE="${CMAKE_SOURCE_DIR}/tests/sstv/fixtures/libsstv-193157-robot-landmarks.json"
  )
add_decodium_sstv_test (test_sstv_robot_rx_session
  test_sstv_robot_rx_session.cpp)
add_decodium_sstv_integration_test (test_sstv_audio_ingress
  test_sstv_audio_ingress.cpp)
add_decodium_sstv_integration_test (test_sstv_qso_log
  test_sstv_qso_log.cpp)
add_decodium_sstv_integration_test (test_sstv_rx_runtime
  test_sstv_rx_runtime.cpp)
add_decodium_sstv_integration_test (test_sstv_tx_audio_device
  test_sstv_tx_audio_device.cpp)
add_decodium_sstv_integration_test (test_sstv_tx_coordinator
  test_sstv_tx_coordinator.cpp)
target_link_libraries (test_sstv_tx_coordinator PRIVATE Qt6::Gui)
add_decodium_sstv_integration_test (test_sstv_wav_exporter
  test_sstv_wav_exporter.cpp)
add_decodium_sstv_integration_test (test_sstv_wav_pcm_reader
  test_sstv_wav_pcm_reader.cpp)
add_decodium_sstv_integration_test (test_sstv_wav_replay_controller
  test_sstv_wav_replay_controller.cpp)

set (DECODIUM_SSTV_EXTERNAL_VECTOR_DIR "" CACHE PATH
  "Optional local directory containing pinned independent SSTV WAV vectors")
if (DECODIUM_SSTV_EXTERNAL_VECTOR_DIR)
  get_filename_component (_decodium_sstv_external_vector_dir
    "${DECODIUM_SSTV_EXTERNAL_VECTOR_DIR}" REALPATH)
  if (NOT IS_DIRECTORY "${_decodium_sstv_external_vector_dir}")
    message (FATAL_ERROR
      "DECODIUM_SSTV_EXTERNAL_VECTOR_DIR is not a readable directory")
  endif ()
  if (_decodium_sstv_external_vector_dir MATCHES "[\";]")
    message (FATAL_ERROR
      "DECODIUM_SSTV_EXTERNAL_VECTOR_DIR contains an unsupported character")
  endif ()
  add_executable (test_sstv_external_vectors
    test_sstv_external_vectors.cpp)
  target_link_libraries (test_sstv_external_vectors
    PRIVATE
      Decodium::SstvIntegration
      Qt6::Core
      Qt6::Gui
      Qt6::Test
    )
  target_compile_definitions (test_sstv_external_vectors PRIVATE
    DECODIUM_SSTV_EXTERNAL_VECTOR_DIR="${_decodium_sstv_external_vector_dir}"
    )
  set_target_properties (test_sstv_external_vectors PROPERTIES
    AUTOMOC ON
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME test_sstv_external_vectors
    COMMAND $<TARGET_FILE:test_sstv_external_vectors>)
  set_tests_properties (test_sstv_external_vectors PROPERTIES
    LABELS "sstv;external-fixture;interop;wav;decoder"
    TIMEOUT 600
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
  unset (_decodium_sstv_external_vector_dir)
endif ()

# Keep the final performance claim executable and machine-readable.  The
# target measures both the inactive runtime and a deterministic 12 kHz DSP
# stream; it opens no device and performs no network or file I/O.
add_executable (sstv_performance tools/sstv_performance.cpp)
target_link_libraries (sstv_performance
  PRIVATE
    Decodium::SstvIntegration
    Qt6::Core
  )
set_target_properties (sstv_performance PROPERTIES
  AUTOMOC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME sstv_performance COMMAND $<TARGET_FILE:sstv_performance>)
set_tests_properties (sstv_performance PROPERTIES
  LABELS "sstv;performance;runtime"
  TIMEOUT 30
  )

add_executable (test_sstv_receive_qml test_sstv_receive_qml.cpp)
target_link_libraries (test_sstv_receive_qml
  PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Test
  )
target_compile_definitions (test_sstv_receive_qml PRIVATE
  DECODIUM_SSTV_RECEIVE_QML_SOURCE="${CMAKE_SOURCE_DIR}/qml/decodium/components/sstv/SstvReceivePage.qml"
  )
set_target_properties (test_sstv_receive_qml PROPERTIES
  AUTOMOC ON
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_receive_qml
  COMMAND $<TARGET_FILE:test_sstv_receive_qml>)
set_tests_properties (test_sstv_receive_qml PROPERTIES
  LABELS "sstv;replay;qml;render"
  TIMEOUT 20
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic"
  )

add_executable (test_sstv_settings_qml test_sstv_settings_qml.cpp)
target_link_libraries (test_sstv_settings_qml
  PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Test
  )
target_compile_definitions (test_sstv_settings_qml PRIVATE
  DECODIUM_SSTV_SETTINGS_QML_SOURCE="${CMAKE_SOURCE_DIR}/qml/decodium/components/sstv/SstvSettingsPage.qml"
  )
set_target_properties (test_sstv_settings_qml PROPERTIES
  AUTOMOC ON
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_settings_qml
  COMMAND $<TARGET_FILE:test_sstv_settings_qml>)
set_tests_properties (test_sstv_settings_qml PROPERTIES
  LABELS "sstv;settings;qml;render"
  TIMEOUT 20
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic"
  )

add_executable (test_sstv_image_preprocessor
  test_sstv_image_preprocessor.cpp)
target_link_libraries (test_sstv_image_preprocessor
  PRIVATE
    Decodium::SstvStudio
    Qt6::Core
    Qt6::Gui
    Qt6::Test
  )
set_target_properties (test_sstv_image_preprocessor PROPERTIES
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_image_preprocessor
  COMMAND $<TARGET_FILE:test_sstv_image_preprocessor>)
set_tests_properties (test_sstv_image_preprocessor PROPERTIES
  LABELS "sstv;studio;image"
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )

add_executable (test_sstv_studio_controller
  test_sstv_studio_controller.cpp)
target_link_libraries (test_sstv_studio_controller
  PRIVATE
    Decodium::SstvStudio
    Qt6::Core
    Qt6::Gui
    Qt6::Test
  )
set_target_properties (test_sstv_studio_controller PROPERTIES
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_studio_controller
  COMMAND $<TARGET_FILE:test_sstv_studio_controller>)
set_tests_properties (test_sstv_studio_controller PROPERTIES
  LABELS "sstv;studio;integration"
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )

add_executable (test_sstv_studio_qml
  test_sstv_studio_qml.cpp)
target_link_libraries (test_sstv_studio_qml
  PRIVATE
    Decodium::SstvStudio
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Test
  )
target_compile_definitions (test_sstv_studio_qml PRIVATE
  DECODIUM_SSTV_QML_SOURCE="${CMAKE_SOURCE_DIR}/qml/decodium/components/sstv/SstvTransmitPage.qml"
  )
set_target_properties (test_sstv_studio_qml PROPERTIES
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_studio_qml
  COMMAND $<TARGET_FILE:test_sstv_studio_qml>)
set_tests_properties (test_sstv_studio_qml PROPERTIES
  LABELS "sstv;studio;qml;render"
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic"
  )

add_executable (test_sstv_storage test_sstv_storage.cpp)
target_link_libraries (test_sstv_storage
  PRIVATE
    Decodium::SstvStorage
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
    Qt6::Test
  )
set_target_properties (test_sstv_storage PROPERTIES
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_storage COMMAND $<TARGET_FILE:test_sstv_storage>)
set_tests_properties (test_sstv_storage PROPERTIES
  LABELS "sstv;storage"
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )

add_executable (test_sstv_tx_gallery_archive
  test_sstv_tx_gallery_archive.cpp)
target_link_libraries (test_sstv_tx_gallery_archive
  PRIVATE
    Decodium::SstvStorage
    Qt6::Core
    Qt6::Gui
    Qt6::Test
  )
set_target_properties (test_sstv_tx_gallery_archive PROPERTIES
  AUTOMOC ON
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_tx_gallery_archive
  COMMAND $<TARGET_FILE:test_sstv_tx_gallery_archive>)
set_tests_properties (test_sstv_tx_gallery_archive PROPERTIES
  LABELS "sstv;storage;gallery;tx"
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )

add_executable (test_sstv_incoming_gallery_import
  test_sstv_incoming_gallery_import.cpp)
target_link_libraries (test_sstv_incoming_gallery_import
  PRIVATE
    Decodium::SstvSharingUi
    Decodium::SstvStorage
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Sql
    Qt6::Test
  )
set_target_properties (test_sstv_incoming_gallery_import PROPERTIES
  AUTOMOC ON
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_incoming_gallery_import
  COMMAND $<TARGET_FILE:test_sstv_incoming_gallery_import>)
set_tests_properties (test_sstv_incoming_gallery_import PROPERTIES
  LABELS "sstv;sharing;inbox;storage;gallery;threading;security"
  TIMEOUT 30
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )

add_executable (test_sstv_gallery_model test_sstv_gallery_model.cpp)
target_link_libraries (test_sstv_gallery_model
  PRIVATE
    Decodium::SstvGallery
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Sql
    Qt6::Test
  )
set_target_properties (test_sstv_gallery_model PROPERTIES
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_gallery_model
  COMMAND $<TARGET_FILE:test_sstv_gallery_model>)
set_tests_properties (test_sstv_gallery_model PROPERTIES
  LABELS "sstv;gallery;storage;model"
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software"
  )

add_executable (test_sstv_gallery_qml test_sstv_gallery_qml.cpp)
target_link_libraries (test_sstv_gallery_qml
  PRIVATE
    Decodium::SstvGallery
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::Test
  )
target_compile_definitions (test_sstv_gallery_qml PRIVATE
  DECODIUM_SSTV_GALLERY_QML_SOURCE="${CMAKE_SOURCE_DIR}/qml/decodium/components/sstv/SstvGalleryPage.qml"
  )
set_target_properties (test_sstv_gallery_qml PROPERTIES
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_gallery_qml
  COMMAND $<TARGET_FILE:test_sstv_gallery_qml>)
set_tests_properties (test_sstv_gallery_qml PROPERTIES
  LABELS "sstv;gallery;qml;render"
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic"
  )

add_executable (test_sstv_diagnostics test_sstv_diagnostics.cpp)
target_link_libraries (test_sstv_diagnostics
  PRIVATE
    Decodium::SstvDiagnostics
    Qt6::Core
    Qt6::Test
  )
set_target_properties (test_sstv_diagnostics PROPERTIES
  AUTOMOC ON
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_diagnostics
  COMMAND $<TARGET_FILE:test_sstv_diagnostics>)
set_tests_properties (test_sstv_diagnostics PROPERTIES
  LABELS "sstv;diagnostics;security;threading;export"
  TIMEOUT 30
  )

add_executable (test_sstv_diagnostics_qml
  test_sstv_diagnostics_qml.cpp)
target_link_libraries (test_sstv_diagnostics_qml
  PRIVATE
    Decodium::SstvDiagnostics
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Test
  )
target_compile_definitions (test_sstv_diagnostics_qml PRIVATE
  DECODIUM_SSTV_DIAGNOSTICS_QML_SOURCE="${CMAKE_SOURCE_DIR}/qml/decodium/components/sstv/SstvDiagnosticsPage.qml"
  )
set_target_properties (test_sstv_diagnostics_qml PROPERTIES
  AUTOMOC ON
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_diagnostics_qml
  COMMAND $<TARGET_FILE:test_sstv_diagnostics_qml>)
set_tests_properties (test_sstv_diagnostics_qml PROPERTIES
  LABELS "sstv;diagnostics;qml;render;export"
  TIMEOUT 20
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic"
  )

add_executable (test_sstv_qso_qml test_sstv_qso_qml.cpp)
target_link_libraries (test_sstv_qso_qml
  PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Test
  )
target_compile_definitions (test_sstv_qso_qml PRIVATE
  DECODIUM_SSTV_QSO_QML_SOURCE="${CMAKE_SOURCE_DIR}/qml/decodium/components/sstv/SstvLogQsoDialog.qml"
  )
set_target_properties (test_sstv_qso_qml PROPERTIES
  AUTOMOC ON
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_qso_qml
  COMMAND $<TARGET_FILE:test_sstv_qso_qml>)
set_tests_properties (test_sstv_qso_qml PROPERTIES
  LABELS "sstv;qso;adif;gallery;qml;render"
  TIMEOUT 20
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic"
  )

add_executable (test_sstv_sharing_core test_sstv_sharing_core.cpp)
target_link_libraries (test_sstv_sharing_core
  PRIVATE
    Decodium::SstvSharingCore
    Qt6::Core
  )
set_target_properties (test_sstv_sharing_core PROPERTIES
  AUTOMOC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_sharing_core
  COMMAND $<TARGET_FILE:test_sstv_sharing_core>)
set_tests_properties (test_sstv_sharing_core PROPERTIES
  LABELS "sstv;sharing;security"
  )

add_executable (test_sstv_share_queue_manager
  test_sstv_share_queue_manager.cpp)
target_link_libraries (test_sstv_share_queue_manager
  PRIVATE
    Decodium::SstvSharingQueue
    Qt6::Core
    Qt6::Sql
  )
set_target_properties (test_sstv_share_queue_manager PROPERTIES
  AUTOMOC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_share_queue_manager
  COMMAND $<TARGET_FILE:test_sstv_share_queue_manager>)
set_tests_properties (test_sstv_share_queue_manager PROPERTIES
  LABELS "sstv;sharing;queue;storage;security"
  TIMEOUT 30
  )

add_executable (test_sstv_incoming_media_validator
  test_sstv_incoming_media_validator.cpp)
target_link_libraries (test_sstv_incoming_media_validator
  PRIVATE
    Decodium::SstvSharingQueue
    Qt6::Core
    Qt6::Gui
    Qt6::Test
  )
set_target_properties (test_sstv_incoming_media_validator PROPERTIES
  AUTOMOC ON
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_incoming_media_validator
  COMMAND $<TARGET_FILE:test_sstv_incoming_media_validator>)
set_tests_properties (test_sstv_incoming_media_validator PROPERTIES
  LABELS "sstv;sharing;inbox;image;security"
  TIMEOUT 20
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )

add_executable (test_sstv_share_controller
  test_sstv_share_controller.cpp)
target_link_libraries (test_sstv_share_controller
  PRIVATE
    Decodium::SstvSharingUi
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Sql
    Qt6::Test
  )
set_target_properties (test_sstv_share_controller PROPERTIES
  AUTOMOC ON
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_share_controller
  COMMAND $<TARGET_FILE:test_sstv_share_controller>)
set_tests_properties (test_sstv_share_controller PROPERTIES
  LABELS "sstv;sharing;controller;threading;security"
  TIMEOUT 20
  )

add_executable (test_sstv_local_integration_provider
  test_sstv_local_integration_provider.cpp)
target_link_libraries (test_sstv_local_integration_provider
  PRIVATE
    Decodium::SstvSharingUi
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Sql
    Qt6::Test
  )
set_target_properties (test_sstv_local_integration_provider PROPERTIES
  AUTOMOC ON
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_local_integration_provider
  COMMAND $<TARGET_FILE:test_sstv_local_integration_provider>)
set_tests_properties (test_sstv_local_integration_provider PROPERTIES
  LABELS "sstv;sharing;local-integration;queue;controller;security"
  TIMEOUT 30
  )

add_executable (test_sstv_share_qml test_sstv_share_qml.cpp)
target_link_libraries (test_sstv_share_qml
  PRIVATE
    Decodium::SstvSharingUi
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::Test
  )
target_compile_definitions (test_sstv_share_qml PRIVATE
  DECODIUM_SSTV_SHARE_QML_SOURCE="${CMAKE_SOURCE_DIR}/qml/decodium/components/sstv/SstvSharePage.qml"
  )
set_target_properties (test_sstv_share_qml PROPERTIES
  AUTOMOC ON
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_share_qml
  COMMAND $<TARGET_FILE:test_sstv_share_qml>)
set_tests_properties (test_sstv_share_qml PROPERTIES
  LABELS "sstv;sharing;qml;render;privacy"
  TIMEOUT 20
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic"
  )

# Compile the transport directly with the double-gated localhost test escape.
# The production Decodium::SstvSharingHttp target above never receives it.
add_executable (test_sstv_http_providers
  test_sstv_http_providers.cpp
  ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvHttpShareProviders.cpp)
target_link_libraries (test_sstv_http_providers
  PRIVATE
    Decodium::SstvSharingCore
    Qt6::Core
    Qt6::Network
  )
target_compile_definitions (test_sstv_http_providers PRIVATE
  DECODIUM_SSTV_ALLOW_INSECURE_LOCAL_TEST_TRANSPORT=1
  DECODIUM_SSTV_PROVIDER_TESTING=1)
set_target_properties (test_sstv_http_providers PROPERTIES
  AUTOMOC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_http_providers
  COMMAND $<TARGET_FILE:test_sstv_http_providers>)
set_tests_properties (test_sstv_http_providers PROPERTIES
  LABELS "sstv;sharing;network;security"
  TIMEOUT 30
  )

# Compile and execute the explicit production-gate case against the normal
# provider library to prove a runtime flag alone cannot enable plaintext.
add_executable (test_sstv_http_provider_production_gate
  test_sstv_http_providers.cpp)
target_link_libraries (test_sstv_http_provider_production_gate
  PRIVATE
    Decodium::SstvSharingHttp
    Qt6::Core
    Qt6::Network
  )
set_target_properties (test_sstv_http_provider_production_gate PROPERTIES
  AUTOMOC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_http_provider_production_gate
  COMMAND $<TARGET_FILE:test_sstv_http_provider_production_gate>
    plaintextTransportNeedsBothCompileAndRuntimeTestGates)
set_tests_properties (test_sstv_http_provider_production_gate PROPERTIES
  LABELS "sstv;sharing;network;security;production-gate"
  TIMEOUT 10
  )

# This deterministic target keeps the seed corpus and parser invariants
# executable on every supported compiler. It does not require libFuzzer and is
# the only fuzz-hardening target included in an ordinary BUILD_TESTING build.
add_executable (test_sstv_fuzz_smoke
  test_sstv_fuzz_smoke.cpp
  ${CMAKE_SOURCE_DIR}/src/sstv/integration/SstvQsoLog.cpp
  ${CMAKE_SOURCE_DIR}/src/sstv/integration/SstvQsoLog.h
  ${CMAKE_SOURCE_DIR}/src/sstv/integration/SstvWavPcmReader.cpp
  ${CMAKE_SOURCE_DIR}/src/sstv/integration/SstvWavPcmReader.h
  )
target_include_directories (test_sstv_fuzz_smoke PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries (test_sstv_fuzz_smoke
  PRIVATE
    Decodium::SstvSharingCore
    Qt6::Core
  )
target_compile_definitions (test_sstv_fuzz_smoke PRIVATE
  DECODIUM_SSTV_FUZZ_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus"
  )
set_target_properties (test_sstv_fuzz_smoke PROPERTIES
  AUTOMOC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_fuzz_smoke
  COMMAND $<TARGET_FILE:test_sstv_fuzz_smoke>)
set_tests_properties (test_sstv_fuzz_smoke PROPERTIES
  LABELS "sstv;fuzz-smoke;parser;security"
  TIMEOUT 20
  )

# Keep the exact remote-image staging boundary under ordinary CTest as well as
# the libFuzzer build.  This exercises hostile bytes through the real
# hash/MIME/decode/normalise/reinspect path without requiring a Clang runtime.
add_executable (test_sstv_incoming_media_fuzz_smoke
  test_sstv_incoming_media_fuzz_smoke.cpp
  fuzz/fuzz_sstv_incoming_media.cpp)
target_include_directories (test_sstv_incoming_media_fuzz_smoke PRIVATE
  ${CMAKE_SOURCE_DIR})
target_link_libraries (test_sstv_incoming_media_fuzz_smoke
  PRIVATE
    Decodium::SstvSharingQueue
    Qt6::Core
    Qt6::Gui
  )
target_compile_definitions (test_sstv_incoming_media_fuzz_smoke PRIVATE
  DECODIUM_SSTV_INCOMING_MEDIA_FUZZ_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/incoming-media"
  )
set_target_properties (test_sstv_incoming_media_fuzz_smoke PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_incoming_media_fuzz_smoke
  COMMAND $<TARGET_FILE:test_sstv_incoming_media_fuzz_smoke>)
set_tests_properties (test_sstv_incoming_media_fuzz_smoke PROPERTIES
  LABELS "sstv;fuzz-smoke;sharing;inbox;image;security"
  TIMEOUT 30
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )

add_executable (test_sstv_protocol_fuzz_smoke
  test_sstv_protocol_fuzz_smoke.cpp
  fuzz/fuzz_sstv_protocol_parsers.cpp)
target_include_directories (test_sstv_protocol_fuzz_smoke PRIVATE
  ${CMAKE_SOURCE_DIR})
target_link_libraries (test_sstv_protocol_fuzz_smoke
  PRIVATE Decodium::SstvCore)
set_target_properties (test_sstv_protocol_fuzz_smoke PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
add_test (NAME test_sstv_protocol_fuzz_smoke
  COMMAND $<TARGET_FILE:test_sstv_protocol_fuzz_smoke>)
set_tests_properties (test_sstv_protocol_fuzz_smoke PROPERTIES
  LABELS "sstv;fuzz-smoke;parser;security;vis;fskid"
  TIMEOUT 30
  )

if (DECODIUM_ENABLE_SSTV_FUZZING)
  function (configure_decodium_sstv_fuzzer target_name)
    target_compile_options (${target_name} PRIVATE
      -fsanitize=fuzzer,address,undefined
      -fno-omit-frame-pointer
      )
    # LINK_FLAGS keeps this developer-only branch compatible with the
    # repository's declared CMake 3.7 baseline.
    set_property (TARGET ${target_name} APPEND_STRING PROPERTY LINK_FLAGS
      " -fsanitize=fuzzer,address,undefined")
    set_target_properties (${target_name} PROPERTIES
      AUTOMOC OFF
      AUTORCC OFF
      AUTOUIC OFF
      CXX_EXTENSIONS OFF
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON
      )
  endfunction ()

  # Compile the parser source into each fuzzer instead of linking the normal
  # static runtime libraries, so libFuzzer coverage and sanitizers instrument
  # the code under test as well as the small harness.
  add_executable (fuzz_sstv_wav_pcm_reader
    fuzz/fuzz_sstv_wav_pcm_reader.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/integration/SstvWavPcmReader.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/integration/SstvWavPcmReader.h
    )
  target_include_directories (fuzz_sstv_wav_pcm_reader PRIVATE
    ${CMAKE_SOURCE_DIR})
  target_link_libraries (fuzz_sstv_wav_pcm_reader PRIVATE Qt6::Core)
  configure_decodium_sstv_fuzzer (fuzz_sstv_wav_pcm_reader)

  add_executable (fuzz_sstv_share_parsers
    fuzz/fuzz_sstv_share_parsers.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvShareManifest.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvShareManifest.h
    ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvShareSecurity.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvShareSecurity.h
    ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvShareTransfer.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvShareTransfer.h
    )
  target_include_directories (fuzz_sstv_share_parsers PRIVATE
    ${CMAKE_SOURCE_DIR})
  target_link_libraries (fuzz_sstv_share_parsers PRIVATE Qt6::Core)
  configure_decodium_sstv_fuzzer (fuzz_sstv_share_parsers)

  # This target deliberately uses the production remote-image validation
  # sequence, including private staging and reinspection, rather than only a
  # header parser. The readable PNG and JPEG seeds reach decode and
  # normalisation; mutated dimensions and byte streams must fail closed before
  # import.
  add_executable (fuzz_sstv_incoming_media
    fuzz/fuzz_sstv_incoming_media.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvIncomingMediaValidator.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvIncomingMediaValidator.h
    ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvShareManifest.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvShareManifest.h
    ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvShareSecurity.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/sharing/SstvShareSecurity.h
    )
  target_include_directories (fuzz_sstv_incoming_media PRIVATE
    ${CMAKE_SOURCE_DIR})
  target_link_libraries (fuzz_sstv_incoming_media PRIVATE
    Qt6::Core
    Qt6::Gui
    )
  configure_decodium_sstv_fuzzer (fuzz_sstv_incoming_media)

  add_executable (fuzz_sstv_qso_log
    fuzz/fuzz_sstv_qso_log.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/integration/SstvQsoLog.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/integration/SstvQsoLog.h
    )
  target_include_directories (fuzz_sstv_qso_log PRIVATE
    ${CMAKE_SOURCE_DIR})
  target_link_libraries (fuzz_sstv_qso_log PRIVATE Qt6::Core)
  configure_decodium_sstv_fuzzer (fuzz_sstv_qso_log)

  add_executable (fuzz_sstv_protocol_parsers
    fuzz/fuzz_sstv_protocol_parsers.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/core/SstvVisCodec.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/core/SstvVisCodec.h
    ${CMAKE_SOURCE_DIR}/src/sstv/core/SstvNarrowVisCodec.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/core/SstvNarrowVisCodec.h
    ${CMAKE_SOURCE_DIR}/src/sstv/core/SstvFskIdCodec.cpp
    ${CMAKE_SOURCE_DIR}/src/sstv/core/SstvFskIdCodec.h
    )
  target_include_directories (fuzz_sstv_protocol_parsers PRIVATE
    ${CMAKE_SOURCE_DIR})
  configure_decodium_sstv_fuzzer (fuzz_sstv_protocol_parsers)
endif ()

if (DECODIUM_ENABLE_HAMDRM)
  add_executable (test_hamdrm_core test_hamdrm_core.cpp)
  target_link_libraries (test_hamdrm_core PRIVATE Decodium::HamDrmCore)
  set_target_properties (test_hamdrm_core PROPERTIES
    AUTOMOC OFF
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME test_hamdrm_core COMMAND $<TARGET_FILE:test_hamdrm_core>)
  set_tests_properties (test_hamdrm_core PROPERTIES
    LABELS "sstv;hamdrm;digital;mot;bsr;security"
    TIMEOUT 20
    )

  add_executable (test_hamdrm_partial_store
    test_hamdrm_partial_store.cpp)
  target_link_libraries (test_hamdrm_partial_store
    PRIVATE
      Decodium::HamDrmPersistence
      Qt6::Core
    )
  set_target_properties (test_hamdrm_partial_store PROPERTIES
    AUTOMOC OFF
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME test_hamdrm_partial_store
    COMMAND $<TARGET_FILE:test_hamdrm_partial_store>)
  set_tests_properties (test_hamdrm_partial_store PROPERTIES
    LABELS "sstv;hamdrm;digital;resume;storage;security"
    TIMEOUT 20
    )

  add_executable (test_hamdrm_jpeg2000 test_hamdrm_jpeg2000.cpp)
  target_link_libraries (test_hamdrm_jpeg2000
    PRIVATE
      Decodium::HamDrmOpenJpeg
  )
  set_target_properties (test_hamdrm_jpeg2000 PROPERTIES
    AUTOMOC OFF
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME test_hamdrm_jpeg2000
    COMMAND $<TARGET_FILE:test_hamdrm_jpeg2000>)
  set_tests_properties (test_hamdrm_jpeg2000 PROPERTIES
    LABELS "sstv;hamdrm;digital;jpeg2000;openjpeg;security"
    TIMEOUT 20
    )

  add_executable (test_hamdrm_controller test_hamdrm_controller.cpp)
  target_link_libraries (test_hamdrm_controller
    PRIVATE
      Decodium::HamDrmUi
      Qt6::Core
      Qt6::Gui
      Qt6::Test
    )
  set_target_properties (test_hamdrm_controller PROPERTIES
    AUTOMOC ON
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME test_hamdrm_controller
    COMMAND $<TARGET_FILE:test_hamdrm_controller>)
  set_tests_properties (test_hamdrm_controller PROPERTIES
    LABELS "sstv;hamdrm;digital;controller;resume;security"
    TIMEOUT 30
    )

  add_executable (test_sstv_digital_qml test_sstv_digital_qml.cpp)
  target_link_libraries (test_sstv_digital_qml
    PRIVATE
      Decodium::HamDrmUi
      Qt6::Core
      Qt6::Gui
      Qt6::Qml
      Qt6::Quick
      Qt6::QuickControls2
      Qt6::Test
    )
  target_compile_definitions (test_sstv_digital_qml PRIVATE
    DECODIUM_SSTV_DIGITAL_QML_SOURCE="${CMAKE_SOURCE_DIR}/qml/decodium/components/sstv/SstvDigitalPage.qml"
    DECODIUM_SSTV_WORKSPACE_QML_SOURCE="${CMAKE_SOURCE_DIR}/qml/decodium/components/sstv/SstvWorkspace.qml"
    )
  set_target_properties (test_sstv_digital_qml PROPERTIES
    AUTOMOC ON
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME test_sstv_digital_qml
    COMMAND $<TARGET_FILE:test_sstv_digital_qml>)
  set_tests_properties (test_sstv_digital_qml PROPERTIES
    LABELS "sstv;hamdrm;digital;qml;render"
    TIMEOUT 30
    ENVIRONMENT
      "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic"
    )

  add_executable (test_hamdrm_phy test_hamdrm_phy.cpp)
  target_link_libraries (test_hamdrm_phy PRIVATE Decodium::HamDrmCore)
  set_target_properties (test_hamdrm_phy PROPERTIES
    AUTOMOC OFF
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME test_hamdrm_phy COMMAND $<TARGET_FILE:test_hamdrm_phy>)
  set_tests_properties (test_hamdrm_phy PROPERTIES
    LABELS "sstv;hamdrm;digital;phy;ofdm;qam"
    TIMEOUT 30
    )

  add_executable (test_hamdrm_channel test_hamdrm_channel.cpp)
  target_link_libraries (test_hamdrm_channel PRIVATE Decodium::HamDrmCore)
  set_target_properties (test_hamdrm_channel PROPERTIES
    AUTOMOC OFF
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME test_hamdrm_channel
    COMMAND $<TARGET_FILE:test_hamdrm_channel>)
  set_tests_properties (test_hamdrm_channel PROPERTIES
    LABELS "sstv;hamdrm;digital;channel;fac;msc;fec;interleaver"
    TIMEOUT 60
    )

  add_executable (test_hamdrm_waveform test_hamdrm_waveform.cpp)
  target_link_libraries (test_hamdrm_waveform
    PRIVATE Decodium::HamDrmCore)
  set_target_properties (test_hamdrm_waveform PROPERTIES
    AUTOMOC OFF
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME test_hamdrm_waveform
    COMMAND $<TARGET_FILE:test_hamdrm_waveform>)
  set_tests_properties (test_hamdrm_waveform PROPERTIES
    LABELS "sstv;hamdrm;digital;waveform;ofdm;loopback"
    TIMEOUT 180
    )

  add_executable (test_hamdrm_waveform_adapters
    test_hamdrm_waveform_adapters.cpp)
  target_link_libraries (test_hamdrm_waveform_adapters
    PRIVATE
      Decodium::HamDrmRuntime
      Qt6::Core
  )
  set_target_properties (test_hamdrm_waveform_adapters PROPERTIES
    AUTOMOC OFF
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options (test_hamdrm_waveform_adapters PRIVATE
      -Wall -Wextra -Wpedantic -Werror)
  endif ()
  add_test (NAME test_hamdrm_waveform_adapters
    COMMAND $<TARGET_FILE:test_hamdrm_waveform_adapters>)
  set_tests_properties (test_hamdrm_waveform_adapters PROPERTIES
    LABELS "sstv;hamdrm;digital;waveform;adapter;controller;integration"
    TIMEOUT 60
    )

  add_executable (test_hamdrm_fuzz_smoke
    test_hamdrm_fuzz_smoke.cpp
    fuzz/fuzz_hamdrm_parsers.cpp
    )
  target_link_libraries (test_hamdrm_fuzz_smoke
    PRIVATE Decodium::HamDrmCore)
  set_target_properties (test_hamdrm_fuzz_smoke PROPERTIES
    AUTOMOC OFF
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME test_hamdrm_fuzz_smoke
    COMMAND $<TARGET_FILE:test_hamdrm_fuzz_smoke>)
  set_tests_properties (test_hamdrm_fuzz_smoke PROPERTIES
    LABELS "sstv;hamdrm;digital;fuzz-smoke;parser;security"
    TIMEOUT 30
    )

  if (DECODIUM_ENABLE_SSTV_FUZZING)
    add_executable (fuzz_hamdrm_parsers
      fuzz/fuzz_hamdrm_parsers.cpp
      ${CMAKE_SOURCE_DIR}/src/sstv/digital/HamDrmBsrCodec.cpp
      ${CMAKE_SOURCE_DIR}/src/sstv/digital/HamDrmBsrCodec.h
      ${CMAKE_SOURCE_DIR}/src/sstv/digital/HamDrmCrc.cpp
      ${CMAKE_SOURCE_DIR}/src/sstv/digital/HamDrmCrc.h
      ${CMAKE_SOURCE_DIR}/src/sstv/digital/HamDrmImageValidator.cpp
      ${CMAKE_SOURCE_DIR}/src/sstv/digital/HamDrmImageValidator.h
      ${CMAKE_SOURCE_DIR}/src/sstv/digital/HamDrmMotCodec.cpp
      ${CMAKE_SOURCE_DIR}/src/sstv/digital/HamDrmMotCodec.h
      ${CMAKE_SOURCE_DIR}/src/sstv/digital/HamDrmProfileRegistry.cpp
      ${CMAKE_SOURCE_DIR}/src/sstv/digital/HamDrmProfileRegistry.h
      )
    target_include_directories (fuzz_hamdrm_parsers PRIVATE
      ${CMAKE_SOURCE_DIR})
    configure_decodium_sstv_fuzzer (fuzz_hamdrm_parsers)
  endif ()
endif ()

# Give CI and developers one stable target that materialises every ordinary
# SSTV/HAMDRM test executable declared in this directory.  Fuzzers are kept
# behind their explicit developer option and are run with bounded corpora, not
# as part of this aggregate.
get_property (_decodium_sstv_directory_targets
  DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
add_custom_target (decodium_sstv_test_binaries)
foreach (_decodium_sstv_target IN LISTS _decodium_sstv_directory_targets)
  if (_decodium_sstv_target MATCHES "^fuzz_")
    continue ()
  endif ()
  get_target_property (_decodium_sstv_target_type
    ${_decodium_sstv_target} TYPE)
  if (_decodium_sstv_target_type STREQUAL "EXECUTABLE")
    add_dependencies (decodium_sstv_test_binaries
      ${_decodium_sstv_target})
  endif ()
endforeach ()
unset (_decodium_sstv_target_type)
unset (_decodium_sstv_target)
unset (_decodium_sstv_directory_targets)
