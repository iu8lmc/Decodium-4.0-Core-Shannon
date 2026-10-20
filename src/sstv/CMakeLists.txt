set (DECODIUM_SSTV_CORE_SOURCES
  analog/SstvAvt.cpp
  analog/SstvAvtRxSession.cpp
  analog/SstvAvtSyncCodec.cpp
  analog/SstvMartinM1.cpp
  analog/SstvMartinM1RxSession.cpp
  analog/SstvMmsstvExtended.cpp
  analog/SstvMmsstvExtendedRxSession.cpp
  analog/SstvPd.cpp
  analog/SstvPdRxSession.cpp
  analog/SstvRobot.cpp
  analog/SstvRobotRxSession.cpp
  analog/SstvScottie.cpp
  analog/SstvScottieRxSession.cpp
  analog/SstvSequentialRgb.cpp
  analog/SstvSequentialRgbRxSession.cpp
  core/SstvFskIdCodec.cpp
  core/SstvModeRegistry.cpp
  core/SstvModeSpec.cpp
  core/SstvNarrowVisCodec.cpp
  core/SstvTimingAccumulator.cpp
  core/SstvVisCodec.cpp
  dsp/SstvFrequencyDemodulator.cpp
  dsp/SstvPreprocessor.cpp
  dsp/SstvResampler.cpp
  dsp/SstvSignalMetrics.cpp
  dsp/SstvSlantEstimator.cpp
  dsp/SstvSyncTracker.cpp
  dsp/SstvToneDetector.cpp
  image/SstvColourConverter.cpp
  image/SstvImageFrame.cpp
  rx/SstvAudioRingBuffer.cpp
  rx/SstvFskIdDetector.cpp
  rx/SstvNarrowVisDetector.cpp
  rx/SstvReplayBuffer.cpp
  rx/SstvRxControlPolicy.cpp
  rx/SstvRxCorrectionController.cpp
  rx/SstvRxRetainedAudio.cpp
  rx/SstvRxStateMachine.cpp
  rx/SstvTimingFallbackDetector.cpp
  rx/SstvVisDetector.cpp
  tx/SstvFskIdTxStream.cpp
  tx/SstvToneGenerator.cpp
  tx/SstvTxStateMachine.cpp
  tx/SstvTxStream.cpp
  tx/SstvWavStreamWriter.cpp
  )

set (DECODIUM_SSTV_CORE_HEADERS
  analog/SstvAvt.h
  analog/SstvAvtRxSession.h
  analog/SstvAvtSyncCodec.h
  analog/SstvMartinM1.h
  analog/SstvMartinM1RxSession.h
  analog/SstvMmsstvExtended.h
  analog/SstvMmsstvExtendedRxSession.h
  analog/SstvPd.h
  analog/SstvPdRxSession.h
  analog/SstvRobot.h
  analog/SstvRobotRxSession.h
  analog/SstvScottie.h
  analog/SstvScottieRxSession.h
  analog/SstvSequentialRgb.h
  analog/SstvSequentialRgbRxSession.h
  core/SstvFskIdCodec.h
  core/SstvModeRegistry.h
  core/SstvModeSpec.h
  core/SstvNarrowVisCodec.h
  core/SstvTimingAccumulator.h
  core/SstvTypes.h
  core/SstvVisCodec.h
  dsp/SstvFrequencyDemodulator.h
  dsp/SstvPreprocessor.h
  dsp/SstvResampler.h
  dsp/SstvSignalMetrics.h
  dsp/SstvSlantEstimator.h
  dsp/SstvSyncTracker.h
  dsp/SstvToneDetector.h
  image/SstvColourConverter.h
  image/SstvImageFrame.h
  rx/SstvAudioRingBuffer.h
  rx/SstvFskIdDetector.h
  rx/SstvNarrowVisDetector.h
  rx/SstvReplayBuffer.h
  rx/SstvRxControlPolicy.h
  rx/SstvRxCorrectionController.h
  rx/SstvRxRetainedAudio.h
  rx/SstvRxStateMachine.h
  rx/SstvTimingFallbackDetector.h
  rx/SstvVisDetector.h
  tx/SstvFskIdTxStream.h
  tx/SstvToneGenerator.h
  tx/SstvTxStateMachine.h
  tx/SstvTxStream.h
  tx/SstvWavStreamWriter.h
  )

find_package (Threads REQUIRED)

add_library (decodium_sstv_diagnostic_logging STATIC
  diagnostics/SstvDiagnosticLogging.cpp
  diagnostics/SstvDiagnosticLogging.h
  )
add_library (Decodium::SstvDiagnosticLogging ALIAS
  decodium_sstv_diagnostic_logging)
target_include_directories (decodium_sstv_diagnostic_logging
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_diagnostic_logging PUBLIC Qt6::Core)
target_compile_features (decodium_sstv_diagnostic_logging PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_diagnostic_logging PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options (decodium_sstv_diagnostic_logging PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif ()

add_library (decodium_sstv_core STATIC
  ${DECODIUM_SSTV_CORE_SOURCES}
  ${DECODIUM_SSTV_CORE_HEADERS}
  )
add_library (Decodium::SstvCore ALIAS decodium_sstv_core)

target_include_directories (decodium_sstv_core
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_core
  PUBLIC Decodium::SstvDiagnosticLogging
  PRIVATE Threads::Threads)
target_compile_features (decodium_sstv_core PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_core PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

add_library (decodium_sstv_integration STATIC
  integration/SstvAudioIngress.cpp
  integration/SstvAudioIngress.h
  integration/SstvPcm16Queue.cpp
  integration/SstvPcm16Queue.h
  integration/SstvQsoLog.cpp
  integration/SstvQsoLog.h
  integration/SstvRxAudioJobController.cpp
  integration/SstvRxAudioJobController.h
  integration/SstvRxRuntime.cpp
  integration/SstvRxRuntime.h
  integration/SstvTxAudioDevice.cpp
  integration/SstvTxAudioDevice.h
  integration/SstvTxCoordinator.cpp
  integration/SstvTxCoordinator.h
  integration/SstvTxSources.cpp
  integration/SstvTxSources.h
  integration/SstvWavExporter.cpp
  integration/SstvWavExporter.h
  integration/SstvWavPcmReader.cpp
  integration/SstvWavPcmReader.h
  integration/SstvWavReplayController.cpp
  integration/SstvWavReplayController.h
  )
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  set_source_files_properties (integration/SstvQsoLog.cpp PROPERTIES
    COMPILE_OPTIONS
      "-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion;-Werror")
endif ()
add_library (Decodium::SstvIntegration ALIAS decodium_sstv_integration)
target_include_directories (decodium_sstv_integration
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_integration
  PUBLIC
    Decodium::SstvCore
  PRIVATE
    Qt6::Core
    Qt6::Concurrent
    Qt6::Gui
  )
target_compile_features (decodium_sstv_integration PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_integration PROPERTIES
  AUTOMOC ON
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

add_library (decodium_sstv_studio STATIC
  integration/SstvStudioController.cpp
  integration/SstvStudioController.h
  tx/SstvImagePreprocessor.cpp
  tx/SstvImagePreprocessor.h
  )
add_library (Decodium::SstvStudio ALIAS decodium_sstv_studio)
target_include_directories (decodium_sstv_studio
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_studio
  PUBLIC
    Decodium::SstvCore
    Qt6::Core
    Qt6::Gui
  PRIVATE
    Decodium::SstvIntegration
    Qt6::Concurrent
  )
target_compile_features (decodium_sstv_studio PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_studio PROPERTIES
  AUTOMOC ON
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

add_library (decodium_sstv_storage STATIC
  storage/SstvGalleryQuery.cpp
  storage/SstvGalleryQuery.h
  storage/SstvImageStorage.cpp
  storage/SstvImageStorage.h
  storage/SstvTxGalleryArchive.cpp
  storage/SstvTxGalleryArchive.h
  storage/SstvStorageWorker.cpp
  storage/SstvStorageWorker.h
  )

add_library (Decodium::SstvStorage ALIAS decodium_sstv_storage)
target_include_directories (decodium_sstv_storage
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_storage
  PUBLIC
    Decodium::SstvDiagnosticLogging
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
  )
target_compile_features (decodium_sstv_storage PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_storage PROPERTIES
  AUTOMOC ON
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

add_library (decodium_sstv_gallery STATIC
  models/SstvGalleryModel.cpp
  models/SstvGalleryModel.h
  models/SstvThumbnailProvider.cpp
  models/SstvThumbnailProvider.h
  )
add_library (Decodium::SstvGallery ALIAS decodium_sstv_gallery)
target_include_directories (decodium_sstv_gallery
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_gallery
  PUBLIC
    Decodium::SstvStorage
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
  )
target_compile_features (decodium_sstv_gallery PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_gallery PROPERTIES
  AUTOMOC ON
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

add_library (decodium_sstv_diagnostics STATIC
  diagnostics/SstvDiagnosticsController.cpp
  diagnostics/SstvDiagnosticsController.h
  )
add_library (Decodium::SstvDiagnostics ALIAS decodium_sstv_diagnostics)
target_include_directories (decodium_sstv_diagnostics
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_diagnostics
  PUBLIC
    Decodium::SstvDiagnosticLogging
    Qt6::Core
  PRIVATE
    Decodium::SstvCore
  )
target_compile_features (decodium_sstv_diagnostics PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_diagnostics PROPERTIES
  AUTOMOC ON
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options (decodium_sstv_diagnostics PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif ()

add_library (decodium_sstv_sharing_core STATIC
  sharing/SstvLocalIntegrationShareProvider.cpp
  sharing/SstvLocalIntegrationShareProvider.h
  sharing/SstvShareManifest.cpp
  sharing/SstvShareManifest.h
  sharing/SstvShareProvider.h
  sharing/SstvShareSecurity.cpp
  sharing/SstvShareSecurity.h
  sharing/SstvShareTransfer.cpp
  sharing/SstvShareTransfer.h
  )
add_library (Decodium::SstvSharingCore ALIAS decodium_sstv_sharing_core)
target_include_directories (decodium_sstv_sharing_core
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_sharing_core PUBLIC Qt6::Core)
target_compile_features (decodium_sstv_sharing_core PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_sharing_core PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

add_library (decodium_sstv_sharing_queue STATIC
  sharing/SstvIncomingMediaValidator.cpp
  sharing/SstvIncomingMediaValidator.h
  sharing/SstvShareQueueManager.cpp
  sharing/SstvShareQueueManager.h
  )
add_library (Decodium::SstvSharingQueue ALIAS decodium_sstv_sharing_queue)
target_include_directories (decodium_sstv_sharing_queue
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_sharing_queue
  PUBLIC
    Decodium::SstvSharingCore
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
  )
target_compile_features (decodium_sstv_sharing_queue PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_sharing_queue PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

# Network providers are a separate target so the security-critical production
# compile never inherits the loopback-only plaintext test gate.
add_library (decodium_sstv_sharing_http STATIC
  sharing/SstvHttpShareProviders.cpp
  sharing/SstvHttpShareProviders.h
  )
add_library (Decodium::SstvSharingHttp ALIAS decodium_sstv_sharing_http)
target_include_directories (decodium_sstv_sharing_http
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_sharing_http
  PUBLIC
    Decodium::SstvSharingCore
    Qt6::Core
    Qt6::Network
  )
target_compile_features (decodium_sstv_sharing_http PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_sharing_http PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

add_library (decodium_sstv_sharing_ui STATIC
  models/SstvShareController.cpp
  models/SstvShareController.h
  )
add_library (Decodium::SstvSharingUi ALIAS decodium_sstv_sharing_ui)
target_include_directories (decodium_sstv_sharing_ui
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_sharing_ui
  PUBLIC
    Decodium::SstvDiagnosticLogging
    Decodium::SstvSharingQueue
    Decodium::SstvSharingHttp
    Qt6::Core
    Qt6::Gui
  )
target_compile_features (decodium_sstv_sharing_ui PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_sharing_ui PROPERTIES
  AUTOMOC ON
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

if (DECODIUM_ENABLE_HAMDRM)
  add_subdirectory (digital)
endif ()
