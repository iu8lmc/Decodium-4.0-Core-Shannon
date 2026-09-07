cmake_minimum_required (VERSION 3.7.2 FATAL_ERROR)

if (APPLE)
  #
  # The following variable defines the default deployment target for macOS
  # builds. Qt 6 no longer matches the old 10.12 baseline used by the Qt 5
  # packaging line, so keep the local default modern and let CI / release
  # jobs override it explicitly when they target a specific macOS line.
  #
  # Among other things this should match the portability and compatibility
  # settings of the Qt framework dylibs, which can be checked as follows:
  #
  # otool -l <binary> | grep -A3 LC_VERSION_MIN_MACOSX
  #
  if (NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR "${CMAKE_OSX_DEPLOYMENT_TARGET}" STREQUAL "")
    set (CMAKE_OSX_DEPLOYMENT_TARGET 13.0
      CACHE STRING "Default minimum macOS version for Qt 6 builds.

Override explicitly in CI / packaging jobs when targeting Intel Monterey,
Intel Sequoia or Apple Silicon release lines.")
  else ()
    set (CMAKE_OSX_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}"
      CACHE STRING "Default minimum macOS version for Qt 6 builds.

Override explicitly in CI / packaging jobs when targeting Intel Monterey,
Intel Sequoia or Apple Silicon release lines." FORCE)
  endif ()
endif (APPLE)

#
# CMake policies
#
if (POLICY CMP0020)
  cmake_policy (SET CMP0020 NEW) # link to Qt winmain on Windows
endif ()

if (POLICY CMP0043)
  cmake_policy (SET CMP0043 NEW) # ignore COMPILE_DEFINITIONS_<CONFIG>
endif ()

if (POLICY CMP0048)
  cmake_policy (SET CMP0048 NEW) # clear PROJECT_Version_* variables if not set in project() command
endif ()

if (POLICY CMP0063)
  cmake_policy (SET CMP0063 NEW) # honour visibility properties for all library types
endif ()

if (POLICY CMP0071)
  cmake_policy (SET CMP0071 NEW) # run automoc and autouic on generated sources
endif ()

if (POLICY CMP0079)
  cmake_policy (SET CMP0079 NEW) # allow target_link_libraries across directories
endif ()

if (POLICY CMP0075)
  cmake_policy (SET CMP0075 NEW) # honour CMAKE_REQUIRED_LIBRARIES in config checks
endif ()

project (wsjtx
  VERSION 4.0.0.0
  LANGUAGES C CXX
  )
set (PROJECT_DESCRIPTION "Decodium 4.0 Core Gallager: Digital Modes for Weak Signal Communications in Amateur Radio")
set (CMAKE_PROJECT_DESCRIPTION ${PROJECT_DESCRIPTION})
set (CMAKE_AUTOUIC ON)
if (APPLE)
  set (WSJT_APP_BUNDLE_NAME "Decodium4")
  set (WSJT_APP_EXECUTABLE_NAME "Decodium4")
  set (WSJT_APP_DISPLAY_NAME "Decodium4")
  set (WSJT_APP_GUI_IDENTIFIER "org.decodium.decodium4")
else ()
  set (WSJT_APP_BUNDLE_NAME "ft2")
  set (WSJT_APP_EXECUTABLE_NAME "ft2")
  set (WSJT_APP_DISPLAY_NAME "ft2")
  set (WSJT_APP_GUI_IDENTIFIER "org.decodium.ft2")
endif ()
include (CTest)

#
# Local CMake modules and support files
#
set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake/Modules ${CMAKE_MODULE_PATH})

set (DECODIUM_RUNTIME_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources/runtime")
set (DECODIUM_CMAKE_TEMPLATE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/CMake/templates")
set (DECODIUM_PACKAGING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/packaging")

set (DECODIUM_REQUIRED_RUNTIME_FILES
  ALLCALL7.TXT
  CALL3.TXT
  cty.dat
  cty.dat_copyright.txt
  eclipse.txt
  grid.dat
  sat.dat
  )
foreach (required_runtime_file IN LISTS DECODIUM_REQUIRED_RUNTIME_FILES)
  if (NOT EXISTS "${DECODIUM_RUNTIME_DATA_DIR}/${required_runtime_file}")
    message (FATAL_ERROR
      "Required runtime data file is missing: "
      "${DECODIUM_RUNTIME_DATA_DIR}/${required_runtime_file}")
  endif ()
endforeach ()

set (PROJECT_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
if (NOT PROJECT_ARCHITECTURE)
  # This is supposed to happen already on Windows
  if (CMAKE_SIZEOF_VOID_P MATCHES 8)
    set (PROJECT_ARCHITECTURE "x64")
  else ()
    set (PROJECT_ARCHITECTURE "$ENV{PROCESSOR_ARCHITECTURE}")
  endif ()
endif ()
message (STATUS "******************************************************")
message (STATUS "Building for for: ${CMAKE_SYSTEM_NAME}-${PROJECT_ARCHITECTURE}")
message (STATUS "******************************************************")

include (set_build_type)
# RC 0 or omitted is a development build, GA is a General Availability release build
set_build_type (RC 1)
set_property (DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/fork_release_version.txt")
file (READ "${CMAKE_CURRENT_SOURCE_DIR}/fork_release_version.txt" FORK_RELEASE_VERSION)
string (STRIP "${FORK_RELEASE_VERSION}" FORK_RELEASE_VERSION)
if (NOT FORK_RELEASE_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
  message (FATAL_ERROR "fork_release_version.txt must contain a semantic version in the form MAJOR.MINOR.PATCH")
endif ()
set (FORK_RELEASE_VERSION_MAJOR "${CMAKE_MATCH_1}")
set (FORK_RELEASE_VERSION_MINOR "${CMAKE_MATCH_2}")
set (FORK_RELEASE_VERSION_PATCH "${CMAKE_MATCH_3}")
set (wsjtx_VERSION "${FORK_RELEASE_VERSION}")
set (FORK_RELEASE_VERSION "${FORK_RELEASE_VERSION}" CACHE STRING "Fork release version shown in UI title" FORCE)
add_definitions (-DFORK_RELEASE_VERSION=\"${FORK_RELEASE_VERSION}\")

#
# project information
#
set (PROJECT_BUNDLE_NAME "Decodium 4.0 Core Gallager")
set (PROJECT_VENDOR "IU8LMC")
set (PROJECT_CONTACT "IU8LMC <qrz.com/db/IU8LMC>")
set (PROJECT_COPYRIGHT "See Help | Copyright info")
set (PROJECT_HOMEPAGE qrz.com/db/IU8LMC)
set (PROJECT_MANUAL wsjtx-main)
set (PROJECT_MANUAL_DIRECTORY_URL https://wsjt.sourceforge.io/wsjtx-doc/)
set (PROJECT_SAMPLES_URL http://downloads.sourceforge.net/project/wsjt/)
set (PROJECT_SAMPLES_UPLOAD_DEST frs.sourceforge.net:/home/frs/project/wsjt/)

# make sure that the default configuration is MinSizeRel (-Os)
if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE MinSizeRel CACHE STRING
    "Choose the type of build, options are: None Debug Release MinSizeRel."
    FORCE)
endif (NOT CMAKE_BUILD_TYPE)
if (CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
  set (is_debug_build 1)
endif ()

# Ottimizzazione dimensione: strip + gc-sections
if (NOT is_debug_build)
  if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections")
  endif ()
  if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdata-sections -ffunction-sections")
  endif ()
  if (NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # DECODIUM_KEEP_SYMBOLS=ON conserva la tabella dei simboli: serve a gdb e
    # a Dr. Memory per risolvere gli stack. Spenta per default: i binari
    # pubblicati restano strippati come prima.
    if (DECODIUM_KEEP_SYMBOLS)
      set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
    else ()
      set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections -Wl,-s -Wl,--strip-all")
    endif ()
  endif ()
endif ()


#
# Options & features
#
#	Some of these directly effect compilation by being defined in
#	wsjtx_config.h.in which makes them available to the C/C++
#	pre-processor.
#
include (CMakeDependentOption)

# Allow the developer to select if Dynamic or Static libraries are built
OPTION (BUILD_SHARED_LIBS "Build Shared Libraries" OFF)
# Set the LIB_TYPE variable to STATIC
SET (LIB_TYPE STATIC)
if (BUILD_SHARED_LIBS)
  # User wants to build Dynamic Libraries, so change the LIB_TYPE variable to CMake keyword 'SHARED'
  set (LIB_TYPE SHARED)
endif (BUILD_SHARED_LIBS)

option (UPDATE_TRANSLATIONS "Update source translation translations/*.ts
files (WARNING: make clean will delete the source .ts files! Danger!)")
option (WSJT_SHARED_RUNTIME "Debugging option that allows running from a shared Cloud directory.")
option (WSJT_QDEBUG_TO_FILE "Redirect Qt debuging messages to a trace file.")
option (WSJT_TRACE_CAT "Debugging option that turns on CAT diagnostics.")
option (WSJT_TRACE_CAT_POLLS "Debugging option that turns on CAT diagnostics during polling.")
option (WSJT_SOFT_KEYING "Apply a ramp to CW keying envelope to reduce transients." ON)
option (WSJT_SKIP_MANPAGES "Skip *nix manpage generation.")
option (WSJT_GENERATE_DOCS "Generate documentation files." ON)
option (WSJT_RIG_NONE_CAN_SPLIT "Allow split operation with \"None\" as rig.")
option (WSJT_TRACE_UDP "Debugging option that turns on UDP message protocol diagnostics.")
option (WSJT_BUILD_UTILS "Build simulators and code demonstrators." OFF)
option (WSJT_FOX_OTP "Enable Fox OTP Verification Messages." ON)
option (DECODIUM_ENABLE_SSTV
  "Build the native analog SSTV subsystem inside Decodium" ON)
option (DECODIUM_ENABLE_HAMDRM
  "Build the native HAMDRM digital SSTV subsystem inside Decodium" ON)
if (DECODIUM_ENABLE_HAMDRM AND NOT DECODIUM_ENABLE_SSTV)
  message (FATAL_ERROR
    "DECODIUM_ENABLE_HAMDRM requires DECODIUM_ENABLE_SSTV=ON; "
    "the analog target remains independent when HAMDRM is disabled")
endif ()
# DecoRTTY innestato: RTTY sulla radio di Decodium. Vedi
# doc/PIANO_INTEGRAZIONE_DECORTTY.md.
#
# Il trasporto di rete del progetto originale — VITA-49 verso i FlexRadio, e il
# gateway per una FT-991A su un altro PC — e' stato tolto: qui la radio e'
# quella che il CAT dell'applicazione governa gia', e cercarla in rete non
# poteva che non trovarla. Con quel trasporto se n'e' andata anche Opus, che
# serviva solo a comprimere l'audio sul filo.
option (DECODIUM_ENABLE_RTTY
  "Build the DecoRTTY subsystem (RTTY on Decodium's own radio) inside Decodium" ON)
set (DECORTTY_VERSION "0.7.2" CACHE STRING
     "Versione DecoRTTY, scritta nell'intestazione degli ADIF esportati")
option (DECODIUM_ENABLE_SSTV_FUZZING
  "Build Clang/libFuzzer SSTV parser targets (developer-only)" OFF)
if (DECODIUM_ENABLE_SSTV_FUZZING)
  if (NOT DECODIUM_ENABLE_SSTV)
    message (FATAL_ERROR
      "DECODIUM_ENABLE_SSTV_FUZZING requires DECODIUM_ENABLE_SSTV=ON")
  endif ()
  if (NOT BUILD_TESTING)
    message (FATAL_ERROR
      "DECODIUM_ENABLE_SSTV_FUZZING requires BUILD_TESTING=ON")
  endif ()
  if (NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|AppleClang)$")
    message (FATAL_ERROR
      "DECODIUM_ENABLE_SSTV_FUZZING requires a Clang-family C++ compiler")
  endif ()

  include (CheckCXXSourceCompiles)
  set (_decodium_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
  set (CMAKE_REQUIRED_FLAGS
    "${CMAKE_REQUIRED_FLAGS} -fsanitize=fuzzer,address,undefined")
  unset (DECODIUM_SSTV_LIBFUZZER_AVAILABLE CACHE)
  check_cxx_source_compiles (
    "#include <cstddef>
     #include <cstdint>
     extern \"C\" int LLVMFuzzerTestOneInput(const std::uint8_t*, std::size_t)
     { return 0; }"
    DECODIUM_SSTV_LIBFUZZER_AVAILABLE)
  set (CMAKE_REQUIRED_FLAGS "${_decodium_saved_required_flags}")
  unset (_decodium_saved_required_flags)
  if (NOT DECODIUM_SSTV_LIBFUZZER_AVAILABLE)
    message (FATAL_ERROR
      "DECODIUM_ENABLE_SSTV_FUZZING needs Clang with the libFuzzer, "
      "AddressSanitizer and UndefinedBehaviorSanitizer runtimes")
  endif ()
endif ()
CMAKE_DEPENDENT_OPTION (WSJT_QDEBUG_IN_RELEASE "Leave Qt debugging statements in Release configuration." OFF
  "NOT is_debug_build" OFF)
CMAKE_DEPENDENT_OPTION (WSJT_ENABLE_EXPERIMENTAL_FEATURES "Enable features not fully ready for public releases." ON
  is_debug_build OFF)
CMAKE_DEPENDENT_OPTION (WSJT_CREATE_WINMAIN
  "The wsjtx target is normally built as GUI executable with a WinMain entry point on Windows,
if you want a console application instead then set this option to OFF.

If you just want to see the debug output from the application then the easiest way is to
attach a debugger which will then receive the console output inside its console." ON
  "WIN32" OFF)

#
# install locations
#

if (APPLE)
  set (CMAKE_INSTALL_BINDIR ${WSJT_APP_BUNDLE_NAME}.app/Contents/MacOS)
  set (CMAKE_INSTALL_DATAROOTDIR ${WSJT_APP_BUNDLE_NAME}.app/Contents/Resources)
  # Clean up stale bundle artifacts from older macOS target layouts.
  file (REMOVE_RECURSE
    "${CMAKE_BINARY_DIR}/CMakeFiles/Decodium4.dir"
    "${CMAKE_BINARY_DIR}/CMakeFiles/Decodium4_autogen.dir"
    "${CMAKE_BINARY_DIR}/CMakeFiles/Decodium4_autogen_timestamp_deps.dir"
    "${CMAKE_BINARY_DIR}/CMakeFiles/ft2.dir"
    "${CMAKE_BINARY_DIR}/CMakeFiles/ft2_autogen.dir"
    "${CMAKE_BINARY_DIR}/CMakeFiles/ft2_autogen_timestamp_deps.dir"
    "${CMAKE_BINARY_DIR}/ft2.app"
    "${CMAKE_BINARY_DIR}/${WSJT_APP_BUNDLE_NAME}.app/Contents/MacOS/sounds")
endif ()

include (GNUInstallDirs)

set (PLUGIN_DESTINATION ${CMAKE_INSTALL_LIBDIR}/plugins)
set (QT_CONF_DESTINATION ${CMAKE_INSTALL_BINDIR})
if (WIN32)
  set (PLUGIN_DESTINATION plugins)
elseif (APPLE)
  set (PLUGIN_DESTINATION ${CMAKE_INSTALL_BINDIR}/../PlugIns)
  set (QT_CONF_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR})
endif ()

if (APPLE)
  set (WSJT_PLUGIN_DESTINATION ${PLUGIN_DESTINATION} CACHE PATH "Path for plugins" FORCE)
  set (WSJT_QT_CONF_DESTINATION ${QT_CONF_DESTINATION} CACHE PATH "Path for the qt.conf file" FORCE)
else ()
  set (WSJT_PLUGIN_DESTINATION ${PLUGIN_DESTINATION} CACHE PATH "Path for plugins")
  set (WSJT_QT_CONF_DESTINATION ${QT_CONF_DESTINATION} CACHE PATH "Path for the qt.conf file")
endif ()

if (WSJT_FOX_OTP)
  set (wsjt_fox_CXXSRCS
  )
  message (STATUS "Including Fox verification code feature")
endif ()

#
# Project sources
#
set (DECODIUM_SOURCE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bridge
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/models
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net
  ${CMAKE_CURRENT_SOURCE_DIR}/src/radio
  ${CMAKE_CURRENT_SOURCE_DIR}/src/security
  ${CMAKE_CURRENT_SOURCE_DIR}/src/services
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
  )
include_directories (${DECODIUM_SOURCE_DIRS})

set (fort_qt_CXXSRCS
  lib/shmem.cpp
  )

set (wsjt_qt_CXXSRCS
  src/core/helper_functions.cpp
  src/security/SecureSettings.cpp
  src/core/qt_helpers.cpp
  widgets/MessageBox.cpp
  src/app/MetaDataRegistry.cpp
  Network/NetworkServerLookup.cpp
  src/core/revision_utils.cpp
  src/app/L10nLoader.cpp
  src/ui/WFPalette.cpp
  src/radio/Radio.cpp
  src/radio/RadioMetaType.cpp
  src/core/NonInheritingProcess.cpp
  models/IARURegions.cpp
  models/Bands.cpp
  models/Modes.cpp
  models/FrequencyList.cpp
  models/StationList.cpp
  widgets/FrequencyLineEdit.cpp
  widgets/FrequencyDeltaLineEdit.cpp
  item_delegates/CandidateKeyFilter.cpp
  item_delegates/ForeignKeyDelegate.cpp
  item_delegates/MessageItemDelegate.cpp
  validators/LiveFrequencyValidator.cpp
  src/app/GetUserId.cpp
  Audio/AudioDevice.cpp
  Transceiver/Transceiver.cpp
  Transceiver/TransceiverBase.cpp
  Transceiver/EmulateSplitTransceiver.cpp
  Transceiver/TransceiverFactory.cpp
  Transceiver/PollingTransceiver.cpp
  Transceiver/HamlibTransceiver.cpp
  Transceiver/TCITransceiver.cpp
  Transceiver/HRDTransceiver.cpp
  Transceiver/DXLabSuiteCommanderTransceiver.cpp
  Network/NetworkMessage.cpp
  Network/MessageClient.cpp
  Network/RemoteCommandServer.cpp
  widgets/LettersSpinBox.cpp
  widgets/HintedSpinBox.cpp
  widgets/RestrictedSpinBox.cpp
  widgets/HelpTextWindow.cpp
  src/services/SampleDownloader.cpp
  SampleDownloader/DirectoryDelegate.cpp
  SampleDownloader/Directory.cpp
  SampleDownloader/FileNode.cpp
  SampleDownloader/RemoteFile.cpp
  src/ui/DisplayManual.cpp
  src/app/MultiSettings.cpp
  validators/MaidenheadLocatorValidator.cpp
  validators/CallsignValidator.cpp
  widgets/SplashScreen.cpp
  src/ui/EqualizationToolsDialog.cpp
  Detector/FtxFt8Stage4.cpp
  Detector/FST4DecodeWorker.cpp
  widgets/DoubleClickablePushButton.cpp
  widgets/DoubleClickableRadioButton.cpp
  Network/LotWUsers.cpp
  Network/FileDownload.cpp
  Network/Cloudlog.cpp
  models/DecodeHighlightingModel.cpp
  widgets/DecodeHighlightingListView.cpp
  models/FoxLog.cpp
  widgets/AbstractLogWindow.cpp
  widgets/FoxLogWindow.cpp
  widgets/CabrilloLogWindow.cpp
  item_delegates/CallsignDelegate.cpp
  item_delegates/MaidenheadLocatorDelegate.cpp
  item_delegates/FrequencyDelegate.cpp
  item_delegates/FrequencyDeltaDelegate.cpp
  item_delegates/SQLiteDateTimeDelegate.cpp
  models/CabrilloLog.cpp
  logbook/AD1CCty.cpp
  logbook/WorkedBefore.cpp
  logbook/Multiplier.cpp
  Network/NetworkAccessManager.cpp
  Network/NtpClient.cpp
  Network/HttpsTimeSource.cpp
  Network/DecoSyncKalman.cpp
  Network/DecoSyncSelfCal.cpp
  Network/DecoSyncTime.cpp
  widgets/LazyFillComboBox.cpp
  widgets/CheckableItemComboBox.cpp
  widgets/BandComboBox.cpp
  Detector/FtxFt2Stage7.cpp
  Detector/fastldpc/decodium_dispatch.cpp
  Detector/FtxQ65Core.cpp
  Detector/FtxQ65Decoder.cpp
  Detector/FtxQ65Frontend.cpp
  Detector/FtxMap65Q65.cpp
  Detector/FtxQ65Support.cpp
  Detector/LegacyDspIoHelpers.cpp
  Detector/JT65Decoder.cpp
  Detector/JT9FastDecoder.cpp
  Detector/JT9FastHelpers.cpp
  Detector/JT9WideDecoder.cpp
  Detector/FftCompat.cpp
  Detector/FanoSequentialDecoder.cpp
  Detector/JT4Decoder.cpp
  Detector/JT9NarrowDecoder.cpp
  Modulator/FtxWaveformGenerator.cpp
  Modulator/FtxMessageEncoder.cpp
  widgets/LegacyUiHelpers.cpp
  widgets/PlotLegacyHelpers.cpp
  Detector/AstroCompat.cpp
  Detector/AstroQ3ListCompat.cpp
  Detector/Conv232Compat.cpp
  Detector/IndexSortCompat.cpp
  Modulator/WsprLegacyCompat.cpp
  src/core/otpgenerator.cpp
  )

# fastldpc: dispatcher e fallback restano sempre compilati per la CPU minima.
# Il backend min-sum e' una translation unit separata: AVX2/FMA su x86 e NEON
# su ARM64. Il dispatcher entra nel backend soltanto dopo il controllo runtime
# delle capacita' richieste dall'architettura.
set (DECODIUM_FASTLDPC_X86_TARGET OFF)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86)$"
    OR CMAKE_OSX_ARCHITECTURES MATCHES "(^|;)x86_64(;|$)")
  set (DECODIUM_FASTLDPC_X86_TARGET ON)
endif ()

set (DECODIUM_FASTLDPC_ARM64_TARGET OFF)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64|AARCH64)$"
    OR CMAKE_OSX_ARCHITECTURES MATCHES "(^|;)(arm64|aarch64)(;|$)")
  set (DECODIUM_FASTLDPC_ARM64_TARGET ON)
endif ()

# I runner di release producono slice architetturali distinte. Se viene
# richiesto eccezionalmente un universal binary, privilegiamo il backend NEON:
# la slice x86 resta comunque sicura e usa il fallback generico.
if (DECODIUM_FASTLDPC_ARM64_TARGET)
  list (APPEND wsjt_qt_CXXSRCS Detector/fastldpc/decodium_bridge.cpp)
  set_source_files_properties (Detector/fastldpc/decodium_dispatch.cpp
    PROPERTIES COMPILE_DEFINITIONS "DECODIUM_FASTLDPC_NEON_BUILT=1")
  # FASTLDPC_HAVE_GATE_WEIGHTS: strato 2 (gate appreso, FASTLDPC-AI-SPEC-001
  # §2). Senza questa macro gate.hpp compila comunque (gate_accept() sempre
  # true) ma DECODIUM_LDPC_GATE=1 diventerebbe silenziosamente solo una soglia
  # nd_max rilassata, senza classificatore a filtrarla: pericoloso. La macro
  # carica i pesi; il flag a runtime resta comunque spento di default.
  set_source_files_properties (Detector/fastldpc/decodium_bridge.cpp
    PROPERTIES COMPILE_DEFINITIONS "FASTLDPC_HAVE_GATE_WEIGHTS=1")
  if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties (Detector/fastldpc/decodium_bridge.cpp
      PROPERTIES COMPILE_OPTIONS "-O3")
  elseif (MSVC)
    set_source_files_properties (Detector/fastldpc/decodium_bridge.cpp
      PROPERTIES COMPILE_OPTIONS "/O2")
  endif ()
elseif (DECODIUM_FASTLDPC_X86_TARGET)
  list (APPEND wsjt_qt_CXXSRCS Detector/fastldpc/decodium_bridge.cpp)
  set_source_files_properties (Detector/fastldpc/decodium_dispatch.cpp
    PROPERTIES COMPILE_DEFINITIONS "DECODIUM_FASTLDPC_AVX2_BUILT=1")
  # FASTLDPC_HAVE_GATE_WEIGHTS: vedi il commento nel ramo ARM64 sopra.
  set_source_files_properties (Detector/fastldpc/decodium_bridge.cpp
    PROPERTIES COMPILE_DEFINITIONS "FASTLDPC_HAVE_GATE_WEIGHTS=1")
  if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties (Detector/fastldpc/decodium_bridge.cpp
      PROPERTIES COMPILE_OPTIONS "-O3;-mavx2;-mfma")
  elseif (MSVC)
    set_source_files_properties (Detector/fastldpc/decodium_bridge.cpp
      PROPERTIES COMPILE_OPTIONS "/O2;/arch:AVX2")
  endif ()
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties (Detector/LegacyDspIoHelpers.cpp PROPERTIES COMPILE_FLAGS "-Wno-error=stringop-overflow -Wno-error=stringop-overread")
  # GCC 15 misdiagnoses std::atomic increments in inlined Qt functor wrappers.
  set_source_files_properties (src/services/MapIntelligenceService.cpp PROPERTIES COMPILE_FLAGS "-Wno-error=stringop-overflow")
endif ()

set (wsjt_qtmm_CXXSRCS
  Audio/BWFFile.cpp
  )

set (wsjtx_CXXSRCS
  src/bridge/DecodiumLogging.cpp
  logbook/logbook.cpp
  Network/PSKReporter.cpp
  Modulator/Modulator.cpp
  Detector/Detector.cpp
  Detector/FT2DecodeWorker.cpp
  Detector/FT4DecodeWorker.cpp
  Detector/FT8DecodeWorker.cpp
  Detector/JT9FastDecodeWorker.cpp
  Detector/LegacyJtDecodeWorker.cpp
  Detector/MSK144DecodeWorker.cpp
  Detector/Q65DecodeWorker.cpp
  Detector/WSPRDecodeWorker.cpp
  lib/wsprd/wsprd_embedded.c
  lib/wsprd/osdwspr.c
  lib/wsprd/wsprsim_utils.c
  lib/wsprd/wsprd_utils.c
  lib/wsprd/jelinek.c
  widgets/logqso.cpp
  widgets/displaytext.cpp
  Decoder/decodedtext.cpp
  src/core/getfile.cpp
  Audio/soundout.cpp
  Audio/soundin.cpp
  src/rtl/RtlSdrDsp.cpp
  src/rtl/RtlSdrDsp.h
  src/rtl/RtlSdrAudioOutput.cpp
  src/rtl/RtlSdrAudioOutput.h
  src/rtl/RtlSdrCapabilities.h
  src/rtl/RtlSdrRfSpectrum.cpp
  src/rtl/RtlSdrRfSpectrum.h
  src/rtl/RtlSdrTuningPlan.h
  src/rtl/RtlSdrInput.cpp
  src/rtl/RtlSdrInput.h
  widgets/meterwidget.cpp
  widgets/signalmeter.cpp
  widgets/plotter.cpp
  widgets/widegraph.cpp
  widgets/echograph.cpp
  widgets/echoplot.cpp
  widgets/fastgraph.cpp
  widgets/fastplot.cpp
  widgets/about.cpp
  widgets/asyncmodewidget.cpp
  widgets/FT2QsoFlowPolicy.cpp
  widgets/astro.cpp
  widgets/messageaveraging.cpp
  widgets/activeStations.cpp
  widgets/worldmapwidget.cpp
  widgets/colorhighlighting.cpp
  widgets/RTTYTerminalWidget.cpp
  Modulator/RTTYModulator.cpp
  Detector/RTTYDetector.cpp
  Decoder/BaudotDecoder.cpp
  src/security/DecodiumCertificate.cpp
  WSPR/WsprTxScheduler.cpp
  widgets/mainwindow.cpp
  src/app/Configuration.cpp
  src/app/main.cpp
  src/net/DecoPortPacket.cpp
  src/net/DecoPortPacket.h
  src/net/RadioLink.h
  src/net/DecoPortLink.cpp
  src/net/DecoPortLink.h
  src/net/DecodiumDecoPortGateway.cpp
  src/net/DecoPortRigDriver.cpp
  src/net/DecodiumDecoPortGateway.h
  src/bridge/DecodiumBridge.cpp
  src/bridge/AdifExportSanitizer.cpp
  src/bridge/AdifExportSanitizer.h
  src/bridge/DecodiumBridgeSstv.cpp
  src/bridge/DecodiumBridge.h
  Sequencer/QsoSequencerRules.cpp
  Sequencer/QsoSequencerRules.hpp
  Sequencer/MessageTokenRules.cpp
  Sequencer/MessageTokenRules.hpp
  Sequencer/QsoSequencerState.hpp
  Sequencer/ISequencerSink.hpp
  Sequencer/QsoSequencer.hpp
  src/app/DecodiumSelfCheck.cpp
  src/app/DecodiumSelfCheck.hpp
  src/services/UsStateDataManager.cpp
  src/services/UsStateDataManager.h
  lib/persistence/DecodeHistoryWorker.cpp
  lib/persistence/DecodeHistoryWorker.h
  src/models/DecodeListModel.cpp
  src/models/DecodeListModel.h
  controllers/FT2LinkQmlAdapter.cpp
  controllers/FT2LinkQmlAdapter.hpp
  src/bridge/DecodiumDiagnostics.cpp
  src/bridge/DecodiumDiagnostics.h
  src/bridge/DecodiumLegacyBackend.cpp
  src/bridge/DecodiumLegacyBackend.h
  src/services/DecodiumPropagationManager.cpp
  src/services/DecodiumPropagationManager.h
  src/services/RotatorService.cpp
  src/services/RotatorService.h
  src/services/SatelliteTrackingService.cpp
  src/services/SatelliteTrackingService.h
  src/services/satellite/sgp4/SGP4.cpp
  src/services/satellite/sgp4/SGP4.h
  src/services/MapIntelligenceService.cpp
  src/services/MapIntelligenceService.h
  src/services/MapBaseMapService.cpp
  src/services/MapBaseMapService.h
  src/services/MapPskFeedService.cpp
  src/services/MapPskFeedService.h
  src/services/MapOperationsService.cpp
  src/services/MapOperationsService.h
  src/services/MapExternalOverlayService.cpp
  src/services/MapExternalOverlayService.h
  src/services/MapLayerModel.cpp
  src/services/MapLayerModel.h
  src/bridge/DecodiumAudioSink.h
  controllers/ActiveStationsModel.cpp
  controllers/ActiveStationsModel.hpp
  src/ui/DecodiumThemeManager.cpp
  src/ui/DecodiumThemeManager.h
  src/bridge/DecodiumSubManagers.cpp
  src/bridge/DecodiumSubManagers.h
  src/radio/DecodiumCatManager.cpp
  src/radio/DecodiumCatManager.h
  src/radio/DecodiumOmniRigManager.cpp
  src/radio/DecodiumOmniRigManager.h
  src/radio/DecodiumCat4OmManager.cpp
  src/radio/DecodiumCat4OmManager.h
  src/radio/DecodiumTransceiverManager.cpp
  src/radio/DecodiumTransceiverManager.h
  src/radio/DecodiumCatShare.cpp
  src/radio/DecodiumCatShare.h
  src/radio/DecodiumAmplifier.cpp
  src/radio/StationTelemetryCodec.cpp
  src/radio/DecodiumAmplifier.h
  src/radio/DecodiumRigDetector.cpp
  src/radio/DecodiumRigDetector.h
  src/services/DecodiumAlertManager.cpp
  src/services/DecodiumAlertManager.h
  Network/FoxVerifier.cpp
  Network/FoxVerifier.hpp
  Network/DecodiumWsprUploader.cpp
  Network/DecodiumWsprUploader.h
  Network/DecodiumPskReporterLite.cpp
  Network/DecodiumPskReporterLite.h
  Network/DecodiumCloudlogLite.cpp
  Network/DecodiumCloudlogLite.h
  Network/DecodiumQrzLogbookLite.cpp
  Network/DecodiumQrzLogbookLite.h
  src/services/DecodiumDxCluster.cpp
  src/services/DecodiumDxCluster.h
  src/services/DecodiumSpotShare.cpp
  src/services/DecodiumSpotShare.h
  src/services/DxccLookup.cpp
  src/services/DxccLookup.h
  src/services/CallsignIntelligenceService.cpp
  src/services/CallsignIntelligenceService.h
  src/bridge/DecodiumLegacyLinkCompat.cpp
  Network/wsprnet.cpp
  WSPR/WSPRBandHopping.cpp
  widgets/ExportCabrillo.cpp
  widgets/QSYMessage.cpp 
  widgets/QSYMessageCreator.cpp
  widgets/qsymonitor.cpp
  widgets/TimeSyncPanel.cpp
  widgets/IonosphericForecastWindow.cpp
  widgets/DXClusterWindow.cpp
  )

if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  set_source_files_properties (
    widgets/plotter.cpp 
    widgets/mainwindow.cpp
    PROPERTIES COMPILE_FLAGS "-Wno-vla-extension")
endif ()

set (wsjt_CXXSRCS
  src/core/Logger.cpp
  lib/crc10.cpp
  lib/crc13.cpp
  lib/crc14.cpp
  Modulator/WattersonChannel.cpp
  Detector/Fil4Filter.cpp
  # PctileCompat: pctile_ already defined in FtxSharedDsp.cpp
  Detector/FtxBaseline.cpp
  Detector/FtxCandidates.cpp
  Detector/FtxBitmetrics.cpp
  Detector/FtxApStorico.cpp
  Detector/FtxSharedDsp.cpp
  Detector/FtxFst4Core.cpp
  Detector/FtxFst4Ldpc.cpp
  Detector/FtxDecodeBookkeeping.cpp
  Detector/FtxFt4Decoder.cpp
  Detector/FtxFt4Rollout.cpp
  Detector/FtxFt8Chkgrid.cpp
  Detector/FtxLdpc.cpp
  Detector/FtxDownsample.cpp
  Detector/FtxFt8Search.cpp
  Detector/FtxSubtract.cpp
  Detector/SfoxFt8Remove.cpp
  Detector/FtxSync.cpp
  Detector/FtxTweak.cpp
  Detector/FtxVarSyncTemplates.cpp
  ${wsjt_fox_CXXSRCS}
  )

set (FT2LINK_CORE_CXXSRCS
  lib/ft2link/FT2LinkAppModel.cpp
  lib/ft2link/FT2LinkAudio.cpp
  lib/ft2link/FT2LinkFrame.cpp
  lib/ft2link/FT2LinkHandshake.cpp
  lib/ft2link/FT2LinkOffline.cpp
  lib/ft2link/FT2LinkPhysical.cpp
  lib/ft2link/FT2LinkSession.cpp
  lib/ft2link/FT2LinkWaveform.cpp
  )

# deal with a GCC v6 UB error message
set_source_files_properties (
  lib/crc10.cpp
  lib/crc13.cpp
  lib/crc14.cpp
  PROPERTIES COMPILE_FLAGS -fpermissive)

# Windows-specific helper sources intentionally excluded in this fork.

# wsjt_FSRCS is intentionally empty: the maintained application path follows
# Decodium 3 and resolves the migrated Fortran functionality through native C++.
# Historical Fortran sources stay in-tree only as dormant archives / utility-only
# material until a larger repo prune is done.
set (wsjt_FSRCS)

set (ka9q_CSRCS
  lib/ftrsd/decode_rs.c
  lib/ftrsd/encode_rs.c
  lib/ftrsd/init_rs.c
  )
set_source_files_properties (${ka9q_CSRCS} PROPERTIES COMPILE_FLAGS -Wno-sign-compare)

set (qra_CSRCS
  lib/qra/qracodes/qra12_63_64_irr_b.c
  lib/qra/qracodes/qra13_64_64_irr_e.c
  lib/qra/q65/npfwht.c
  lib/qra/q65/pdmath.c
  lib/qra/q65/qracodes.c
  lib/qra/q65/normrnd.c
  lib/qra/q65/qra15_65_64_irr_e23.c
  lib/qra/q65/q65.c
  )

set (wsjt_CSRCS
  ${ka9q_CSRCS}
  lib/db.c
  lib/graycode.c
  lib/hash.c
  lib/move.c
  lib/nuttal_window.c
  lib/peakup.c
  lib/averms.c
  lib/azdist.c
  lib/c0com.c
  lib/chkcall.c
  lib/coord.c
  lib/degrade_snr.c
  lib/geodist.c
  lib/morse.c
  lib/genwave.c
  lib/getmet4.c
  lib/chkhist.c
  lib/chkmsg.c
  lib/chkss2.c
  lib/deg2grid.c
  lib/entail.c
  lib/flat1.c
  lib/fmtmsg.c
  lib/grid2deg.c
  lib/interleave4.c
  lib/interleave63.c
  lib/interleave9.c
  lib/getlags.c
  lib/grayline.c
  lib/twkfreq.c
  lib/platanh.c
  lib/plotsave.c
  lib/sec0.c
  lib/sleep_msec.c
  lib/slope.c
  lib/smo.c
  lib/ftrsd/ftrsdap.c
  lib/sgran.c
  lib/golay24_table.c
  lib/gran.c
  lib/igray.c
  lib/init_random_seed.c
  lib/ldpc32_table.c
  lib/wsprd/nhash.c
  lib/wsprd/fano.c
  lib/wsprd/tab.c
  lib/wsprd/osdwspr.c
  lib/tab.c
  lib/tmoonsub.c
  lib/usleep.c
  lib/vit213.c
  lib/wisdom.c
  lib/wrapkarn.c
  ${ldpc_CSRCS}
  ${qra_CSRCS}

  lib/superfox/qpc/dbgprintf.c
  lib/superfox/qpc/nhash2.c
  lib/superfox/qpc/np_qpc.c
  lib/superfox/qpc/np_rnd.c
  lib/superfox/qpc/qpc_fwht.c
  lib/superfox/qpc/qpc_n127k50q128.c
  lib/superfox/qpc/qpc_subs.c

  )

set (wsjt_qt_UISRCS
  src/ui/wf_palette_design_dialog.ui
  widgets/FoxLogWindow.ui
  widgets/CabrilloLogWindow.ui
  )

set (wsprsim_CSRCS
  lib/wsprd/wsprsim.c
  lib/wsprd/wsprsim_utils.c
  lib/wsprd/wsprd_utils.c
  lib/wsprd/fano.c
  lib/wsprd/tab.c
  lib/wsprd/nhash.c
  )

set (wsprd_CSRCS
  lib/wsprd/wsprd.c
  lib/wsprd/osdwspr.c
  lib/wsprd/wsprsim_utils.c
  lib/wsprd/wsprd_utils.c
  lib/wsprd/fano.c
  lib/wsprd/jelinek.c
  lib/wsprd/tab.c
  lib/wsprd/nhash.c
  lib/init_random_seed.c
  )

set (wsjtx_UISRCS
  widgets/mainwindow.ui
  widgets/about.ui
  widgets/astro.ui
  widgets/colorhighlighting.ui
  widgets/echograph.ui
  widgets/fastgraph.ui
  widgets/messageaveraging.ui
  widgets/activeStations.ui
  widgets/widegraph.ui
  widgets/logqso.ui
  src/app/Configuration.ui
  widgets/ExportCabrillo.ui
  widgets/QSYMessage.ui
  widgets/QSYMessageCreator.ui
  widgets/qsymonitor.ui
  widgets/TimeSyncPanel.ui
  )

set (UDP_library_CXXSRCS
  src/radio/Radio.cpp
  src/radio/RadioMetaType.cpp
  Network/NetworkMessage.cpp
  UDPExamples/MessageServer.cpp
  )

set (UDP_library_HEADERS
  src/radio/Radio.hpp
  UDPExamples/MessageServer.hpp
  ${PROJECT_BINARY_DIR}/udp_export.h
  )

set (message_aggregator_CXXSRCS
  UDPExamples/MessageAggregator.cpp
  UDPExamples/MessageAggregatorMainWindow.cpp
  UDPExamples/DecodesModel.cpp
  UDPExamples/BeaconsModel.cpp
  UDPExamples/ClientWidget.cpp
  validators/MaidenheadLocatorValidator.cpp
  )

set (message_aggregator_STYLESHEETS
  UDPExamples/qss/default.qss
  )

set (qcp_CXXSRCS
  qcustomplot-source/qcustomplot.cpp
  )

set (all_CXXSRCS
  ${wsjt_CXXSRCS}
  ${fort_qt_CXXSRCS}
  ${wsjt_qt_CXXSRCS}
  ${wsjt_qtmm_CXXSRCS}
  ${wsjtx_CXXSRCS}
  ${qcp_CXXSRCS}
  )

set (all_C_and_CXXSRCS
  ${wsjt_CSRCS}
  ${wsprsim_CSRCS}
  ${wsprd_CSRCS}
  ${all_CXXSRCS}
  )

set (TOP_LEVEL_RESOURCES
  "${DECODIUM_RUNTIME_DATA_DIR}/sat.dat"
  resources/map/decodium-awards-catalog.json
  artwork/ft2logo.png
  contrib/gpl-v3-logo.svg
  artwork/splash.png
  icons/Darwin/wsjt.iconset/icon_128x128.png
  artwork/maps/earth_2048x1024.jpg
  artwork/maps/world_overlay_2048x1024.png
  )

set (PALETTE_FILES
  Palettes/Banana.pal
  Palettes/Blue1.pal
  Palettes/Blue2.pal
  Palettes/Blue3.pal
  Palettes/Brown.pal
  Palettes/Cyan1.pal
  Palettes/Cyan2.pal
  Palettes/Cyan3.pal
  Palettes/Default.pal
  Palettes/Digipan.pal
  Palettes/Fldigi.pal
  Palettes/Gray1.pal
  Palettes/Gray2.pal
  Palettes/Green1.pal
  Palettes/Green2.pal
  Palettes/Jungle.pal
  Palettes/Linrad.pal
  Palettes/W9MDB.pal
  Palettes/Negative.pal
  Palettes/Orange.pal
  Palettes/Pink.pal
  Palettes/Rainbow.pal
  Palettes/Raptor.pal
  Palettes/Scope.pal
  Palettes/Sunburst.pal
  Palettes/VK4BDJ.pal
  Palettes/YL2KF.pal
  Palettes/Yellow1.pal
  Palettes/Yellow2.pal
  Palettes/ZL1FZ.pal
)

if (APPLE)
  set (WSJTX_ICON_FILE ${CMAKE_PROJECT_NAME}.icns)
  set (WSJTX_ICONSET_NAME ${CMAKE_PROJECT_NAME})
  if (NOT EXISTS "${CMAKE_SOURCE_DIR}/icons/Darwin/${WSJTX_ICONSET_NAME}.iconset")
    set (WSJTX_ICONSET_NAME wsjt)
  endif ()
  set (ICONSRCS
    icons/Darwin/${WSJTX_ICONSET_NAME}.iconset/icon_16x16.png
    icons/Darwin/${WSJTX_ICONSET_NAME}.iconset/icon_16x16@2x.png
    icons/Darwin/${WSJTX_ICONSET_NAME}.iconset/icon_32x32.png
    icons/Darwin/${WSJTX_ICONSET_NAME}.iconset/icon_32x32@2x.png
    icons/Darwin/${WSJTX_ICONSET_NAME}.iconset/icon_128x128.png
    icons/Darwin/${WSJTX_ICONSET_NAME}.iconset/icon_128x128@2x.png
    icons/Darwin/${WSJTX_ICONSET_NAME}.iconset/icon_256x256.png
    icons/Darwin/${WSJTX_ICONSET_NAME}.iconset/icon_256x256@2x.png
    icons/Darwin/${WSJTX_ICONSET_NAME}.iconset/icon_512x512.png
    icons/Darwin/${WSJTX_ICONSET_NAME}.iconset/icon_512x512@2x.png
    )
  add_custom_command (
    OUTPUT ${WSJTX_ICON_FILE}
    COMMAND iconutil -c icns --output "${CMAKE_BINARY_DIR}/${WSJTX_ICON_FILE}" "${CMAKE_SOURCE_DIR}/icons/Darwin/${WSJTX_ICONSET_NAME}.iconset"
    DEPENDS ${ICONSRCS}
    COMMENT "Building Icons"
    )
else ()
  set (WSJTX_ICON_FILE icons/windows-icons/decodium.ico)
endif (APPLE)

set_source_files_properties (${WSJTX_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

## disable Qt trace and warning messages from release configurations
# set_property (DIRECTORY APPEND PROPERTY
#  COMPILE_DEFINITIONS $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT;QT_NO_WARNING_OUTPUT>
#  )

set_property (SOURCE ${all_C_and_CXXSRCS} APPEND_STRING PROPERTY COMPILE_FLAGS " -include wsjtx_config.h")
set_property (SOURCE ${all_C_and_CXXSRCS} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/wsjtx_config.h)

option (ENABLE_OMNIRIG "Enable OmniRig support (requires dumpcpp and OmniRig)" ON)
if (WIN32 AND ENABLE_OMNIRIG)
  # generate the OmniRig COM interface source
  find_program (DUMPCPP dumpcpp)
  if (DUMPCPP)
    execute_process (
      COMMAND ${DUMPCPP} -getfile {4FE359C5-A58F-459D-BE95-CA559FB4F270}
      OUTPUT_VARIABLE AXSERVER
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    string (STRIP "${AXSERVER}" AXSERVER)
    if (NOT AXSERVER)
      message (WARNING "OmniRig not installed - OmniRig support disabled")
    else ()
      string (REPLACE "\"" "" AXSERVER ${AXSERVER})
      file (TO_CMAKE_PATH ${AXSERVER} AXSERVERSRCS)
      add_compile_definitions (OMNIRIG_AVAILABLE=1)
      list (APPEND wsjt_qt_CXXSRCS Transceiver/OmniRigTransceiver.cpp)
      set_property (SOURCE Transceiver/OmniRigTransceiver.cpp APPEND_STRING
        PROPERTY COMPILE_FLAGS " -include wsjtx_config.h")
      set_property (SOURCE Transceiver/OmniRigTransceiver.cpp APPEND
        PROPERTY OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/wsjtx_config.h)
    endif ()
  else ()
    message (WARNING "dumpcpp not found - OmniRig support disabled")
  endif ()
endif ()


#
# decide on platform specifc packing and fixing up
#
if (APPLE)
  set (WSJTX_BUNDLE_VERSION ${wsjtx_VERSION})

  # make sure CMAKE_INSTALL_PREFIX ends in /
  string (LENGTH "${CMAKE_INSTALL_PREFIX}" LEN)
  math (EXPR LEN "${LEN} -1" )
  string (SUBSTRING "${CMAKE_INSTALL_PREFIX}" ${LEN} 1 ENDCH)
  if (NOT "${ENDCH}" STREQUAL "/")
    set (CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/")
  endif ()
endif (APPLE)


#
# find some useful tools
#
include (CheckTypeSize)
include (CheckCSourceCompiles)
include (CheckIncludeFiles)
include (CheckSymbolExists)
include (generate_version_info)

find_program(CTAGS ctags)
find_program(ETAGS etags)

#
# Platform checks
#
check_include_files ("stdlib.h;stdarg.h;string.h;float.h" STDC_HEADERS)
check_include_files (stdio.h HAVE_STDIO_H)
check_include_files (stdlib.h HAVE_STDLIB_H)
check_include_files (unistd.h HAVE_UNISTD_H)
check_include_files (sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_files (sys/types.h HAVE_SYS_TYPES_H)
check_include_files (fcntl.h HAVE_FCNTL_H)
check_include_files (sys/stat.h HAVE_SYS_STAT_H)
check_include_files ("linux/ppdev.h;linux/parport.h" HAVE_LINUX_PPDEV_H)
check_include_files ("dev/ppbus/ppi.h;dev/ppbus/ppbconf.h" HAVE_DEV_PPBUS_PPI_H)

#
# Standard C Math Library
#
set (LIBM_TEST_SOURCE "#include<math.h>\nfloat f; int main(){sqrt(f);return 0;}")
check_c_source_compiles ("${LIBM_TEST_SOURCE}" HAVE_MATH)
if (HAVE_MATH)
  set (LIBM_LIBRARIES)
else ()
  set (CMAKE_REQUIRED_LIBRARIES m)
  check_c_source_compiles ("${LIBM_TEST_SOURCE}" HAVE_LIBM_MATH)
  unset (CMAKE_REQUIRED_LIBRARIES)
  if (NOT HAVE_LIBM_MATH)
    message (FATAL_ERROR "Unable to use C math library functions")
  endif ()
  set (LIBM_LIBRARIES m)
endif ()

#
# Boost
#
if (WIN32)
  set (Boost_USE_STATIC_LIBS OFF)
endif ()
find_package (Boost 1.62 REQUIRED COMPONENTS log_setup log)

#
# OpenMP
#
if (APPLE)
  find_path (DECODIUM_LIBOMP_INCLUDE_DIR omp.h
    HINTS
      /opt/homebrew/opt/libomp/include
      /usr/local/opt/libomp/include
    )
  find_library (DECODIUM_LIBOMP_LIBRARY omp
    HINTS
      /opt/homebrew/opt/libomp/lib
      /usr/local/opt/libomp/lib
    )
  if (DECODIUM_LIBOMP_INCLUDE_DIR AND DECODIUM_LIBOMP_LIBRARY)
    set (OpenMP_C_FLAGS "-Xpreprocessor -fopenmp -I${DECODIUM_LIBOMP_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set (OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I${DECODIUM_LIBOMP_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set (OpenMP_C_LIB_NAMES "omp" CACHE STRING "" FORCE)
    set (OpenMP_CXX_LIB_NAMES "omp" CACHE STRING "" FORCE)
    set (OpenMP_omp_LIBRARY "${DECODIUM_LIBOMP_LIBRARY}" CACHE FILEPATH "" FORCE)
  endif ()
endif ()
find_package (OpenMP)

#
# fftw3 single precision library
#
find_package (FFTW3 COMPONENTS single threads REQUIRED)

#
# hamlib setup
#
find_package (Hamlib REQUIRED)
find_program (RIGCTL_EXE rigctl)
find_program (RIGCTLD_EXE rigctld)
find_program (RIGCTLCOM_EXE rigctlcom)

if (RIGCTL_EXE)
  get_filename_component (RIGCTL_EXE "${RIGCTL_EXE}" REALPATH)
endif ()
if (RIGCTLD_EXE)
  get_filename_component (RIGCTLD_EXE "${RIGCTLD_EXE}" REALPATH)
endif ()
if (RIGCTLCOM_EXE)
  get_filename_component (RIGCTLCOM_EXE "${RIGCTLCOM_EXE}" REALPATH)
endif ()

set (_decodium_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
set (_decodium_saved_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
set (_decodium_saved_extra_include_files "${CMAKE_EXTRA_INCLUDE_FILES}")
list (APPEND CMAKE_REQUIRED_INCLUDES "${Hamlib_INCLUDE_DIR}")
list (APPEND CMAKE_REQUIRED_LIBRARIES Hamlib::Hamlib)
list (APPEND CMAKE_EXTRA_INCLUDE_FILES "hamlib/rig.h")
check_type_size (CACHE_ALL HAMLIB_OLD_CACHING)
check_symbol_exists (rig_set_cache_timeout_ms "hamlib/rig.h" HAVE_HAMLIB_CACHING)
check_symbol_exists (rig_get_conf2 "hamlib/rig.h" HAVE_HAMLIB_GET_CONF2)
check_symbol_exists (rig_send_raw "hamlib/rig.h" HAVE_HAMLIB_SEND_RAW)
set (CMAKE_REQUIRED_INCLUDES "${_decodium_saved_required_includes}")
set (CMAKE_REQUIRED_LIBRARIES "${_decodium_saved_required_libraries}")
set (CMAKE_EXTRA_INCLUDE_FILES "${_decodium_saved_extra_include_files}")
unset (_decodium_saved_required_includes)
unset (_decodium_saved_required_libraries)
unset (_decodium_saved_extra_include_files)

find_package (Usb REQUIRED)

# RTL-SDR is optional for developer builds, but release workflows enable and
# package it.  Keeping the feature switch explicit lets contributors build the
# traditional audio-only application without a USB SDR development package.
option (DECODIUM_ENABLE_RTLSDR "Enable RTL-SDR RX input support" ON)
option (DECODIUM_REQUIRE_RTLSDR "Fail configuration if RTL-SDR input support is unavailable" OFF)
set (DECODIUM_HAS_RTLSDR OFF)
if (DECODIUM_ENABLE_RTLSDR)
  find_package (RtlSdr QUIET)
  if (RtlSdr_FOUND)
    set (DECODIUM_HAS_RTLSDR ON)
    message (STATUS "RTL-SDR support enabled: ${RtlSdr_LIBRARY}")
  else ()
    if (DECODIUM_REQUIRE_RTLSDR)
      message (FATAL_ERROR "RTL-SDR support is required but librtlsdr was not found")
    else ()
      message (WARNING "RTL-SDR support requested but librtlsdr was not found; building without RTL-SDR input")
    endif ()
  endif ()
endif ()

if (WSJT_FOX_OTP)
  add_definitions (-DFOX_OTP)
endif ()

#
# Qt6 setup
#

# Widgets finds its own dependencies.
find_package (Qt6 COMPONENTS Widgets SerialPort Multimedia PrintSupport Sql LinguistTools WebSockets Test Core5Compat Concurrent REQUIRED)
find_package (Qt6 COMPONENTS Qml Quick QuickControls2 REQUIRED)
find_package (Qt6 COMPONENTS ShaderTools QUIET)
find_package (Qt6 COMPONENTS GuiPrivate QUIET)
if (APPLE)
  find_package (Qt6 COMPONENTS DBus REQUIRED)
endif ()

if (WIN32)
  add_definitions (-DQT_NEEDS_QTMAIN)
  find_package (Qt6 COMPONENTS ActiveQt AxContainer REQUIRED)
endif (WIN32)

#
# Library building setup
#
include (GenerateExportHeader)
set (CMAKE_CXX_VISIBILITY_PRESET hidden)
set (CMAKE_C_VISIBILITY_PRESET hidden)
set (CMAKE_VISIBILITY_INLINES_HIDDEN ON)
#set (CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)


#
# C & C++ setup
#
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -fexceptions -frtti")
elseif (MSVC)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /permissive-")
endif ()

if (NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pragmas")
endif ()
if (NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fdata-sections -ffunction-sections")
  set (CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -fdata-sections -ffunction-sections")
endif ()
if (NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fdata-sections -ffunction-sections")
  set (CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -fdata-sections -ffunction-sections")
endif ()

if (OPENMP_FOUND)
  if (OpenMP_C_FLAGS)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  endif ()
  if (OpenMP_CXX_FLAGS)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  elseif (OpenMP_C_FLAGS)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_C_FLAGS}")
  endif ()
endif ()

if (WIN32)
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endif (WIN32)

if (APPLE AND ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -stdlib=libc++")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=gnu++17 -pthread")
else ()
  set (CMAKE_CXX_STANDARD 17)
  set (CMAKE_CXX_STANDARD_REQUIRED ON)
endif ()


#
# Linker setup
#
if (NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  if (DECODIUM_KEEP_SYMBOLS)
    set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -g")
  else ()
    set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s")
  endif ()
  set (CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} -Wl,--gc-sections")
endif ()


#
# sort out pre-requisites
#

#
# Setup RPATH so that built executable targets will run in both the
# build tree and the install location without having to set a
# (DYLD|LD)_LIBRARY_PATH override.
#

# use the full RPATH of the build tree
set (CMAKE_SKIP_BUILD_RPATH FALSE)

# when building, don't use the install RPATH, it will still be used
# later on in the install phase
set (CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

# add the automaticaly determined parts of the RPATH which point to
# directories outside of the build tree to the install RPATH
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# the RPATH to be used when installing, but only if it's not a system
# directory
# list (FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" isSystemDir)
# if ("${isSystemDir}" STREQUAL "-1")
#   set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
# endif ("${isSystemDir}" STREQUAL "-1")

set (QT_NEED_RPATH FALSE)
if (NOT "${QT_LIBRARY_DIR}" STREQUAL "/lib" AND NOT "${QT_LIBRARY_DIR}" STREQUAL "/usr/lib" AND NOT "${QT_LIBRARY_DIR}" STREQUAL "/lib64" AND NOT "${QT_LIBRARY_DIR}" STREQUAL "/usr/lib64")
  set (QT_NEED_RPATH TRUE)
endif ()

#
# stuff only qmake can tell us
#
get_target_property (QMAKE_EXECUTABLE Qt6::qmake LOCATION)
get_target_property (LCONVERT_EXECUTABLE Qt6::lconvert LOCATION)
function (QUERY_QMAKE VAR RESULT)
  exec_program (${QMAKE_EXECUTABLE} ARGS "-query ${VAR}" RETURN_VALUE return_code OUTPUT_VARIABLE output)
  if (NOT return_code)
    file (TO_CMAKE_PATH "${output}" output)
    set (${RESULT} ${output} PARENT_SCOPE)
  endif (NOT return_code)
  message (STATUS "Asking qmake for ${RESULT} and got ${output}")
endfunction (QUERY_QMAKE)

query_qmake (QT_INSTALL_PLUGINS QT_PLUGINS_DIR)
query_qmake (QT_INSTALL_TRANSLATIONS QT_TRANSLATIONS_DIR)
query_qmake (QT_INSTALL_IMPORTS QT_IMPORTS_DIR)
query_qmake (QT_HOST_DATA QT_DATA_DIR)
set (QT_MKSPECS_DIR ${QT_DATA_DIR}/mkspecs)

# project definitions
add_definitions (-DQT6 -DCMAKE_BUILD -DBIGSYM=1 -DBOOST_ALL_DYN_LINK)
if (CMAKE_HOST_UNIX)
  add_definitions (-DUNIX)
elseif (CMAKE_HOST_WIN32)
  add_definitions (-DWIN32)
endif ()

#
# sub-directories
#
if (EXISTS ${CMAKE_SOURCE_DIR}/samples AND IS_DIRECTORY ${CMAKE_SOURCE_DIR}/samples)
  add_subdirectory (samples)
endif ()
if (WSJT_GENERATE_DOCS)
  add_subdirectory (doc)
endif (WSJT_GENERATE_DOCS)

add_library (ft2link_core STATIC ${FT2LINK_CORE_CXXSRCS})
target_include_directories (ft2link_core PUBLIC ${CMAKE_SOURCE_DIR})
set_target_properties (ft2link_core PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

if (DECODIUM_ENABLE_SSTV)
  add_subdirectory (src/sstv)
endif ()

if (DECODIUM_ENABLE_RTTY)
  add_subdirectory (src/rtty)
endif ()

if (BUILD_TESTING AND EXISTS ${CMAKE_SOURCE_DIR}/tests AND IS_DIRECTORY ${CMAKE_SOURCE_DIR}/tests)
  add_subdirectory (tests)
endif ()

# build a library of package functionality (without and optionally with OpenMP support)
add_library (wsjt_cxx STATIC ${wsjt_CSRCS} ${wsjt_CXXSRCS})
target_include_directories (wsjt_cxx PRIVATE ${FFTW3_INCLUDE_DIRS})
target_link_libraries (wsjt_cxx ft2link_core ${LIBM_LIBRARIES} Boost::log_setup ${FFTW3_LIBRARIES} ${LIBM_LIBRARIES})

# wsjt_fort is now a pure interface library: all maintained application-path
# Fortran has already been migrated in Decodium 3 and is reused here.
add_library (wsjt_fort INTERFACE)
target_link_libraries (wsjt_fort INTERFACE ${FFTW3_LIBRARIES})
add_library (wsjt_fort_omp ALIAS wsjt_fort)

function (link_wsjtx_cycle_group target)
  if (APPLE OR WIN32)
    target_link_libraries (${target} ${ARGN})
  else ()
    target_link_libraries (${target} "-Wl,--start-group" ${ARGN} "-Wl,--end-group")
  endif ()
endfunction ()

if(WSJT_BUILD_UTILS)

# The application no longer links the historical Fortran runtime.  These
# verification utilities compare the maintained C++ JT65 implementation with
# an independently built official WSJT-X Fortran archive, when one is supplied.
set (DECODIUM_JT65_REFERENCE_LIBRARY "" CACHE FILEPATH
  "Path to an official WSJT-X libwsjt_fort.a used by JT65 comparison utilities")

add_executable (q65sim utils/q65sim.cpp)
link_wsjtx_cycle_group (q65sim wsjt_qt wsjt_cxx wsjt_fort)

if (EXISTS ${CMAKE_SOURCE_DIR}/utils/EchoCallSim.cpp
    AND EXISTS ${CMAKE_SOURCE_DIR}/utils/EchoToolSupport.cpp)
  add_executable (EchoCallSim
    utils/EchoCallSim.cpp
    utils/EchoToolSupport.cpp)
  link_wsjtx_cycle_group (EchoCallSim wsjt_qt wsjt_cxx wsjt_fort)
endif ()

if (EXISTS ${CMAKE_SOURCE_DIR}/utils/testEchoCall.cpp
    AND EXISTS ${CMAKE_SOURCE_DIR}/utils/EchoToolSupport.cpp)
  add_executable (testEchoCall
    utils/testEchoCall.cpp
    utils/EchoToolSupport.cpp)
  link_wsjtx_cycle_group (testEchoCall wsjt_qt wsjt_cxx wsjt_fort)
endif ()

add_executable (q65code utils/q65code.cpp)
link_wsjtx_cycle_group (q65code wsjt_qt wsjt_cxx wsjt_fort)

add_executable (test_q65 utils/test_q65.cpp)
link_wsjtx_cycle_group (test_q65 wsjt_qt wsjt_cxx wsjt_fort)

add_executable (q65_ftn_test utils/q65_ftn_test.cpp)
link_wsjtx_cycle_group (q65_ftn_test wsjt_qt wsjt_cxx wsjt_fort)

add_executable (q65params utils/q65params.cpp)
target_link_libraries (q65params wsjt_qt wsjt_cxx)

add_executable (rtty_spec utils/rtty_spec.cpp)
link_wsjtx_cycle_group (rtty_spec wsjt_qt wsjt_cxx wsjt_fort)

if (DECODIUM_JT65_REFERENCE_LIBRARY)
  find_program (DECODIUM_GFORTRAN_EXECUTABLE NAMES gfortran REQUIRED)
  execute_process (
    COMMAND ${DECODIUM_GFORTRAN_EXECUTABLE} -print-file-name=libgfortran${CMAKE_SHARED_LIBRARY_SUFFIX}
    OUTPUT_VARIABLE DECODIUM_GFORTRAN_RUNTIME
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if (NOT EXISTS ${DECODIUM_GFORTRAN_RUNTIME})
    message (FATAL_ERROR "Cannot locate the gfortran runtime needed by JT65 reference comparison utilities")
  endif ()
  add_executable (jt65_compare
    utils/jt65_compare.cpp
    Modulator/FtxWaveformGenerator.cpp)
  link_wsjtx_cycle_group (jt65_compare wsjt_qt wsjt_cxx wsjt_fort
                          ${DECODIUM_JT65_REFERENCE_LIBRARY} ${DECODIUM_GFORTRAN_RUNTIME})

  add_executable (jt65_symspec_compare utils/jt65_symspec_compare.cpp)
  link_wsjtx_cycle_group (jt65_symspec_compare wsjt_qt wsjt_cxx wsjt_fort
                          ${DECODIUM_JT65_REFERENCE_LIBRARY} ${DECODIUM_GFORTRAN_RUNTIME})

  add_executable (jt65_sync_compare
    utils/jt65_sync_compare.cpp)
  link_wsjtx_cycle_group (jt65_sync_compare wsjt_qt wsjt_cxx wsjt_fort
                          ${DECODIUM_JT65_REFERENCE_LIBRARY} ${DECODIUM_GFORTRAN_RUNTIME})

  add_executable (jt65_extract_primitives_compare
    utils/jt65_extract_primitives_compare.cpp)
  link_wsjtx_cycle_group (jt65_extract_primitives_compare wsjt_qt wsjt_cxx wsjt_fort
                          ${DECODIUM_JT65_REFERENCE_LIBRARY} ${DECODIUM_GFORTRAN_RUNTIME})

  add_executable (jt65_extract_compare
    utils/jt65_extract_compare.cpp)
  link_wsjtx_cycle_group (jt65_extract_compare wsjt_qt wsjt_cxx wsjt_fort
                          ${DECODIUM_JT65_REFERENCE_LIBRARY} ${DECODIUM_GFORTRAN_RUNTIME})

  add_executable (jt65_decode65b_compare
    utils/jt65_decode65b_compare.cpp)
  link_wsjtx_cycle_group (jt65_decode65b_compare wsjt_qt wsjt_cxx wsjt_fort
                          ${DECODIUM_JT65_REFERENCE_LIBRARY} ${DECODIUM_GFORTRAN_RUNTIME})

  add_executable (jt65_decode65a_compare
    utils/jt65_decode65a_compare.cpp)
  link_wsjtx_cycle_group (jt65_decode65a_compare wsjt_qt wsjt_cxx wsjt_fort
                          ${DECODIUM_JT65_REFERENCE_LIBRARY} ${DECODIUM_GFORTRAN_RUNTIME})
else ()
  message (STATUS "JT65 reference comparison utilities disabled: set DECODIUM_JT65_REFERENCE_LIBRARY to an official WSJT-X libwsjt_fort.a")
endif ()

if (DECODIUM_JT65_REFERENCE_LIBRARY)
  add_executable (jt9_compare utils/jt9_compare.cpp)
  link_wsjtx_cycle_group (jt9_compare wsjt_qt wsjt_cxx wsjt_fort
                          ${DECODIUM_JT65_REFERENCE_LIBRARY} ${DECODIUM_GFORTRAN_RUNTIME})
endif ()

add_executable (jt9_narrow_compare
  utils/jt9_narrow_compare.cpp
  Detector/FftCompat.cpp
  Detector/JT9FastDecoder.cpp
  Detector/JT9FastHelpers.cpp
  Detector/JT9NarrowDecoder.cpp
  Detector/LegacyDspIoHelpers.cpp
  lib/wsprd/fano.c
)

if (NOT MSVC)
  set_source_files_properties(
    src/services/satellite/sgp4/SGP4.cpp
    PROPERTIES COMPILE_OPTIONS "-Wno-unused-function;-Wno-unused-result;-Wno-error=unused-result")
endif()
target_include_directories (jt9_narrow_compare PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${FFTW3_INCLUDE_DIRS}
)
target_link_libraries (jt9_narrow_compare
  Qt6::Core
  ${FFTW3_LIBRARIES}
  ${LIBM_LIBRARIES}
  wsjt_cxx
  wsjt_qt
)

if (DECODIUM_JT65_REFERENCE_LIBRARY)
  add_executable (jt4_compare utils/jt4_compare.cpp)
  link_wsjtx_cycle_group (jt4_compare wsjt_qt wsjt_cxx wsjt_fort
                          ${DECODIUM_JT65_REFERENCE_LIBRARY} ${DECODIUM_GFORTRAN_RUNTIME})
endif ()

add_executable (jt4_wav_decode utils/jt4_wav_decode.cpp)
target_link_libraries (jt4_wav_decode wsjt_qt wsjt_cxx Qt6::Core)

add_executable (wspr_compare utils/wspr_compare.cpp)
link_wsjtx_cycle_group (wspr_compare wsjt_qt wsjt_cxx)

add_executable (plot_compare utils/plot_compare.cpp)
link_wsjtx_cycle_group (plot_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (indexx_compare utils/indexx_compare.cpp)
link_wsjtx_cycle_group (indexx_compare wsjt_qt wsjt_cxx)
	       
add_executable (encode77 utils/encode77.cpp)
link_wsjtx_cycle_group (encode77 wsjt_qt wsjt_cxx wsjt_fort)

add_executable (nonstandard_call_compare utils/nonstandard_call_compare.cpp)
link_wsjtx_cycle_group (nonstandard_call_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (hash22calc utils/hash22calc.cpp)
link_wsjtx_cycle_group (hash22calc wsjt_qt wsjt_cxx wsjt_fort)

add_executable (wsprsim ${wsprsim_CSRCS})
target_link_libraries (wsprsim ${LIBM_LIBRARIES})

add_executable (msk144code utils/msk144code.cpp)
link_wsjtx_cycle_group (msk144code wsjt_qt wsjt_cxx wsjt_fort)

add_executable (ft8code utils/ft8code.cpp)
link_wsjtx_cycle_group (ft8code wsjt_qt wsjt_cxx wsjt_fort)

add_executable (ft4code utils/ft4code.cpp)
link_wsjtx_cycle_group (ft4code wsjt_qt wsjt_cxx wsjt_fort)

if (EXISTS ${CMAKE_SOURCE_DIR}/utils/echosim.cpp
    AND EXISTS ${CMAKE_SOURCE_DIR}/utils/EchoToolSupport.cpp)
  add_executable (echosim
    utils/echosim.cpp
    utils/EchoToolSupport.cpp)
  link_wsjtx_cycle_group (echosim wsjt_qt wsjt_cxx wsjt_fort)
endif ()

add_executable (ft8sim utils/ft8sim.cpp)
link_wsjtx_cycle_group (ft8sim wsjt_qt wsjt_cxx wsjt_fort)

add_executable (sfoxsim utils/sfoxsim.cpp)
link_wsjtx_cycle_group (sfoxsim wsjt_qt wsjt_cxx wsjt_fort)

add_executable (sfrx utils/sfrx.cpp)
link_wsjtx_cycle_group (sfrx wsjt_qt wsjt_cxx wsjt_fort)

add_executable (sftx utils/sftx.cpp)
link_wsjtx_cycle_group (sftx wsjt_qt wsjt_cxx wsjt_fort)

add_executable (msk144sim utils/msk144sim.cpp)
link_wsjtx_cycle_group (msk144sim wsjt_qt wsjt_cxx wsjt_fort)

add_executable (ft4sim utils/ft4sim.cpp)
link_wsjtx_cycle_group (ft4sim wsjt_qt wsjt_cxx wsjt_fort)

add_executable (ft4sim_mult utils/ft4sim_mult.cpp)
link_wsjtx_cycle_group (ft4sim_mult wsjt_qt wsjt_cxx wsjt_fort)

add_executable (fst4sim utils/fst4sim.cpp)
link_wsjtx_cycle_group (fst4sim wsjt_qt wsjt_cxx wsjt_fort)
if (MINGW)
  set_target_properties (fst4sim PROPERTIES
    LINK_FLAGS -Wl,--stack,0x4000000,--heap,0x6000000
    )
endif ()

add_executable (ldpcsim240_101 utils/ldpcsim240_101.cpp)
target_link_libraries (ldpcsim240_101 wsjt_qt wsjt_cxx wsjt_fort)

add_executable (ldpcsim240_74 utils/ldpcsim240_74.cpp)
target_link_libraries (ldpcsim240_74 wsjt_qt wsjt_cxx wsjt_fort)

endif(WSJT_BUILD_UTILS)

# build the wsprd WSPR mode decoder driver
generate_version_info (wsprd_VERSION_RESOURCES
  NAME wsprd
  BUNDLE ${PROJECT_BUNDLE_NAME}
  ICON ${WSJTX_ICON_FILE}
  FILE_DESCRIPTION "WSPR mode decoder"
  )
add_executable (wsprd ${wsprd_CSRCS} ${wsprd_VERSION_RESOURCES})
target_include_directories (wsprd PRIVATE ${FFTW3_INCLUDE_DIRS})
target_link_libraries (wsprd ${FFTW3_LIBRARIES} ${LIBM_LIBRARIES})
if (APPLE AND CMAKE_C_COMPILER_ID MATCHES "Clang")
  execute_process (
    COMMAND ${CMAKE_C_COMPILER} --print-resource-dir
    OUTPUT_VARIABLE CLANG_RESOURCE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  set (CLANG_RT_OSX "${CLANG_RESOURCE_DIR}/lib/darwin/libclang_rt.osx.a")
  if (EXISTS "${CLANG_RT_OSX}")
    target_link_libraries (wsprd "${CLANG_RT_OSX}")
  endif ()
endif ()

# Tell CMake to run moc when necessary
set (CMAKE_AUTOMOC ON)
include_directories (${CMAKE_CURRENT_BINARY_DIR})

# don't use Qt "keywords" signal, slot, emit in generated files to
# avoid compatability issue with other libraries
# ADD_DEFINITIONS (-DQT_NO_KEYWORDS)
# ADD_DEFINITIONS (-DUNICODE)	#as per qmake

# As moc files are generated in the binary dir, tell CMake to always
# look for includes there:
set (CMAKE_INCLUDE_CURRENT_DIR ON)

#
# source navigation
#
set (sources
  ${CMAKE_SOURCE_DIR}/*
  ${CMAKE_SOURCE_DIR}/src/*
  ${CMAKE_SOURCE_DIR}/logbook/*
  ${CMAKE_SOURCE_DIR}/lib/*
  )
add_custom_target (ctags COMMAND ${CTAGS} -o ${CMAKE_SOURCE_DIR}/tags -R ${sources})
add_custom_target (etags COMMAND ${ETAGS} -o ${CMAKE_SOURCE_DIR}/TAGS -R ${sources})


# Qt i18n - always include the country generic if any regional variant is included
set (LANGUAGES
  ca				# Catalan
  da				# Danish
  de				# German
  en				# English (we need this to stop
				# translation loaders loading the
				# second preference UI languge, it
				# doesn't need to be populated)
  es				# Spanish
  fr				# French
  hu				# Hungarian
  it				# Italian
  ja				# Japanese
  lv				# Latvian
  nl				# Dutch (Nederlands)
  #no				# Norwegian
  #pt				# Portuguese
  ro				# Romanian
  ru				# Russian
  #sv				# Swedish
  zh				# Chinese
  zh_TW				# Chinese traditional
  )
foreach (lang_ ${LANGUAGES})
  set (ts_source_ "${CMAKE_SOURCE_DIR}/translations/decodium_${lang_}.ts")
  if (EXISTS "${ts_source_}")
    list (APPEND TS_FILES "${ts_source_}")
  else ()
    message (STATUS "Skipping missing translation source: ${ts_source_}")
  endif ()
  set (qt_translations_ "${QT_TRANSLATIONS_DIR}/qtbase_${lang_}.qm")
  if (EXISTS "${qt_translations_}")
    add_custom_command (
      OUTPUT "${CMAKE_BINARY_DIR}/qt_${lang_}.qm"
      COMMAND ${LCONVERT_EXECUTABLE} -o "${CMAKE_BINARY_DIR}/qt_${lang_}.qm" ${qt_translations_}
      COMMENT "Building required Qt translations for language ${lang_}"
      )
    list (APPEND QM_FILES "${CMAKE_BINARY_DIR}/qt_${lang_}.qm")
  endif ()
endforeach ()
if (UPDATE_TRANSLATIONS)
  message (STATUS "UPDATE_TRANSLATIONS option is set.")
  if (TS_FILES)
    qt_create_translation (
      QM_FILES ${wsjt_qt_UISRCS} ${wsjtx_UISRCS} ${wsjt_qt_CXXSRCS} ${wsjtx_CXXSRCS}
      ${TS_FILES}
      OPTIONS -I${CMAKE_CURRENT_SOURCE_DIR}
      )
  else ()
    message (STATUS "No source translation .ts files found; skipping translation update.")
  endif ()
else ()
  if (TS_FILES)
    qt_add_translation (QM_FILES ${TS_FILES})
  else ()
    message (STATUS "No source translation .ts files found; skipping app translation compilation.")
  endif ()
endif ()
add_custom_target (translations DEPENDS ${QM_FILES})
set_property (DIRECTORY PROPERTY CLEAN_NO_CUSTOM TRUE)

# embedded resources
function (add_resources resources path)
  foreach (resource_file_ ${ARGN})
    get_filename_component (name_ ${resource_file_} NAME)
    if (IS_ABSOLUTE "${resource_file_}")
      file (TO_CMAKE_PATH "${resource_file_}" source_)
    else ()
      file (TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${resource_file_}" source_)
    endif ()
    file (TO_CMAKE_PATH "${path}/${name_}" dest_)
    set (resources_ "${resources_}\n    <file alias=\"${dest_}\">${source_}</file>")
    set (${resources} ${${resources}}${resources_} PARENT_SCOPE)
  endforeach (resource_file_ ${ARGN})
endfunction (add_resources resources path)

add_resources (decodium_RESOURCES "" ${TOP_LEVEL_RESOURCES})
add_resources (decodium_RESOURCES /Palettes ${PALETTE_FILES})
add_resources (decodium_RESOURCES /Translations ${QM_FILES})

configure_file (
  "${DECODIUM_CMAKE_TEMPLATE_DIR}/decodium.qrc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/decodium.qrc"
  @ONLY)

# UI generation
qt_wrap_ui (wsjt_qt_GENUISRCS ${wsjt_qt_UISRCS})
qt_wrap_ui (wsjtx_GENUISRCS ${wsjtx_UISRCS})

# Resource generation
qt_add_resources (wsjtx_RESOURCES_RCC
  ${CMAKE_BINARY_DIR}/decodium.qrc
  contrib/QDarkStyleSheet/qdarkstyle/style.qrc
  )

# AX COM servers
if (WIN32 AND AXSERVERSRCS)
  include (QtAxMacros)
  wrap_ax_server (GENAXSRCS ${AXSERVERSRCS})
endif ()

#
# targets dependent on Qt
#

# build a library for the QCustomPlot widget
# Suppress deprecated-declarations for the bundled QCustomPlot 2.1.1 which uses some Qt5 APIs
# that are deprecated in Qt6 but still functional.
set_source_files_properties (qcustomplot-source/qcustomplot.cpp PROPERTIES
  COMPILE_FLAGS "-Wno-deprecated-declarations")
add_library (qcp STATIC ${qcp_CXXSRCS})
target_compile_options (qcp PRIVATE -Wno-error=misleading-indentation)
target_include_directories (qcp PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qcustomplot-source>)
target_link_libraries (qcp Qt6::Widgets Qt6::PrintSupport)

# build a library of package Qt functionality
add_library (wsjt_qt STATIC ${wsjt_qt_CXXSRCS} ${wsjt_qt_GENUISRCS} ${GENAXSRCS})
# Suppress deprecation warnings from Qt5 APIs not yet updated for Qt6 in wsjt_qt sources
target_compile_options (wsjt_qt PRIVATE -Wno-deprecated-declarations)
# set wsjtx_udp exports to static variants
target_compile_definitions (wsjt_qt PUBLIC UDP_STATIC_DEFINE)
target_link_libraries (wsjt_qt Hamlib::Hamlib Boost::log qcp Qt6::Widgets Qt6::Network Qt6::Sql Qt6::WebSockets Qt6::Core5Compat)
if (TARGET OpenMP::OpenMP_CXX)
  target_link_libraries (wsjt_qt OpenMP::OpenMP_CXX)
endif ()
if (WIN32)
  target_link_libraries (wsjt_qt Qt6::ActiveQt crypt32)
endif (WIN32)

# wsjt_fort is an INTERFACE library: propagate wsjt_qt and wsjt_cxx transitively
# so targets that still link wsjt_fort continue to resolve the migrated symbols.
target_link_libraries (wsjt_fort INTERFACE wsjt_qt wsjt_cxx)
# build a library of package Qt functionality used in Fortran utilities
add_library (fort_qt STATIC ${fort_qt_CXXSRCS})
target_link_libraries (fort_qt Qt6::Core)

# build a library of WSJT Qt multimedia components
add_library (wsjt_qtmm STATIC ${wsjt_qtmm_CXXSRCS} ${wsjt_qtmm_GENUISRCS})
target_link_libraries (wsjt_qtmm Qt6::Multimedia)

if (WSJT_BUILD_UTILS)
  add_executable (jt9 utils/jt9.cpp)
  target_sources (jt9 PRIVATE
    Detector/FT2DecodeWorker.cpp
    Detector/FT4DecodeWorker.cpp
    Detector/FT8DecodeWorker.cpp
    Detector/LegacyJtDecodeWorker.cpp
    Detector/MSK144DecodeWorker.cpp
    Detector/Q65DecodeWorker.cpp
    )
  if (DECODIUM_JT65_REFERENCE_LIBRARY)
    link_wsjtx_cycle_group (jt9 wsjt_qt wsjt_cxx wsjt_fort
                            ${DECODIUM_JT65_REFERENCE_LIBRARY} ${DECODIUM_GFORTRAN_RUNTIME})
  else ()
    link_wsjtx_cycle_group (jt9 wsjt_qt wsjt_cxx wsjt_fort)
  endif ()

  add_executable (jt9a utils/jt9.cpp)
  target_sources (jt9a PRIVATE
    Detector/FT2DecodeWorker.cpp
    Detector/FT4DecodeWorker.cpp
    Detector/FT8DecodeWorker.cpp
    Detector/LegacyJtDecodeWorker.cpp
    Detector/MSK144DecodeWorker.cpp
    Detector/Q65DecodeWorker.cpp
    )
  if (DECODIUM_JT65_REFERENCE_LIBRARY)
    link_wsjtx_cycle_group (jt9a wsjt_qt wsjt_cxx wsjt_fort
                            ${DECODIUM_JT65_REFERENCE_LIBRARY} ${DECODIUM_GFORTRAN_RUNTIME})
  else ()
    link_wsjtx_cycle_group (jt9a wsjt_qt wsjt_cxx wsjt_fort)
  endif ()

  if (NOT APPLE)
    add_executable (record_time_signal Audio/tools/record_time_signal.cpp)
    target_link_libraries (record_time_signal wsjt_cxx wsjt_qtmm wsjt_qt)
  endif ()
endif (WSJT_BUILD_UTILS)

if (WIN32)
  find_package (Portaudio REQUIRED)
  add_subdirectory (map65)
endif ()
  if (WIN32)
    add_subdirectory (qmap)
  endif ()

# build the main application
generate_version_info (wsjtx_VERSION_RESOURCES
  NAME wsjtx
  BUNDLE ${PROJECT_BUNDLE_NAME}
  ICON ${WSJTX_ICON_FILE}
  )

add_executable (wsjtx MACOSX_BUNDLE
  ${wsjtx_CXXSRCS}
  ${wsjtx_GENUISRCS}
  Web/DecodiumWebServer.cpp
  Web/DecodiumWebServer.hpp
  ${WSJTX_ICON_FILE}
  ${wsjtx_RESOURCES_RCC}
  ${wsjtx_VERSION_RESOURCES}
  )

if (WSJT_CREATE_WINMAIN)
  set_target_properties (wsjtx PROPERTIES WIN32_EXECUTABLE ON)
endif (WSJT_CREATE_WINMAIN)

set_target_properties (wsjtx PROPERTIES
  OUTPUT_NAME "${WSJT_APP_EXECUTABLE_NAME}"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Darwin/Info.plist.in"
  MACOSX_BUNDLE_INFO_STRING "${PROJECT_DESCRIPTION}"
  MACOSX_BUNDLE_ICON_FILE "${WSJTX_ICON_FILE}"
  MACOSX_BUNDLE_BUNDLE_VERSION "${FORK_RELEASE_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "v${FORK_RELEASE_VERSION}"
  MACOSX_BUNDLE_LONG_VERSION_STRING "Version ${FORK_RELEASE_VERSION}${SCS_VERSION_STR}"
  MACOSX_BUNDLE_BUNDLE_NAME "${WSJT_APP_DISPLAY_NAME}"
  MACOSX_BUNDLE_BUNDLE_EXECUTABLE_NAME "${WSJT_APP_EXECUTABLE_NAME}"
  MACOSX_BUNDLE_COPYRIGHT "${PROJECT_COPYRIGHT}"
  MACOSX_BUNDLE_GUI_IDENTIFIER "${WSJT_APP_GUI_IDENTIFIER}"
  )

target_include_directories (wsjtx PRIVATE ${FFTW3_INCLUDE_DIRS})
if (OPENMP_FOUND)
  if (OpenMP_CXX_FLAGS)
    set_target_properties (wsjtx PROPERTIES
      COMPILE_FLAGS "${OpenMP_CXX_FLAGS}"
      )
  endif ()
  if (TARGET OpenMP::OpenMP_CXX)
    target_link_libraries (wsjtx OpenMP::OpenMP_CXX)
  elseif (APPLE AND DECODIUM_LIBOMP_LIBRARY)
    target_link_libraries (wsjtx ${DECODIUM_LIBOMP_LIBRARY})
  elseif (OpenMP_CXX_FLAGS)
    set_target_properties (wsjtx PROPERTIES
      LINK_FLAGS "${OpenMP_CXX_FLAGS}"
      )
  endif ()
  if (MINGW)
    set_target_properties (wsjtx PROPERTIES
      LINK_FLAGS -Wl,--stack,0x1000000,--heap,0x20000000
      )
    endif ()
endif ()
target_link_libraries (wsjtx Qt6::SerialPort Qt6::Quick Qt6::Qml Qt6::QuickControls2 Qt6::Network Qt6::Sql Qt6::WebSockets Qt6::Concurrent wsjt_qtmm ${FFTW3_LIBRARIES} ${LIBM_LIBRARIES})
if (TARGET Decodium::SstvIntegration)
  target_link_libraries (wsjtx Decodium::SstvIntegration)
  target_link_libraries (wsjtx Decodium::SstvGallery)
  target_link_libraries (wsjtx Decodium::SstvSharingUi)
  target_link_libraries (wsjtx Decodium::SstvDiagnostics)
  if (TARGET Decodium::SstvStudio)
    target_link_libraries (wsjtx Decodium::SstvStudio)
  endif ()
  target_compile_definitions (wsjtx PRIVATE DECODIUM_HAS_SSTV=1)
else ()
  target_compile_definitions (wsjtx PRIVATE DECODIUM_HAS_SSTV=0)
endif ()
if (TARGET Decodium::HamDrmRuntime)
  target_link_libraries (wsjtx Decodium::HamDrmRuntime)
  target_compile_definitions (wsjtx PRIVATE DECODIUM_HAS_HAMDRM=1)
else ()
  target_compile_definitions (wsjtx PRIVATE DECODIUM_HAS_HAMDRM=0)
endif ()
if (APPLE)
  target_link_libraries (wsjtx Qt6::DBus "-framework AudioToolbox")
endif ()
if (WIN32)
  target_link_libraries (wsjtx Pdh Qt6::AxContainer)
endif ()
target_compile_options (wsjtx PRIVATE -Wno-deprecated-declarations)
if (DECODIUM_HAS_RTLSDR)
  target_compile_definitions (wsjtx PRIVATE DECODIUM_HAS_RTLSDR=1)
  target_link_libraries (wsjtx RtlSdr::RtlSdr)
else ()
  target_compile_definitions (wsjtx PRIVATE DECODIUM_HAS_RTLSDR=0)
endif ()
link_wsjtx_cycle_group (wsjtx wsjt_qt wsjt_cxx wsjt_fort)

# -----------------------------------------------------------------------
# decodium_qml: lightweight QML-based frontend (uses DecodiumBridge)
# -----------------------------------------------------------------------
if (WIN32)
  generate_version_info (decodium_qml_VERSION_RESOURCES
    NAME decodium
    BUNDLE ${PROJECT_BUNDLE_NAME}
    ICON ${WSJTX_ICON_FILE}
    VERSION_MAJOR ${FORK_RELEASE_VERSION_MAJOR}
    VERSION_MINOR ${FORK_RELEASE_VERSION_MINOR}
    VERSION_PATCH ${FORK_RELEASE_VERSION_PATCH}
    VERSION_REVISION NONE
    )
else ()
  set (decodium_qml_VERSION_RESOURCES "")
endif ()
set (DECODIUM_QML_LEGACY_SRCS
  src/bridge/DecodiumLogging.cpp
  logbook/logbook.cpp
  Network/PSKReporter.cpp
  Detector/Detector.cpp
  widgets/logqso.cpp
  widgets/displaytext.cpp
  Decoder/decodedtext.cpp
  src/core/getfile.cpp
  widgets/meterwidget.cpp
  widgets/signalmeter.cpp
  widgets/plotter.cpp
  widgets/widegraph.cpp
  widgets/echograph.cpp
  widgets/echoplot.cpp
  widgets/fastgraph.cpp
  widgets/fastplot.cpp
  widgets/about.cpp
  widgets/asyncmodewidget.cpp
  widgets/FT2QsoFlowPolicy.cpp
  widgets/astro.cpp
  widgets/messageaveraging.cpp
  widgets/activeStations.cpp
  widgets/worldmapwidget.cpp
  widgets/colorhighlighting.cpp
  widgets/RTTYTerminalWidget.cpp
  Modulator/RTTYModulator.cpp
  Detector/RTTYDetector.cpp
  Decoder/BaudotDecoder.cpp
  src/security/DecodiumCertificate.cpp
  WSPR/WsprTxScheduler.cpp
  widgets/mainwindow.cpp
  src/app/Configuration.cpp
  WSPR/WSPRBandHopping.cpp
  widgets/ExportCabrillo.cpp
  widgets/QSYMessage.cpp
  widgets/QSYMessageCreator.cpp
  widgets/qsymonitor.cpp
  widgets/TimeSyncPanel.cpp
  widgets/IonosphericForecastWindow.cpp
  widgets/DXClusterWindow.cpp
  )
add_executable (decodium_qml
  src/app/main_qml.cpp
  src/app/DecodiumStorageMigration.cpp
  src/app/DecodiumStorageMigration.hpp
  src/app/DecodiumSelfCheck.cpp
  src/app/DecodiumSelfCheck.hpp
  src/app/DecodiumUpdater.cpp
  src/app/DecodiumUpdater.hpp
  src/app/DecodiumUpdateSupport.hpp
  src/services/DecodiumCommunityReport.cpp
  src/services/DecodiumCommunityReport.hpp
  src/net/DecoPortPacket.cpp
  src/net/DecoPortPacket.h
  src/net/RadioLink.h
  src/net/DecoPortLink.cpp
  src/net/DecoPortLink.h
  src/net/DecodiumDecoPortGateway.cpp
  src/net/DecoPortRigDriver.cpp
  src/net/DecodiumDecoPortGateway.h
  src/bridge/DecodiumBridge.cpp
  src/bridge/AdifExportSanitizer.cpp
  src/bridge/AdifExportSanitizer.h
  src/bridge/DecodiumBridgeSstv.cpp
  src/bridge/DecodiumBridge.h
  Sequencer/QsoSequencerRules.cpp
  Sequencer/QsoSequencerRules.hpp
  Sequencer/MessageTokenRules.cpp
  Sequencer/MessageTokenRules.hpp
  Sequencer/QsoSequencerState.hpp
  Sequencer/ISequencerSink.hpp
  Sequencer/QsoSequencer.hpp
  src/services/UsStateDataManager.cpp
  src/services/UsStateDataManager.h
  src/models/DecodeListModel.cpp
  src/models/DecodeListModel.h
  lib/persistence/DecodeHistoryWorker.cpp
  lib/persistence/DecodeHistoryWorker.h
  Web/DecodiumWebServer.cpp
  Web/DecodiumWebServer.hpp
  src/bridge/DecodiumDiagnostics.cpp
  src/services/DecodiumPropagationManager.cpp
  src/services/DecodiumPropagationManager.h
  src/services/RotatorService.cpp
  src/services/RotatorService.h
  src/services/SatelliteTrackingService.cpp
  src/services/SatelliteTrackingService.h
  src/services/satellite/sgp4/SGP4.cpp
  src/services/satellite/sgp4/SGP4.h
  src/services/MapIntelligenceService.cpp
  src/services/MapIntelligenceService.h
  src/services/MapBaseMapService.cpp
  src/services/MapBaseMapService.h
  src/services/MapPskFeedService.cpp
  src/services/MapPskFeedService.h
  src/services/MapOperationsService.cpp
  src/services/MapOperationsService.h
  src/services/MapExternalOverlayService.cpp
  src/services/MapExternalOverlayService.h
  src/services/MapLayerModel.cpp
  src/services/MapLayerModel.h
  src/bridge/DecodiumLegacyBackend.cpp
  src/bridge/DecodiumLegacyBackend.h
  src/core/helper_functions.cpp
  src/core/helper_functions.h
  controllers/ActiveStationsModel.cpp
  controllers/ActiveStationsModel.hpp
  controllers/FT2LinkQmlAdapter.cpp
  controllers/FT2LinkQmlAdapter.hpp
  src/ui/DecodiumThemeManager.cpp
  src/ui/DecodiumThemeManager.h
  src/bridge/DecodiumSubManagers.cpp
  src/bridge/DecodiumSubManagers.h
  src/radio/DecodiumCatManager.cpp
  src/radio/DecodiumCatManager.h
  src/radio/DecodiumOmniRigManager.cpp
  src/radio/DecodiumOmniRigManager.h
  src/radio/DecodiumCat4OmManager.cpp
  src/radio/DecodiumCat4OmManager.h
  src/radio/DecodiumTransceiverManager.cpp
  src/radio/DecodiumTransceiverManager.h
  src/radio/DecodiumCatShare.cpp
  src/radio/DecodiumCatShare.h
  src/radio/DecodiumAmplifier.cpp
  src/radio/StationTelemetryCodec.cpp
  src/radio/DecodiumAmplifier.h
  src/radio/DecodiumRigDetector.cpp
  src/radio/DecodiumRigDetector.h
  src/services/DecodiumAlertManager.cpp
  src/services/DecodiumAlertManager.h
  src/ui/WaterfallItem.cpp
  src/ui/WaterfallItem.hpp
  src/ui/PanadapterItem.cpp
  src/ui/PanadapterItem.hpp
  src/ui/WorldMapItem.cpp
  src/ui/WorldMapItem.hpp
  src/ui/WorldMapGpuItem.cpp
  src/ui/WorldMapGpuItem.hpp
  src/bridge/DecodiumAudioSink.h
  src/rtl/RtlSdrDsp.h
  src/rtl/RtlSdrDsp.cpp
  src/rtl/RtlSdrAudioOutput.h
  src/rtl/RtlSdrAudioOutput.cpp
  src/rtl/RtlSdrCapabilities.h
  src/rtl/RtlSdrRfSpectrum.h
  src/rtl/RtlSdrRfSpectrum.cpp
  src/rtl/RtlSdrTuningPlan.h
  src/rtl/RtlSdrInput.h
  src/rtl/RtlSdrInput.cpp
  Detector/FT8DecodeWorker.cpp
  Detector/FT2DecodeWorker.cpp
  Detector/FT4DecodeWorker.cpp
  Detector/Q65DecodeWorker.cpp
  Detector/MSK144DecodeWorker.cpp
  Detector/WSPRDecodeWorker.cpp
  Detector/JT9FastDecodeWorker.cpp
  Detector/JT9FastDecoder.cpp
  Detector/JT9FastHelpers.cpp
  Detector/JT9WideDecoder.cpp
  Detector/LegacyJtDecodeWorker.cpp
  Detector/LegacyDspIoHelpers.cpp
  Detector/FftCompat.cpp
  Detector/FanoSequentialDecoder.cpp
  Detector/JT4Decoder.cpp
  Detector/JT9NarrowDecoder.cpp
  widgets/PlotLegacyHelpers.cpp
  lib/wsprd/wsprd_embedded.c
  lib/wsprd/osdwspr.c
  lib/wsprd/wsprsim_utils.c
  lib/wsprd/wsprd_utils.c
  lib/wsprd/fano.c
  lib/wsprd/jelinek.c
  lib/wsprd/tab.c
  lib/wsprd/nhash.c
  lib/init_random_seed.c
  Audio/soundin.cpp
  Audio/soundout.cpp
  Modulator/Modulator.cpp
  src/bridge/DecodiumQmlLegacyCompat.cpp
  Network/FoxVerifier.cpp
  Network/FoxVerifier.hpp
  Network/wsprnet.cpp
  Network/wsprnet.h
  Network/DecodiumWsprUploader.cpp
  Network/DecodiumWsprUploader.h
  Network/DecodiumPskReporterLite.cpp
  Network/DecodiumPskReporterLite.h
  Network/DecodiumCloudlogLite.cpp
  Network/DecodiumCloudlogLite.h
  Network/DecodiumQrzLogbookLite.cpp
  Network/DecodiumQrzLogbookLite.h
  src/services/DecodiumDxCluster.cpp
  src/services/DecodiumDxCluster.h
  src/services/DecodiumSpotShare.cpp
  src/services/DecodiumSpotShare.h
  src/services/DxccLookup.cpp
  src/services/DxccLookup.h
  src/services/CallsignIntelligenceService.cpp
  src/services/CallsignIntelligenceService.h
  ${DECODIUM_QML_LEGACY_SRCS}
  ${wsjtx_GENUISRCS}
  ${wsjtx_RESOURCES_RCC}
  ${decodium_qml_VERSION_RESOURCES}
)
if (WSJT_CREATE_WINMAIN)
  set_target_properties (decodium_qml PROPERTIES WIN32_EXECUTABLE ON)
endif ()
set_target_properties (decodium_qml PROPERTIES OUTPUT_NAME decodium)
set (DECODIUM_FT2LINK_ACCESS_SALT_B64 "$ENV{DECODIUM_FT2LINK_ACCESS_SALT_B64}" CACHE STRING "Base64 salt for sealed FT2-Link access gate")
set (DECODIUM_FT2LINK_ACCESS_HASH_B64 "$ENV{DECODIUM_FT2LINK_ACCESS_HASH_B64}" CACHE STRING "Base64 PBKDF2-HMAC-SHA256 hash for sealed FT2-Link access gate")
set (DECODIUM_FT2LINK_ACCESS_ITERATIONS "160000" CACHE STRING "PBKDF2 iterations for sealed FT2-Link access gate")
mark_as_advanced (DECODIUM_FT2LINK_ACCESS_SALT_B64 DECODIUM_FT2LINK_ACCESS_HASH_B64 DECODIUM_FT2LINK_ACCESS_ITERATIONS)
# Sopprime i warnings-as-error su LegacyDspIoHelpers (vector bounds in GCC15)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties (Detector/LegacyDspIoHelpers.cpp PROPERTIES
    COMPILE_FLAGS "-Wno-error=stringop-overflow -Wno-error=stringop-overread -Wno-error=array-bounds")
endif ()
# C17 — FoxVerifier senza Boost.Log (usa qDebug come shim)
target_compile_definitions (decodium_qml PRIVATE DECODIUM_NO_BOOST_LOG FFTW3_SINGLE_FOUND DECODIUM_QML_PANADAPTER_DIRECT=1)
if (DECODIUM_HAS_RTLSDR)
  target_compile_definitions (decodium_qml PRIVATE DECODIUM_HAS_RTLSDR=1)
else ()
  target_compile_definitions (decodium_qml PRIVATE DECODIUM_HAS_RTLSDR=0)
endif ()
target_compile_definitions (decodium_qml PRIVATE
  DECODIUM_FT2LINK_ACCESS_SALT_B64="${DECODIUM_FT2LINK_ACCESS_SALT_B64}"
  DECODIUM_FT2LINK_ACCESS_HASH_B64="${DECODIUM_FT2LINK_ACCESS_HASH_B64}"
  DECODIUM_FT2LINK_ACCESS_ITERATIONS=${DECODIUM_FT2LINK_ACCESS_ITERATIONS}
)
target_compile_options (decodium_qml PRIVATE -Wno-deprecated-declarations)
target_include_directories (decodium_qml PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${FFTW3_INCLUDE_DIRS}
)
target_link_libraries (decodium_qml
  Qt6::Quick
  Qt6::Qml
  Qt6::QuickControls2
  Qt6::Widgets
  Qt6::Multimedia
  Qt6::Network
  Qt6::SerialPort
  Qt6::Sql           # 1.0.238 Phase 5.2: decode history worker thread
  Qt6::Concurrent    # decode UI snapshot filter/sort off the GUI thread
  Qt6::WebSockets
  wsjt_qtmm
  wsjt_qt
  wsjt_fort
  wsjt_cxx
  Hamlib::Hamlib
  Boost::log
  ${FFTW3_LIBRARIES}
  ${LIBM_LIBRARIES}
)
if (TARGET Decodium::SstvIntegration)
  target_link_libraries (decodium_qml Decodium::SstvIntegration)
  target_link_libraries (decodium_qml Decodium::SstvGallery)
  target_link_libraries (decodium_qml Decodium::SstvSharingUi)
  target_link_libraries (decodium_qml Decodium::SstvDiagnostics)
  if (TARGET Decodium::SstvStudio)
    target_link_libraries (decodium_qml Decodium::SstvStudio)
  endif ()
  target_compile_definitions (decodium_qml PRIVATE DECODIUM_HAS_SSTV=1)
else ()
  target_compile_definitions (decodium_qml PRIVATE DECODIUM_HAS_SSTV=0)
endif ()


# DecoRTTY: la libreria di aggancio porta con se' tutte le altre.
if (DECODIUM_ENABLE_RTTY)
  target_link_libraries (decodium_qml decortty_host)
  target_compile_definitions (decodium_qml PRIVATE DECODIUM_HAS_RTTY=1)
else ()
  target_compile_definitions (decodium_qml PRIVATE DECODIUM_HAS_RTTY=0)
endif ()
if (TARGET Decodium::HamDrmRuntime)
  target_link_libraries (decodium_qml Decodium::HamDrmRuntime)
  target_compile_definitions (decodium_qml PRIVATE DECODIUM_HAS_HAMDRM=1)
else ()
  target_compile_definitions (decodium_qml PRIVATE DECODIUM_HAS_HAMDRM=0)
endif ()
if (DECODIUM_HAS_RTLSDR)
  target_link_libraries (decodium_qml RtlSdr::RtlSdr)
endif ()
if (OPENMP_FOUND)
  if (TARGET OpenMP::OpenMP_CXX)
    target_link_libraries (decodium_qml OpenMP::OpenMP_CXX)
  elseif (APPLE AND DECODIUM_LIBOMP_LIBRARY)
    target_link_libraries (decodium_qml ${DECODIUM_LIBOMP_LIBRARY})
  endif ()
endif ()
if (APPLE)
  target_link_libraries (decodium_qml Qt6::DBus "-framework AudioToolbox")
endif ()
if (TARGET Qt6::GuiPrivate)
  target_link_libraries (decodium_qml Qt6::GuiPrivate)
  target_compile_definitions (decodium_qml PRIVATE DECODIUM_QT_RHI_TEXTURE_UPLOAD=1)
  file (WRITE "${CMAKE_BINARY_DIR}/decodium_qt_rhi_texture_upload.enabled" "1\n")
  message (STATUS "DECODIUM_QT_RHI_TEXTURE_UPLOAD=1 enabled: Qt6::GuiPrivate found")
else ()
  file (WRITE "${CMAKE_BINARY_DIR}/decodium_qt_rhi_texture_upload.enabled" "0\n")
  message (WARNING "DECODIUM_QT_RHI_TEXTURE_UPLOAD disabled: Qt6::GuiPrivate not found, using legacy texture upload path")
endif ()
if (WIN32)
  target_link_libraries (decodium_qml Qt6::AxContainer Pdh)
endif ()
if (Qt6ShaderTools_FOUND)
	  qt_add_shaders(decodium_qml decodium_waterfall_shaders
	    PREFIX
	      "/shaders"
	    BASE
	      "shaders"
	    GLSL
	      "120,150,310 es,330,440"
    FILES
      shaders/waterfall_palette.vert
      shaders/waterfall_palette.frag
  )
	  qt_add_shaders(decodium_qml decodium_panadapter_fft_shader
	    PREFIX
	      "/shaders"
	    BASE
	      "shaders"
	    GLSL
	      "310 es,420,430,440"
    FILES
      shaders/panadapter_fft.comp
      shaders/panadapter_fft_direct.comp
  )
	  qt_add_shaders(decodium_qml decodium_panadapter_spectrum_shader
	    PREFIX
	      "/shaders"
	    BASE
	      "shaders"
	    GLSL
	      "120,150,310 es,330,440"
    FILES
      shaders/panadapter_spectrum.vert
      shaders/panadapter_spectrum.frag
  )
	  qt_add_shaders(decodium_qml decodium_panadapter_spectrum3d_shader
	    PREFIX
	      "/shaders"
	    BASE
	      "shaders"
	    GLSL
	      "120,150,310 es,330,440"
    FILES
      shaders/panadapter_spectrum3d.vert
      shaders/panadapter_spectrum3d.frag
  )
	  qt_add_shaders(decodium_qml decodium_livemap_greyline_shader
	    PREFIX
	      "/shaders"
	    BASE
	      "shaders"
	    GLSL
	      "120,150,310 es,330,440"
    FILES
      shaders/livemap_greyline.vert
      shaders/livemap_greyline.frag
  )
	  qt_add_shaders(decodium_qml decodium_livemap_marker_shader
	    PREFIX
	      "/shaders"
	    BASE
	      "shaders"
	    GLSL
	      "120,150,310 es,330,440"
    FILES
      shaders/livemap_marker.vert
      shaders/livemap_marker.frag
  )
	  qt_add_shaders(decodium_qml decodium_livemap_path_shader
	    PREFIX
	      "/shaders"
	    BASE
	      "shaders"
	    GLSL
	      "120,150,310 es,330,440"
    FILES
      shaders/livemap_path.vert
      shaders/livemap_path.frag
  )
	  qt_add_shaders(decodium_qml decodium_livemap_arrow_shader
	    PREFIX
	      "/shaders"
	    BASE
	      "shaders"
	    GLSL
	      "120,150,310 es,330,440"
    FILES
      shaders/livemap_arrow.vert
      shaders/livemap_arrow.frag
  )
  target_compile_definitions(decodium_qml PRIVATE DECODIUM_WATERFALL_SHADER_QSB=1)
  target_compile_definitions(decodium_qml PRIVATE DECODIUM_GPU_PANADAPTER_FFT_QSB=1)
  target_compile_definitions(decodium_qml PRIVATE DECODIUM_GPU_PANADAPTER_SPECTRUM_QSB=1)
  target_compile_definitions(decodium_qml PRIVATE DECODIUM_GPU_PANADAPTER_SPECTRUM_3D_QSB=1)
  target_compile_definitions(decodium_qml PRIVATE DECODIUM_LIVEMAP_GREYLINE_QSB=1)
  target_compile_definitions(decodium_qml PRIVATE DECODIUM_LIVEMAP_MARKER_QSB=1)
  target_compile_definitions(decodium_qml PRIVATE DECODIUM_LIVEMAP_PATH_QSB=1)
  target_compile_definitions(decodium_qml PRIVATE DECODIUM_LIVEMAP_ARROW_QSB=1)
  file (WRITE "${CMAKE_BINARY_DIR}/decodium_qsb_shaders.enabled" "1\n")
else ()
  file (WRITE "${CMAKE_BINARY_DIR}/decodium_qsb_shaders.enabled" "0\n")
  message (STATUS "Qt6 ShaderTools not found: Decodium waterfall shader disabled, CPU palette fallback will be used")
endif ()
# FT8 features benchmark — misura overhead/recovery di Turbo Feedback + Neural Sync
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/bench_ft8_features.cpp")
  add_executable (bench_ft8_features tools/bench_ft8_features.cpp)
  target_link_libraries (bench_ft8_features
    Qt6::Core
    wsjt_qt
    wsjt_fort
    wsjt_cxx
    Hamlib::Hamlib
    Boost::log
    ${FFTW3_LIBRARIES}
    ${LIBM_LIBRARIES}
  )
else ()
  message (STATUS "tools/bench_ft8_features.cpp not found: skipping optional FT8 features benchmark")
endif ()

# Copy a clean QML tree next to the decodium_qml executable on every build.
# In particular, remove stale qmlcachegen artifacts left by older builds:
# Qt prefers adjacent .qmlc files over the updated source QML.
# Quando si compila dentro la cartella dei sorgenti, origine e destinazione
# sono la stessa cosa: questo passo cancellava i .qml e poi provava a
# ricopiarli da se stessi, cioe' li perdeva. Chi compilava in-source si
# ritrovava settantaquattro file spariti e nessun indizio sul perche'.
if (NOT "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  add_custom_target (sync_decodium_qml ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory
      "${CMAKE_BINARY_DIR}/qml"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/qml"
      "${CMAKE_BINARY_DIR}/qml"
    COMMENT "Syncing QML files to build output"
  )
else ()
  # I file sono gia' al loro posto: non c'e' niente da copiare.
  add_custom_target (sync_decodium_qml ALL
    COMMENT "QML gia' in posizione (compilazione dentro i sorgenti)"
  )
endif ()

add_custom_target (sync_decodium_assets ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "${CMAKE_BINARY_DIR}/artwork/maps"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/artwork/maps/earth_2048x1024.jpg"
    "${CMAKE_BINARY_DIR}/artwork/maps/earth_2048x1024.jpg"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/artwork/maps/world_overlay_2048x1024.png"
    "${CMAKE_BINARY_DIR}/artwork/maps/world_overlay_2048x1024.png"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/sounds"
    "${CMAKE_BINARY_DIR}/sounds"
  COMMENT "Syncing Decodium runtime artwork and sounds"
)

add_dependencies (decodium_qml sync_decodium_qml sync_decodium_assets)
add_custom_target (decodium_app
  DEPENDS decodium_qml sync_decodium_qml sync_decodium_assets
)
# Stessa trappola gia' disinnescata per sync_decodium_qml qui sopra: in una
# compilazione dentro i sorgenti CMAKE_BINARY_DIR e la cartella dei sorgenti
# coincidono, e questo passo cancellava qml/ per poi ricopiarla da se stessa,
# cioe' cancellava gli 88 .qml del progetto a ogni build di decodium_qml.
if (NOT "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  add_custom_command (TARGET decodium_qml POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
      "${CMAKE_BINARY_DIR}/qml"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/qml"
      "${CMAKE_BINARY_DIR}/qml"
    COMMENT "Syncing QML files after decodium_qml build"
  )
endif ()

add_custom_command (TARGET decodium_qml POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<TARGET_FILE_DIR:decodium_qml>/artwork/maps"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/artwork/maps/earth_2048x1024.jpg"
    "$<TARGET_FILE_DIR:decodium_qml>/artwork/maps/earth_2048x1024.jpg"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/artwork/maps/world_overlay_2048x1024.png"
    "$<TARGET_FILE_DIR:decodium_qml>/artwork/maps/world_overlay_2048x1024.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${DECODIUM_RUNTIME_DATA_DIR}/sat.dat"
    "$<TARGET_FILE_DIR:decodium_qml>/sat.dat"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${DECODIUM_RUNTIME_DATA_DIR}/cty.dat"
    "$<TARGET_FILE_DIR:decodium_qml>/cty.dat"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/sounds"
    "$<TARGET_FILE_DIR:decodium_qml>/sounds"
  COMMENT "Syncing QML files, artwork, and sounds after decodium_qml build"
)

if (APPLE)
  add_custom_command (TARGET wsjtx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
      $<TARGET_FILE_DIR:wsjtx>/sounds
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${DECODIUM_RUNTIME_DATA_DIR}/cty.dat"
      "$<TARGET_FILE_DIR:wsjtx>/../Resources/cty.dat"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${DECODIUM_RUNTIME_DATA_DIR}/sat.dat"
      "$<TARGET_FILE_DIR:wsjtx>/../Resources/sat.dat"
    COMMENT "Refreshing macOS bundle data files"
    )
endif ()

# make a library for WSJT-X UDP servers
# add_library (wsjtx_udp SHARED ${UDP_library_CXXSRCS})
add_library (wsjtx_udp-static STATIC ${UDP_library_CXXSRCS})
#target_include_directories (wsjtx_udp
#  INTERFACE
#  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/wsjtx>
#  )
target_include_directories (wsjtx_udp-static
  INTERFACE
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/wsjtx>
  )
#set_target_properties (wsjtx_udp PROPERTIES
#  PUBLIC_HEADER "${UDP_library_HEADERS}"
#  )
set_target_properties (wsjtx_udp-static PROPERTIES
  OUTPUT_NAME wsjtx_udp
  )
target_compile_definitions (wsjtx_udp-static PUBLIC UDP_STATIC_DEFINE)
target_link_libraries (wsjtx_udp-static Qt6::Network Qt6::Gui)
generate_export_header (wsjtx_udp-static BASE_NAME udp)

generate_version_info (udp_daemon_VERSION_RESOURCES
  NAME udp_daemon
  BUNDLE ${PROJECT_BUNDLE_NAME}
  ICON ${WSJTX_ICON_FILE}
  FILE_DESCRIPTION "Example WSJT-X UDP Message Protocol daemon"
  )
add_executable (udp_daemon UDPExamples/UDPDaemon.cpp ${udp_daemon_VERSION_RESOURCES})
target_link_libraries (udp_daemon wsjtx_udp-static)

generate_version_info (wsjtx_app_version_VERSION_RESOURCES
  NAME wsjtx_app_version
  BUNDLE ${PROJECT_BUNDLE_NAME}
  ICON ${WSJTX_ICON_FILE}
  FILE_DESCRIPTION "Display WSJT-X Application Version on console"
  )
add_executable (wsjtx_app_version AppVersion/AppVersion.cpp ${wsjtx_app_version_VERSION_RESOURCES})
target_link_libraries (wsjtx_app_version wsjt_qt)

generate_version_info (message_aggregator_VERSION_RESOURCES
  NAME message_aggregator
  BUNDLE ${PROJECT_BUNDLE_NAME}
  ICON ${WSJTX_ICON_FILE}
  FILE_DESCRIPTION "Example WSJT-X UDP Message Protocol application"
  )
add_resources (message_aggregator_RESOURCES /qss ${message_aggregator_STYLESHEETS})
configure_file (UDPExamples/message_aggregator.qrc.in message_aggregator.qrc @ONLY)
qt_add_resources (message_aggregator_RESOURCES_RCC
  ${CMAKE_CURRENT_BINARY_DIR}/message_aggregator.qrc
  contrib/QDarkStyleSheet/qdarkstyle/style.qrc
  )
add_executable (message_aggregator
  ${message_aggregator_CXXSRCS}
  ${message_aggregator_RESOURCES_RCC}
  ${message_aggregator_VERSION_RESOURCES}
  )
target_link_libraries (message_aggregator wsjt_qt Qt6::Widgets wsjtx_udp-static)
target_compile_options (message_aggregator PRIVATE -Wno-deprecated-declarations)

if (WSJT_CREATE_WINMAIN)
  set_target_properties (message_aggregator PROPERTIES WIN32_EXECUTABLE ON)
endif (WSJT_CREATE_WINMAIN)

if (UNIX)
  if (NOT WSJT_SKIP_MANPAGES)
    add_subdirectory (manpages)
    add_dependencies (wsjtx manpages)
  endif (NOT WSJT_SKIP_MANPAGES)
  if (NOT APPLE)
    add_subdirectory (debian)
    add_dependencies (wsjtx debian)
  endif (NOT APPLE)
endif (UNIX)

#
# installation
#
install (TARGETS wsjtx
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  BUNDLE DESTINATION . COMPONENT runtime
  )

install (TARGETS decodium_qml
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  BUNDLE DESTINATION . COMPONENT runtime
  )

install (DIRECTORY
  ${PROJECT_SOURCE_DIR}/qml
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  PATTERN "._*" EXCLUDE
  PATTERN ".DS_Store" EXCLUDE
  #COMPONENT runtime
  )

# install (TARGETS wsjtx_udp EXPORT udp
#   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wsjtx
#   )
# install (TARGETS wsjtx_udp-static EXPORT udp-static
#   DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   )

# install (EXPORT udp NAMESPACE wsjtx::
#   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wsjtx
#   )
# install (EXPORT udp-static NAMESPACE wsjtx::
#   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wsjtx
#   )

install (TARGETS udp_daemon message_aggregator wsjtx_app_version
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  )

install (TARGETS wsprd
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  )

if(WSJT_BUILD_UTILS)
  set(_wsjt_optional_install_targets)
  foreach(_wsjt_optional_target IN ITEMS jt9 fmtave fcal fmeasure)
    if(TARGET ${_wsjt_optional_target})
      list(APPEND _wsjt_optional_install_targets ${_wsjt_optional_target})
    endif()
  endforeach()
  if(_wsjt_optional_install_targets)
    install (TARGETS ${_wsjt_optional_install_targets}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
      BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
      )
  endif()
endif(WSJT_BUILD_UTILS)

install (PROGRAMS
  ${RIGCTL_EXE}
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  #COMPONENT runtime
  RENAME rigctl-wsjtx${CMAKE_EXECUTABLE_SUFFIX}
  )

install (PROGRAMS
  ${RIGCTLD_EXE}
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  #COMPONENT runtime
  RENAME rigctld-wsjtx${CMAKE_EXECUTABLE_SUFFIX}
  )

install (PROGRAMS
  ${RIGCTLCOM_EXE}
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  #COMPONENT runtime
  RENAME rigctlcom-wsjtx${CMAKE_EXECUTABLE_SUFFIX}
  )

install (FILES
  README.md
  COPYING
  AUTHORS
  doc/project/THANKS
  doc/RTL_SDR.md
  doc/THIRD_PARTY_LICENSES_RTLSDR.md
  doc/THIRD_PARTY_LICENSES_OPENJPEG.md
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  #COMPONENT runtime
  )

if (DECODIUM_ENABLE_SSTV)
  install (DIRECTORY "${PROJECT_SOURCE_DIR}/docs/sstv/"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/sstv"
    FILES_MATCHING
      PATTERN "*.md"
      PATTERN "*.yaml"
    #COMPONENT runtime
    )
  install (FILES
    "${PROJECT_SOURCE_DIR}/tests/sstv/fixtures/EXTERNAL_VECTORS.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/sstv"
    #COMPONENT runtime
    )
endif ()

install (FILES
  "${DECODIUM_RUNTIME_DATA_DIR}/cty.dat"
  "${DECODIUM_RUNTIME_DATA_DIR}/CALL3.TXT"
  "${DECODIUM_RUNTIME_DATA_DIR}/cty.dat_copyright.txt"
  "${DECODIUM_RUNTIME_DATA_DIR}/grid.dat"
  "${DECODIUM_RUNTIME_DATA_DIR}/sat.dat"
  contrib/Ephemeris/JPLEPH
  "${DECODIUM_RUNTIME_DATA_DIR}/eclipse.txt"
  artwork/maps/earth_2048x1024.jpg
  artwork/maps/world_overlay_2048x1024.png
  DESTINATION ${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}
  #COMPONENT runtime
  )

install (FILES
  tools/DXpedCertManager.py
  tools/dxped_manage.py
  tools/dxped_sign_list.py
  tools/generate_dxcert.py
  tools/dxped_public.pem
  DESTINATION ${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}/tools
  #COMPONENT runtime
  )

if (APPLE)
  install (FILES
    "${DECODIUM_RUNTIME_DATA_DIR}/ALLCALL7.TXT"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}
    #COMPONENT runtime
    )
else ()
  install (FILES
    "${DECODIUM_RUNTIME_DATA_DIR}/ALLCALL7.TXT"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    #COMPONENT runtime
    )
endif ()

install (DIRECTORY
  example_log_configurations
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  FILES_MATCHING REGEX "^.*[^~]$"
  #COMPONENT runtime
  )

if (APPLE)
  install (DIRECTORY
    ${PROJECT_SOURCE_DIR}/sounds
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}
    #COMPONENT runtime
    )
else ()
  install (DIRECTORY
    ${PROJECT_SOURCE_DIR}/sounds
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    #COMPONENT runtime
    )
endif ()

if (WIN32)
  install (FILES
    contrib/tls/libssl-1_1.dll
    contrib/tls/libcrypto-1_1.dll
    contrib/tls/libssl-1_1-x64.dll
    contrib/tls/libcrypto-1_1-x64.dll
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    #COMPONENT runtime
  )
endif (WIN32)

#
# Mac installer files
#
if (APPLE)
    install (FILES
        Darwin/ReadMe.txt
        DESTINATION .
        #COMPONENT runtime
    )
endif (APPLE)

#
# uninstall support
#
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  @ONLY)
add_custom_target (uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")


# creates or updates ${PROJECT_BINARY_DIR}/scs_version.h using cmake script
add_custom_target (revisiontag
  COMMAND ${CMAKE_COMMAND}
  -D SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
  -D BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
  -D OUTPUT_DIR=${PROJECT_BINARY_DIR}
  -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake/getsvn.cmake
  VERBATIM
  BYPRODUCTS scs_version.h
  COMMENT "Getting source control system revision information"
  )
# explicitly say that the wsjt_qt depends on custom target, this is
# done indirectly so that the revisiontag target gets built exactly
# once per build
add_dependencies (wsjt_qt revisiontag)


#
# versioning and configuration
#
configure_file (
  "${DECODIUM_CMAKE_TEMPLATE_DIR}/decodium_config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/wsjtx_config.h"
  )


if (NOT WIN32 AND NOT APPLE)
  # install a desktop file so wsjtx appears in the application start
  # menu with an icon
  install (
    FILES
      "${DECODIUM_PACKAGING_DIR}/linux/wsjtx.desktop"
      "${DECODIUM_PACKAGING_DIR}/linux/message_aggregator.desktop"
    DESTINATION share/applications
    #COMPONENT runtime
    )
  install (
    FILES icons/Unix/decodium_icon.png
    DESTINATION share/pixmaps
    #COMPONENT runtime
    )
endif (NOT WIN32 AND NOT APPLE)

if (APPLE)
  set (CMAKE_POSTFLIGHT_SCRIPT
    "${wsjtx_BINARY_DIR}/postflight.sh")
  set (CMAKE_POSTUPGRADE_SCRIPT
    "${wsjtx_BINARY_DIR}/postupgrade.sh")
  configure_file ("${wsjtx_SOURCE_DIR}/Darwin/postflight.sh.in"
    "${CMAKE_POSTFLIGHT_SCRIPT}")
  configure_file ("${wsjtx_SOURCE_DIR}/Darwin/postupgrade.sh.in"
    "${CMAKE_POSTUPGRADE_SCRIPT}")
endif ()


#
# bundle fixup only done in non-Debug configurations
#
if (NOT is_debug_build)
  # add this sub-sirectory after all install steps and other
  # sub-directories to ensure that all executables are in-place before
  # any fixup is done
  add_subdirectory (bundle_fixup)
endif ()


#
# packaging
#
set (CPACK_PACKAGE_DESCRIPTION_FILE
  "${DECODIUM_PACKAGING_DIR}/package_description.txt")
set (CPACK_MONOLITHIC_INSTALL 1)
set (CPACK_PACKAGE_NAME "decodium")
set (CPACK_PACKAGE_VERSION_MAJOR ${FORK_RELEASE_VERSION_MAJOR})
set (CPACK_PACKAGE_VERSION_MINOR ${FORK_RELEASE_VERSION_MINOR})
set (CPACK_PACKAGE_VERSION_PATCH ${FORK_RELEASE_VERSION_PATCH})

if (WIN32)
  set (CPACK_GENERATOR "NSIS")
elseif (APPLE)
  set (CPACK_GENERATOR "DragNDrop")
else ()
  find_program (DPKG_BUILDER dpkg-buildpackage DOC "Debian package builder")
  if (DPKG_BUILDER)
    #
    # Derive the correct filename for a Debian package because the DEB
    # generator doesn't do this correctly at present.
    #
    find_program (DPKG_PROGRAM dpkg DOC "dpkg program of Debian-based systems")
    if (DPKG_PROGRAM)
      execute_process (
	COMMAND ${DPKG_PROGRAM} --print-architecture
	OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
	OUTPUT_STRIP_TRAILING_WHITESPACE
	)
    else (DPKG_PROGRAM)
      set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE noarch)
    endif (DPKG_PROGRAM)

    list (APPEND CPACK_GENERATOR "DEB")
  endif (DPKG_BUILDER)

  find_program (RPMBUILDER rpmbuild DOC "RPM package builder")
  if (RPMBUILDER)
    list (APPEND CPACK_GENERATOR "RPM")
  endif (RPMBUILDER)
endif ()

set (CPACK_DEBIAN_PACKAGE_HOMEPAGE "${PROJECT_HOMEPAGE}")
set (CPACK_DEBIAN_PACKAGE_DEPENDS "libgfortran5 (>=8.3) | libgfortran4 (>=7.3) | libgfortran3 (>=6.3), libfftw3-single3 (>=3.3), libgomp1 (>=6), libqt5serialport5 (>=5.7), libqt5multimedia5-plugins (>=5.7), libqt5widgets5 (>=5.7), libqt5network5 (>=5.7), libqt5printsupport5 (>=5.7), libqt5websockets5 (>=5.7),  libqt5sql5-sqlite (>=5.7), libusb-1.0-0 (>=1.0.21), libboost-log1.62.0 (>=1.62.0) | libboost-log1.65.1 (>=1.65.1) | libboost-log1.67.0 (>=1.67.0) | libboost-log1.71.0 (>=1.71.0) | libboost-log1.74.0 (>=1.74.0)")
set (CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

set (CPACK_RPM_PACKAGE_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
set (CPACK_RPM_PACKAGE_REQUIRES "qt5-qtbase >= 5.9, qt5-qtserialport >= 5.9, qt5-qtmultimedia >= 5.9, qt5-qtwebsockets >= 5.9, qt5-qtsvg >= 5.9, libusbx >= 1.0.22, libgfortran >= 7, libgomp >= 7, fftw-libs-single >= 3.3, boost-log >= 1.62")
set (CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION /usr/share/pixmaps /usr/share/applications /usr/share/man /usr/share/man1)

configure_file ("${DECODIUM_CMAKE_TEMPLATE_DIR}/CMakeCPackOptions.cmake.in"
  "${PROJECT_BINARY_DIR}/CMakeCPackOptions.cmake" @ONLY)
set (CPACK_PROJECT_CONFIG_FILE "${PROJECT_BINARY_DIR}/CMakeCPackOptions.cmake")

include (CPack)
