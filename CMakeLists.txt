cmake_minimum_required (VERSION 3.7.2 FATAL_ERROR)

if (APPLE)
  #
  # The following variables define the portability and compatability attributes of the Mac macOS build
  # they are choosen with care and should not be changed without good cause.
  #
  # Among other things these options are chosen to match the portability and compatability options of the
  # Qt framework dylibs which can be checked as follows:
  #
  # otool -l <binary> | grep -A3 LC_VERSION_MIN_MACOSX
  #
  set (CMAKE_OSX_DEPLOYMENT_TARGET 10.12
    CACHE STRING "Earliest version of macOS supported

Earliest version we can support with Qt 5.12, C++11 & libc++ is 10.12.
Do not override this if you intend to build an official deployable installer.")
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
  LANGUAGES C CXX Fortran
  )
set (PROJECT_DESCRIPTION "Decodium 4.0 Core Shannon: Digital Modes for Weak Signal Communications in Amateur Radio")
set (CMAKE_PROJECT_DESCRIPTION ${PROJECT_DESCRIPTION})
set (CMAKE_AUTOUIC ON)
set (WSJT_APP_BUNDLE_NAME "ft2")
set (WSJT_APP_EXECUTABLE_NAME "ft2")
include (CTest)

#
# Local CMake modules and support files
#
set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake/Modules ${CMAKE_MODULE_PATH})

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
set (PROJECT_BUNDLE_NAME "Decodium 4.0 Core Shannon")
set (PROJECT_VENDOR "IU8LMC")
set (PROJECT_CONTACT "IU8LMC <qrz.com/db/IU8LMC>")
set (PROJECT_COPYRIGHT "See Help | Copyright info")
set (PROJECT_HOMEPAGE qrz.com/db/IU8LMC)
set (PROJECT_MANUAL wsjtx-main)
set (PROJECT_MANUAL_DIRECTORY_URL https://wsjt.sourceforge.io/wsjtx-doc/)
set (PROJECT_SAMPLES_URL http://downloads.sourceforge.net/project/wsjt/)
set (PROJECT_SAMPLES_UPLOAD_DEST frs.sourceforge.net:/home/frs/project/wsjt/)

# make sure that the default configuration is a RELEASE
if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE RELEASE CACHE STRING
    "Choose the type of build, options are: None Debug Release."
    FORCE)
endif (NOT CMAKE_BUILD_TYPE)
if (CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
  set (is_debug_build 1)
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
    "${CMAKE_BINARY_DIR}/CMakeFiles/ft2.dir"
    "${CMAKE_BINARY_DIR}/CMakeFiles/ft2_autogen.dir"
    "${CMAKE_BINARY_DIR}/CMakeFiles/ft2_autogen_timestamp_deps.dir"
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

set (WSJT_PLUGIN_DESTINATION ${PLUGIN_DESTINATION} CACHE PATH "Path for plugins")
set (WSJT_QT_CONF_DESTINATION ${QT_CONF_DESTINATION} CACHE PATH "Path for the qt.conf file")

if (WSJT_FOX_OTP)
  set (wsjt_fox_CXXSRCS
  )
  message (STATUS "Including Fox verification code feature")
endif ()

#
# Project sources
#
set (fort_qt_CXXSRCS
  lib/shmem.cpp
  )

set (wsjt_qt_CXXSRCS
  helper_functions.cpp
  SecureSettings.cpp
  qt_helpers.cpp
  widgets/MessageBox.cpp
  MetaDataRegistry.cpp
  Network/NetworkServerLookup.cpp
  revision_utils.cpp
  L10nLoader.cpp
  WFPalette.cpp
  Radio.cpp
  RadioMetaType.cpp
  NonInheritingProcess.cpp
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
  GetUserId.cpp
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
  SampleDownloader.cpp
  SampleDownloader/DirectoryDelegate.cpp
  SampleDownloader/Directory.cpp
  SampleDownloader/FileNode.cpp
  SampleDownloader/RemoteFile.cpp
  DisplayManual.cpp
  MultiSettings.cpp
  validators/MaidenheadLocatorValidator.cpp
  validators/CallsignValidator.cpp
  widgets/SplashScreen.cpp
  EqualizationToolsDialog.cpp
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
  widgets/LazyFillComboBox.cpp
  widgets/CheckableItemComboBox.cpp
  widgets/BandComboBox.cpp
  Detector/FtxFt2Stage7.cpp
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
  otpgenerator.cpp
  )
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties (Detector/LegacyDspIoHelpers.cpp PROPERTIES COMPILE_FLAGS "-Wno-error=stringop-overflow -Wno-error=stringop-overread")
endif ()

set (wsjt_qtmm_CXXSRCS
  Audio/BWFFile.cpp
  )

set (wsjtx_CXXSRCS
  WSJTXLogging.cpp
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
  lib/wsprd/wsprsim_utils.c
  lib/wsprd/wsprd_utils.c
  lib/wsprd/jelinek.c
  widgets/logqso.cpp
  widgets/displaytext.cpp
  Decoder/decodedtext.cpp
  getfile.cpp
  Audio/soundout.cpp
  Audio/soundin.cpp
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
  DecodiumCertificate.cpp
  WSPR/WsprTxScheduler.cpp
  widgets/mainwindow.cpp
  Configuration.cpp
  main.cpp
  DecodiumBridge.cpp
  DecodiumBridge.h
  DecodiumAudioSink.h
  controllers/ActiveStationsModel.cpp
  controllers/ActiveStationsModel.hpp
  DecodiumThemeManager.cpp
  DecodiumThemeManager.h
  DecodiumSubManagers.cpp
  DecodiumSubManagers.h
  DecodiumCatManager.cpp
  DecodiumCatManager.h
  DecodiumOmniRigManager.cpp
  DecodiumOmniRigManager.h
  DecodiumTransceiverManager.cpp
  DecodiumTransceiverManager.h
  DecodiumAlertManager.cpp
  DecodiumAlertManager.h
  Network/FoxVerifier.cpp
  Network/FoxVerifier.hpp
  Network/DecodiumWsprUploader.cpp
  Network/DecodiumWsprUploader.h
  Network/DecodiumPskReporterLite.cpp
  Network/DecodiumPskReporterLite.h
  Network/DecodiumCloudlogLite.cpp
  Network/DecodiumCloudlogLite.h
  DecodiumDxCluster.cpp
  DecodiumDxCluster.h
  DxccLookup.cpp
  DxccLookup.h
  WsjtxLegacyLinkCompat.cpp
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
  Logger.cpp
  lib/crc10.cpp
  lib/crc13.cpp
  lib/crc14.cpp
  Modulator/WattersonChannel.cpp
  Detector/Fil4Filter.cpp
  # PctileCompat: pctile_ already defined in FtxSharedDsp.cpp
  Detector/FtxBaseline.cpp
  Detector/FtxCandidates.cpp
  Detector/FtxBitmetrics.cpp
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
# deal with a GCC v6 UB error message
set_source_files_properties (
  lib/crc10.cpp
  lib/crc13.cpp
  lib/crc14.cpp
  PROPERTIES COMPILE_FLAGS -fpermissive)

# Windows-specific helper sources intentionally excluded in this fork.

set (wsjt_FSRCS
  # put module sources first in the hope that they get rebuilt before use
  lib/types.f90
  lib/C_interface_module.f90
  lib/shmem.f90
  lib/crc.f90
  lib/fftw3mod.f90
  lib/hashing.f90
  lib/iso_c_utilities.f90
  lib/jt4.f90
  lib/jt4_decode.f90
  lib/jt65_mod.f90
  lib/jt65_mod6.f90 #ft8md
  # lib/get_q3list.f90 — replaced by Detector/AstroQ3ListCompat.cpp
  lib/jt9_decode.f90
  lib/options.f90
  lib/packjt.f90
  lib/normalizebmet.f90
  lib/readwav.f90
  lib/wsjtx_wav_catalog.f90
  lib/timer_C_wrapper.f90
  lib/timer_impl.f90
  lib/timer_module.f90
  lib/wavhdr.f90

  # remaining non-module sources
  lib/addit.f90
  lib/afc65b.f90
  lib/afc9.f90
  lib/ana932.f90
  lib/analytic.f90
  lib/astro.f90
  lib/astrosub.f90
  lib/astro0.f90
  lib/avecho.f90
  lib/averms.f90
  lib/azdist.f90
  lib/bpdecode40.f90
  lib/bpdecode128_90.f90
  lib/baddata.f90
  lib/calibrate.f90
  lib/ccf2.f90
  lib/ccf65.f90
  lib/chkcall.f90
  lib/chkhist.f90
  lib/chkmsg.f90
  lib/chkss2.f90
  lib/coord.f90
  lib/db.f90
  lib/decode4.f90
  lib/decode9w.f90
  lib/decode_echo.f90
  lib/env_module.f90
  lib/deep4.f90
  lib/deg2grid.f90
  lib/degrade_snr.f90
  lib/downsam9.f90
  lib/echo_snr.f90
  # lib/encode232.f90 — replaced by Detector/Conv232Compat.cpp
  lib/encode4.f90
  lib/encode_128_90.f90
  lib/entail.f90
  lib/ephem.f90
  lib/extract4.f90
  lib/extractmessage77.f90
  # lib/fano232.f90 — replaced by Detector/Conv232Compat.cpp
  lib/fast9.f90
  lib/fast_decode.f90
  lib/fchisq.f90
  lib/fchisq0.f90
  lib/fchisq65.f90
  lib/fil3.f90
  lib/fil3c.f90
  # lib/fil4.f90 — replaced by Detector/Fil4Filter.cpp
  lib/fil6521.f90
  lib/filbig.f90
  lib/fitcal.f90
  lib/flat1.f90
  lib/flat1a.f90
  lib/flat1b.f90
  lib/flat2.f90
  lib/flat4.f90
  lib/flat65.f90
  lib/fmtmsg.f90
  lib/foldspec9f.f90
  lib/fspread_lorentz.f90
#  lib/ft8/foxgen_wrap.f90
  lib/freqcal.f90
  lib/gen4.f90
  lib/gen65.f90
  lib/gen9.f90
  lib/gen_cw_wave.f90
  lib/gen_echocall.f90
  lib/genwave.f90
  # lib/genwspr.f90 — replaced by Modulator/WsprLegacyCompat.cpp
  lib/geodist.f90
  lib/getlags.f90
  lib/getmet4.f90
  lib/gfsk_pulse.f90
  lib/graycode.f90
  lib/grayline.f90
  lib/grid2deg.f90
  lib/hash.f90
  lib/hint65.f90
  lib/hspec.f90
  # lib/indexx.f90 — replaced by Detector/IndexSortCompat.cpp
  lib/init_random_seed.f90
  lib/interleave4.f90
  lib/interleave9.f90
  # lib/inter_wspr.f90 — replaced by Modulator/WsprLegacyCompat.cpp
  lib/jplsubs.f
  lib/jt9fano.f90
  lib/libration.f90
  lib/lorentzian.f90
  lib/lpf1.f90
  lib/mixlpf.f90
  lib/makepings.f90
  lib/moondopjpl.f90
  lib/morse.f90
  lib/move.f90
  lib/nuttal_window.f90
  lib/legacy_jt_async_decode.f90
  lib/77bit/my_hash.f90
  lib/wsprd/osdwspr.f90
  lib/osd128_90.f90
  lib/peakdt9.f90
  lib/peakup.f90
  lib/plotsave.f90
  lib/platanh.f90
  lib/pltanh.f90
  lib/prog_args.f90
  lib/ps4.f90
  lib/refspectrum.f90
  lib/savec2.f90
  lib/save_echo_params.f90
  lib/sec0.f90
  lib/sec_midn.f90
  lib/sh65.f90
  lib/sh65snr.f90
  lib/slasubs.f
  lib/sleep_msec.f90
  lib/slope.f90
  lib/smo.f90
  lib/smo121.f90
  lib/softsym.f90
  lib/softsym9f.f90
  lib/softsym9w.f90
  lib/spec9f.f90
  lib/subtract65.f90
  lib/sun.f90
  lib/symspec.f90
  lib/symspec2.f90
  lib/sync4.f90
  lib/sync9.f90
  lib/sync9f.f90
  lib/sync9w.f90
  lib/timf2.f90
  lib/tweak1.f90
  lib/twkfreq.f90
  lib/twkfreq65.f90
  lib/update_recent_calls.f90
  lib/wav11.f90
  lib/wav12.f90
  lib/xcor4.f90
  lib/wqdecode.f90
  # lib/wqencode.f90 — replaced by Modulator/WsprLegacyCompat.cpp
  lib/wspr_downsample.f90
  lib/zplot9.f90
  )

# temporary workaround for a gfortran v7.3 ICE on Fedora 27 64-bit
set_source_files_properties (lib/slasubs.f PROPERTIES COMPILE_FLAGS -O2)

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
  wf_palette_design_dialog.ui
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
  Configuration.ui
  widgets/ExportCabrillo.ui
  widgets/QSYMessage.ui
  widgets/QSYMessageCreator.ui
  widgets/qsymonitor.ui
  widgets/TimeSyncPanel.ui
  )

set (UDP_library_CXXSRCS
  Radio.cpp
  RadioMetaType.cpp
  Network/NetworkMessage.cpp
  UDPExamples/MessageServer.cpp
  )

set (UDP_library_HEADERS
  Radio.hpp
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
  icons/Darwin/wsjtx.iconset/icon_128x128.png
  artwork/ft2logo.png
  contrib/gpl-v3-logo.svg
  artwork/splash.png
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
  set (ICONSRCS
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_16x16.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_16x16@2x.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_32x32.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_32x32@2x.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_128x128.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_128x128@2x.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_256x256.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_256x256@2x.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_512x512.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_512x512@2x.png
    )
  add_custom_command (
    OUTPUT ${WSJTX_ICON_FILE}
    COMMAND iconutil -c icns --output "${CMAKE_BINARY_DIR}/${WSJTX_ICON_FILE}" "${CMAKE_SOURCE_DIR}/icons/Darwin/${CMAKE_PROJECT_NAME}.iconset"
    DEPENDS ${ICONSRCS}
    COMMENT "Building Icons"
    )
else ()
  set (WSJTX_ICON_FILE icons/windows-icons/wsjtx.ico)
endif (APPLE)

set_source_files_properties (${WSJTX_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

# suppress intransigent compiler diagnostics
set_source_files_properties (lib/filbig.f90 PROPERTIES COMPILE_FLAGS "-Wno-aliasing")

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

check_type_size (CACHE_ALL HAMLIB_OLD_CACHING)
check_symbol_exists (rig_set_cache_timeout_ms "hamlib/rig.h" HAVE_HAMLIB_CACHING)
check_symbol_exists (rig_get_conf2 "hamlib/rig.h" HAVE_HAMLIB_GET_CONF2)

find_package (Usb REQUIRED)

if (WSJT_FOX_OTP)
  add_definitions (-DFOX_OTP)
endif ()

#
# Qt6 setup
#

# Widgets finds its own dependencies.
find_package (Qt6 COMPONENTS Widgets SerialPort Multimedia PrintSupport Sql LinguistTools WebSockets Test Core5Compat REQUIRED)
find_package (Qt6 COMPONENTS Qml Quick QuickControls2 REQUIRED)

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
set (CMAKE_Fortran_VISIBILITY_PRESET hidden)
set (CMAKE_VISIBILITY_INLINES_HIDDEN ON)
#set (CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)


#
# C & C++ setup
#
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -fexceptions -frtti")

if (NOT APPLE)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pragmas")
  if (${OPENMP_FOUND})
    if (OpenMP_C_FLAGS)
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_C_FLAGS}")
      set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    endif ()
  endif ()
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fdata-sections -ffunction-sections")
  set (CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -fdata-sections -ffunction-sections")
  set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fdata-sections -ffunction-sections")
  set (CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -fdata-sections -ffunction-sections")
endif (NOT APPLE)

if (WIN32)
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endif (WIN32)

if (APPLE AND ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -stdlib=libc++")
else ()
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=gnu++17 -pthread")
endif ()


#
# Fortran setup
#
set (General_FFLAGS "-Wall -Wno-conversion -Wno-c-binding-type -fno-second-underscore")

# FFLAGS depend on the compiler
get_filename_component (Fortran_COMPILER_NAME ${CMAKE_Fortran_COMPILER} NAME)

if (Fortran_COMPILER_NAME MATCHES "gfortran.*")
  # gfortran

  # CMake compiler test is supposed to do this but doesn't yet
  if (CMAKE_OSX_DEPLOYMENT_TARGET)
    set (CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  endif (CMAKE_OSX_DEPLOYMENT_TARGET)
  if (CMAKE_OSX_SYSROOT)
    set (CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -isysroot ${CMAKE_OSX_SYSROOT}")
  endif (CMAKE_OSX_SYSROOT)

  set (CMAKE_Fortran_FLAGS_RELEASE "${CMAKE_Fortran_FLAGS_RELEASE} -funroll-all-loops -fno-f2c -ffpe-summary=invalid,zero,overflow,underflow ${General_FFLAGS}")

  set (CMAKE_Fortran_FLAGS_DEBUG   "${CMAKE_Fortran_FLAGS_DEBUG} -ggdb -O0 -fbacktrace -fcheck=all -fbounds-check -fno-f2c -ffpe-summary=invalid,zero,overflow,underflow ${General_FFLAGS}")
  
  # FPE traps currently disabled in Debug configuration builds until
  # we decide if they are meaningful, without these FP instructions
  # run in nonstop mode and do not trap
  #set (CMAKE_Fortran_FLAGS_DEBUG   "${CMAKE_Fortran_FLAGS_DEBUG} ${CMAKE_Fortran_FLAGS_DEBUG}  -ffpe-trap=invalid,zero,overflow")
  
elseif (Fortran_COMPILER_NAME MATCHES "ifort.*")
  # ifort (untested)
  set (CMAKE_Fortran_FLAGS_RELEASE "${CMAKE_Fortran_FLAGS_RELEASE} -f77rtl ${General_FFLAGS}")
  set (CMAKE_Fortran_FLAGS_DEBUG   "${CMAKE_Fortran_FLAGS_DEBUG} -f77rtl ${General_FFLAGS}")
elseif (Fortran_COMPILER_NAME MATCHES "g77")
  # g77
  set (CMAKE_Fortran_FLAGS_RELEASE "${CMAKE_Fortran_FLAGS_RELEASE} -funroll-all-loops -fno-f2c -m32 ${General_FFLAGS}")
  set (CMAKE_Fortran_FLAGS_DEBUG   "${CMAKE_Fortran_FLAGS_DEBUG} -fbounds-check -fno-f2c -m32 ${General_FFLAGS}")
else (Fortran_COMPILER_NAME MATCHES "gfortran.*")
  message ("CMAKE_Fortran_COMPILER full path: " ${CMAKE_Fortran_COMPILER})
  message ("Fortran compiler: " ${Fortran_COMPILER_NAME})
  message ("No optimized Fortran compiler flags are known, we just try -O3...")
  set (CMAKE_Fortran_FLAGS_RELEASE "${CMAKE_Fortran_FLAGS_RELEASE} -O3 ${General_FFLAGS}")
  set (CMAKE_Fortran_FLAGS_DEBUG   "${CMAKE_Fortran_FLAGS_DEBUG} -g -fbacktrace -fcheck=all -fbounds-check ${General_FFLAGS}")
endif (Fortran_COMPILER_NAME MATCHES "gfortran.*")


#
# Linker setup
#
if (NOT APPLE)
  set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s")
  set (CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} -Wl,--gc-sections")
endif (NOT APPLE)


#
# setup and test Fortran C/C++ interaction
#

if (WIN32)
  # On Windows with mingw64, the FortranCInterface link test fails due to mixed
  # ucrt64/mingw64 linker environment. Use a known-correct FC.h for gfortran/mingw.
  file (WRITE "${CMAKE_BINARY_DIR}/FC.h"
    "#ifndef FC_HEADER_INCLUDED\n"
    "#define FC_HEADER_INCLUDED\n"
    "\n"
    "/* Mangling for Fortran global symbols without underscores. */\n"
    "#define FC_GLOBAL(name,NAME) name##_\n"
    "\n"
    "/* Mangling for Fortran global symbols with underscores. */\n"
    "#define FC_GLOBAL_(name,NAME) name##_\n"
    "\n"
    "/* Mangling for Fortran module symbols without underscores. */\n"
    "#define FC_MODULE(mod_name,name, mod_NAME,NAME) __##mod_name##_MOD_##name\n"
    "\n"
    "/* Mangling for Fortran module symbols with underscores. */\n"
    "#define FC_MODULE_(mod_name,name, mod_NAME,NAME) __##mod_name##_MOD_##name\n"
    "\n"
    "/*--------------------------------------------------------------------------*/\n"
    "/* Mangle some symbols automatically.                                       */\n"
    "#define FC_grayline FC_GLOBAL(grayline, GRAYLINE)\n"
    "\n"
    "#endif\n"
  )
else ()
  include (FortranCInterface)
  FortranCInterface_VERIFY (CXX)
  FortranCInterface_HEADER (FC.h MACRO_NAMESPACE "FC_" SYMBOL_NAMESPACE "FC_"
    SYMBOLS
    grayline
    )
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
if (BUILD_TESTING AND EXISTS ${CMAKE_SOURCE_DIR}/tests AND IS_DIRECTORY ${CMAKE_SOURCE_DIR}/tests)
  add_subdirectory (tests)
endif ()

# build a library of package functionality (without and optionally with OpenMP support)
add_library (wsjt_cxx STATIC ${wsjt_CSRCS} ${wsjt_CXXSRCS})
target_link_libraries (wsjt_cxx ${LIBM_LIBRARIES} Boost::log_setup ${FFTW3_LIBRARIES} ${LIBM_LIBRARIES})

# build an OpenMP variant of the Fortran library routines
add_library (wsjt_fort STATIC ${wsjt_FSRCS})
target_link_libraries (wsjt_fort  ${FFTW3_LIBRARIES})
if (${OPENMP_FOUND} OR APPLE)
  add_library (wsjt_fort_omp STATIC ${wsjt_FSRCS})
  target_link_libraries (wsjt_fort_omp  ${FFTW3_LIBRARIES})
  if (OpenMP_C_FLAGS AND NOT APPLE)
    set_target_properties (wsjt_fort_omp
      PROPERTIES
      COMPILE_FLAGS "${OpenMP_C_FLAGS}"
      )
  endif ()
  set_target_properties (wsjt_fort_omp
    PROPERTIES
    Fortran_MODULE_DIRECTORY ${CMAKE_BINARY_DIR}/fortran_modules_omp
    )
  file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/fortran_modules_omp)
  if (APPLE)
    target_compile_options (wsjt_fort_omp
      PRIVATE
      $<$<COMPILE_LANGUAGE:Fortran>:-fopenmp>
      )
  endif (APPLE)
endif (${OPENMP_FOUND} OR APPLE)

function (link_wsjtx_cycle_group target)
  if (APPLE OR WIN32)
    target_link_libraries (${target} ${ARGN})
  else ()
    target_link_libraries (${target} "-Wl,--start-group" ${ARGN} "-Wl,--end-group")
  endif ()
endfunction ()

if(WSJT_BUILD_UTILS) 

add_executable (jt4sim lib/jt4sim.f90)
target_link_libraries (jt4sim wsjt_fort wsjt_cxx)

add_executable (jt65sim lib/jt65sim.f90)
target_link_libraries (jt65sim wsjt_fort wsjt_cxx)

add_executable (sumsim lib/sumsim.f90)
target_link_libraries (sumsim wsjt_fort wsjt_cxx)

add_executable (cablog lib/cablog.f90)
target_link_libraries (cablog)

add_executable (test_snr lib/test_snr.f90)
target_link_libraries (test_snr wsjt_fort)

add_executable (q65sim utils/q65sim.cpp)
link_wsjtx_cycle_group (q65sim wsjt_qt wsjt_cxx wsjt_fort)

add_executable (EchoCallSim lib/EchoCallSim.f90)
target_link_libraries (EchoCallSim wsjt_fort wsjt_cxx)

add_executable (testEchoCall lib/testEchoCall.f90)
target_link_libraries (testEchoCall wsjt_fort wsjt_cxx)

add_executable (cwsim lib/cwsim.f90)
target_link_libraries (cwsim wsjt_fort wsjt_cxx)

add_executable (q65code utils/q65code.cpp)
link_wsjtx_cycle_group (q65code wsjt_qt wsjt_cxx wsjt_fort)

add_executable (test_q65 utils/test_q65.cpp)
link_wsjtx_cycle_group (test_q65 wsjt_qt wsjt_cxx wsjt_fort)

add_executable (q65_ftn_test utils/q65_ftn_test.cpp)
link_wsjtx_cycle_group (q65_ftn_test wsjt_qt wsjt_cxx wsjt_fort)

add_executable (q65params utils/q65params.cpp)
target_link_libraries (q65params wsjt_qt wsjt_cxx)

add_executable (jt49sim lib/jt49sim.f90)
target_link_libraries (jt49sim wsjt_fort wsjt_cxx)

add_executable (jt9sim lib/jt9sim.f90)
target_link_libraries (jt9sim wsjt_fort wsjt_cxx)

add_executable (rtty_spec utils/rtty_spec.cpp)
link_wsjtx_cycle_group (rtty_spec wsjt_qt wsjt_cxx wsjt_fort)

add_executable (jt65code lib/jt65code.f90)
target_link_libraries (jt65code wsjt_fort wsjt_cxx)

add_executable (jt65_compare utils/jt65_compare.cpp)
link_wsjtx_cycle_group (jt65_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (jt65_symspec_compare utils/jt65_symspec_compare.cpp)
link_wsjtx_cycle_group (jt65_symspec_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (jt65_sync_compare
  utils/jt65_sync_compare.cpp)
link_wsjtx_cycle_group (jt65_sync_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (jt65_extract_primitives_compare
  utils/jt65_extract_primitives_compare.cpp)
link_wsjtx_cycle_group (jt65_extract_primitives_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (jt65_extract_compare
  utils/jt65_extract_compare.cpp)
link_wsjtx_cycle_group (jt65_extract_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (jt65_decode65b_compare
  utils/jt65_decode65b_compare.cpp)
link_wsjtx_cycle_group (jt65_decode65b_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (jt65_decode65a_compare
  utils/jt65_decode65a_compare.cpp)
link_wsjtx_cycle_group (jt65_decode65a_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (jt9code lib/jt9code.f90)
target_link_libraries (jt9code wsjt_fort wsjt_cxx)

add_executable (jt9_compare utils/jt9_compare.cpp)
link_wsjtx_cycle_group (jt9_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (jt9_wide_compare utils/jt9_wide_compare.cpp)
link_wsjtx_cycle_group (jt9_wide_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (jt9_wide_stage_compare utils/jt9_wide_stage_compare.cpp)
link_wsjtx_cycle_group (jt9_wide_stage_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (jt4_compare utils/jt4_compare.cpp)
link_wsjtx_cycle_group (jt4_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (wspr_compare utils/wspr_compare.cpp)
link_wsjtx_cycle_group (wspr_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (txwave_compare utils/txwave_compare.cpp)
link_wsjtx_cycle_group (txwave_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (plot_compare utils/plot_compare.cpp)
link_wsjtx_cycle_group (plot_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (indexx_compare utils/indexx_compare.cpp)
link_wsjtx_cycle_group (indexx_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (legacy_dspio_compare utils/legacy_dspio_compare.cpp)
link_wsjtx_cycle_group (legacy_dspio_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (legacy_spectrum_compare utils/legacy_spectrum_compare.cpp)
link_wsjtx_cycle_group (legacy_spectrum_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (legacy_echo_compare utils/legacy_echo_compare.cpp)
link_wsjtx_cycle_group (legacy_echo_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (legacy_wsprio_compare utils/legacy_wsprio_compare.cpp)
link_wsjtx_cycle_group (legacy_wsprio_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (fast9_stage_compare utils/fast9_stage_compare.cpp)
link_wsjtx_cycle_group (fast9_stage_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (fast9_decode_compare utils/fast9_decode_compare.cpp)
link_wsjtx_cycle_group (fast9_decode_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (wsprcode lib/wsprcode/wsprcode.f90 lib/wsprcode/nhash.c)
target_link_libraries (wsprcode wsjt_fort wsjt_cxx)
	       
add_executable (encode77 utils/encode77.cpp)
link_wsjtx_cycle_group (encode77 wsjt_qt wsjt_cxx wsjt_fort)

add_executable (nonstandard_call_compare utils/nonstandard_call_compare.cpp)
link_wsjtx_cycle_group (nonstandard_call_compare wsjt_qt wsjt_cxx wsjt_fort)

add_executable (hash22calc utils/hash22calc.cpp)
link_wsjtx_cycle_group (hash22calc wsjt_qt wsjt_cxx wsjt_fort)

add_executable (wsprsim ${wsprsim_CSRCS})
target_link_libraries (wsprsim ${LIBM_LIBRARIES})

add_executable (jt4code lib/jt4code.f90)
target_link_libraries (jt4code wsjt_fort wsjt_cxx)

add_executable (msk144code utils/msk144code.cpp)
link_wsjtx_cycle_group (msk144code wsjt_qt wsjt_cxx wsjt_fort)

add_executable (ft8code utils/ft8code.cpp)
link_wsjtx_cycle_group (ft8code wsjt_qt wsjt_cxx wsjt_fort)

add_executable (ft4code utils/ft4code.cpp)
link_wsjtx_cycle_group (ft4code wsjt_qt wsjt_cxx wsjt_fort)

add_executable (echosim lib/echosim.f90)
target_link_libraries (echosim wsjt_fort wsjt_cxx)

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
if (WIN32)
  set_target_properties (fst4sim PROPERTIES
    LINK_FLAGS -Wl,--stack,0x4000000,--heap,0x6000000
    )
endif ()

add_executable (ldpcsim240_101 utils/ldpcsim240_101.cpp)
target_link_libraries (ldpcsim240_101 wsjt_qt wsjt_cxx wsjt_fort)

add_executable (ldpcsim240_74 utils/ldpcsim240_74.cpp)
target_link_libraries (ldpcsim240_74 wsjt_qt wsjt_cxx wsjt_fort)

endif(WSJT_BUILD_UTILS)

if(WSJT_BUILD_UTILS)

add_executable (fmtave lib/fmtave.f90)

add_executable (fcal lib/fcal.f90)

add_executable (fmeasure lib/fmeasure.f90)

endif(WSJT_BUILD_UTILS)

# build the wsprd WSPR mode decoder driver
generate_version_info (wsprd_VERSION_RESOURCES
  NAME wsprd
  BUNDLE ${PROJECT_BUNDLE_NAME}
  ICON ${WSJTX_ICON_FILE}
  FILE_DESCRIPTION "WSPR mode decoder"
  )
add_executable (wsprd ${wsprd_CSRCS} lib/indexx.f90 lib/wsprd/osdwspr.f90 ${wsprd_VERSION_RESOURCES})
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
  en_GB				# English UK
  es				# Spanish
  fr				# French
  it				# Italian
  ja				# Japanese
  #no				# Norwegian
  #pt				# Portuguese
  ru				# Russian
  #sv				# Swedish
  zh				# Chinese
  zh_TW				# Chinese traditional
  it				# Italian
  )
foreach (lang_ ${LANGUAGES})
  file (TO_NATIVE_PATH ${CMAKE_SOURCE_DIR}/translations/wsjtx_${lang_}.ts ts_)
  list (APPEND TS_FILES ${ts_})
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
  qt_create_translation (
    QM_FILES ${wsjt_qt_UISRCS} ${wsjtx_UISRCS} ${wsjt_qt_CXXSRCS} ${wsjtx_CXXSRCS}
    ${TS_FILES}
    OPTIONS -I${CMAKE_CURRENT_SOURCE_DIR}
    )
else ()
  qt_add_translation (QM_FILES ${TS_FILES})
endif ()
add_custom_target (translations DEPENDS ${QM_FILES})
set_property (DIRECTORY PROPERTY CLEAN_NO_CUSTOM TRUE)

# embedded resources
function (add_resources resources path)
  foreach (resource_file_ ${ARGN})
    get_filename_component (name_ ${resource_file_} NAME)
    if (IS_ABSOLUTE "${resource_file_}")
      file (TO_NATIVE_PATH ${resource_file_} source_)
    else ()
      file (TO_NATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${resource_file_} source_)
    endif ()
    file (TO_NATIVE_PATH ${path}/${name_} dest_)
    set (resources_ "${resources_}\n    <file alias=\"${dest_}\">${source_}</file>")
    set (${resources} ${${resources}}${resources_} PARENT_SCOPE)
  endforeach (resource_file_ ${ARGN})
endfunction (add_resources resources path)

add_resources (wsjtx_RESOURCES "" ${TOP_LEVEL_RESOURCES})
add_resources (wsjtx_RESOURCES /Palettes ${PALETTE_FILES})
add_resources (wsjtx_RESOURCES /Translations ${QM_FILES})

configure_file (wsjtx.qrc.in wsjtx.qrc @ONLY)

# UI generation
qt_wrap_ui (wsjt_qt_GENUISRCS ${wsjt_qt_UISRCS})
qt_wrap_ui (wsjtx_GENUISRCS ${wsjtx_UISRCS})

# Resource generation
qt_add_resources (wsjtx_RESOURCES_RCC
  ${CMAKE_BINARY_DIR}/wsjtx.qrc
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
if (WIN32)
  target_link_libraries (wsjt_qt Qt6::ActiveQt)
endif (WIN32)

# Several legacy Fortran utilities now resolve migrated compatibility exports
# from both the Qt-side encoder code and the shared C++ DSP/Q65 helpers.
# Export both transitively so tools that still link only wsjt_fort continue to
# resolve the native replacements when WSJT_BUILD_UTILS is enabled.
target_link_libraries (wsjt_fort wsjt_qt wsjt_cxx)
if (TARGET wsjt_fort_omp)
  target_link_libraries (wsjt_fort_omp wsjt_qt wsjt_cxx)
endif ()
# build a library of package Qt functionality used in Fortran utilities
add_library (fort_qt STATIC ${fort_qt_CXXSRCS})
target_link_libraries (fort_qt Qt6::Core)

# build a library of WSJT Qt multimedia components
add_library (wsjt_qtmm STATIC ${wsjt_qtmm_CXXSRCS} ${wsjt_qtmm_GENUISRCS})
target_link_libraries (wsjt_qtmm Qt6::Multimedia)

add_executable (jt9 utils/jt9.cpp)
target_sources (jt9 PRIVATE
  Detector/FT2DecodeWorker.cpp
  Detector/FT4DecodeWorker.cpp
  Detector/FT8DecodeWorker.cpp
  Detector/LegacyJtDecodeWorker.cpp
  Detector/MSK144DecodeWorker.cpp
  Detector/Q65DecodeWorker.cpp
  )
link_wsjtx_cycle_group (jt9 wsjt_qt wsjt_cxx wsjt_fort)

add_executable (jt9a utils/jt9.cpp)
target_sources (jt9a PRIVATE
  Detector/FT2DecodeWorker.cpp
  Detector/FT4DecodeWorker.cpp
  Detector/FT8DecodeWorker.cpp
  Detector/LegacyJtDecodeWorker.cpp
  Detector/MSK144DecodeWorker.cpp
  Detector/Q65DecodeWorker.cpp
  )
link_wsjtx_cycle_group (jt9a wsjt_qt wsjt_cxx wsjt_fort)

if (NOT APPLE)
  add_executable (record_time_signal Audio/tools/record_time_signal.cpp)
  target_link_libraries (record_time_signal wsjt_cxx wsjt_qtmm wsjt_qt)
endif ()

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
  MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_BUNDLE_NAME}"
  MACOSX_BUNDLE_BUNDLE_EXECUTABLE_NAME "${WSJT_APP_EXECUTABLE_NAME}"
  MACOSX_BUNDLE_COPYRIGHT "${PROJECT_COPYRIGHT}"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.decodium.ft2"
  )

target_include_directories (wsjtx PRIVATE ${FFTW3_INCLUDE_DIRS})
set (_wsjtx_fort_target wsjt_fort)
if ((NOT ${OPENMP_FOUND}) OR APPLE)
  set (_wsjtx_fort_target wsjt_fort)
else ()
  set (_wsjtx_fort_target wsjt_fort_omp)
  if (OpenMP_C_FLAGS)
    set_target_properties (wsjtx PROPERTIES
      COMPILE_FLAGS "${OpenMP_C_FLAGS}"
      LINK_FLAGS "${OpenMP_C_FLAGS}"
      )
  endif ()
  set_target_properties (wsjtx PROPERTIES
    Fortran_MODULE_DIRECTORY ${CMAKE_BINARY_DIR}/fortran_modules_omp
    )
  if (WIN32)
    set_target_properties (wsjtx PROPERTIES
      LINK_FLAGS -Wl,--stack,0x1000000,--heap,0x20000000
      )
  endif ()
endif ()
target_link_libraries (wsjtx Qt6::SerialPort Qt6::Quick Qt6::Qml Qt6::QuickControls2 wsjt_qtmm ${FFTW3_LIBRARIES} ${LIBM_LIBRARIES})
target_compile_options (wsjtx PRIVATE -Wno-deprecated-declarations)
link_wsjtx_cycle_group (wsjtx wsjt_qt wsjt_cxx ${_wsjtx_fort_target})

# -----------------------------------------------------------------------
# decodium_qml: lightweight QML-based frontend (uses DecodiumBridge)
# -----------------------------------------------------------------------
if (WIN32)
  set (DECODIUM_QML_ICON_RC decodium_qml.rc)
else ()
  set (DECODIUM_QML_ICON_RC "")
endif ()
add_executable (decodium_qml
  main_qml.cpp
  DecodiumBridge.cpp
  DecodiumBridge.h
  helper_functions.cpp
  helper_functions.h
  controllers/ActiveStationsModel.cpp
  controllers/ActiveStationsModel.hpp
  DecodiumThemeManager.cpp
  DecodiumThemeManager.h
  DecodiumSubManagers.cpp
  DecodiumSubManagers.h
  DecodiumCatManager.cpp
  DecodiumCatManager.h
  DecodiumOmniRigManager.cpp
  DecodiumOmniRigManager.h
  DecodiumTransceiverManager.cpp
  DecodiumTransceiverManager.h
  DecodiumAlertManager.cpp
  DecodiumAlertManager.h
  WaterfallItem.cpp
  WaterfallItem.hpp
  PanadapterItem.cpp
  PanadapterItem.hpp
  DecodiumAudioSink.h
  Detector/FT8DecodeWorker.cpp
  Detector/FT2DecodeWorker.cpp
  Detector/FT4DecodeWorker.cpp
  Detector/Q65DecodeWorker.cpp
  Detector/MSK144DecodeWorker.cpp
  Detector/WSPRDecodeWorker.cpp
  Detector/FST4DecodeWorker.cpp
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
  DecodiumQmlLegacyCompat.cpp
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
  DecodiumDxCluster.cpp
  DecodiumDxCluster.h
  DxccLookup.cpp
  DxccLookup.h
  ${wsjtx_GENUISRCS}
  ${wsjtx_RESOURCES_RCC}
  ${DECODIUM_QML_ICON_RC}
)
if (WSJT_CREATE_WINMAIN)
  set_target_properties (decodium_qml PROPERTIES WIN32_EXECUTABLE ON)
endif ()
set_target_properties (decodium_qml PROPERTIES OUTPUT_NAME decodium)
# Sopprime i warnings-as-error su LegacyDspIoHelpers (vector bounds in GCC15)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties (Detector/LegacyDspIoHelpers.cpp PROPERTIES
    COMPILE_FLAGS "-Wno-error=stringop-overflow -Wno-error=stringop-overread -Wno-error=array-bounds")
endif ()
# C17 — FoxVerifier senza Boost.Log (usa qDebug come shim)
target_compile_definitions (decodium_qml PRIVATE DECODIUM_NO_BOOST_LOG FFTW3_SINGLE_FOUND)
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
if (WIN32)
  target_link_libraries (decodium_qml Qt6::AxContainer)
endif ()
# gfortran runtime dinamico
if (MINGW)
  target_link_libraries (decodium_qml gfortran quadmath)
endif ()
# Copy QML files next to the decodium_qml executable at build time
add_custom_command (TARGET decodium_qml POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/qml"
    "$<TARGET_FILE_DIR:decodium_qml>/qml"
  COMMENT "Copying QML files to build output"
)

if (APPLE)
  add_custom_command (TARGET wsjtx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
      $<TARGET_FILE_DIR:wsjtx>/sounds
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/cty.dat"
      "$<TARGET_FILE_DIR:wsjtx>/../Resources/cty.dat"
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

install (TARGETS jt9 wsprd
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  )

if(WSJT_BUILD_UTILS)
install (TARGETS fmtave fcal fmeasure
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  )
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
  README
  COPYING
  AUTHORS
  THANKS
  NEWS
  BUGS
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  #COMPONENT runtime
  )

install (FILES
  cty.dat
  cty.dat_copyright.txt
  grid.dat
  sat.dat
  contrib/Ephemeris/JPLEPH
  eclipse.txt
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

install (FILES
  ALLCALL7.TXT
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  #COMPONENT runtime
  )

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
        Darwin/com.wsjtx.sysctl.plist
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
  "${CMAKE_CURRENT_SOURCE_DIR}/wsjtx_config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/wsjtx_config.h"
  )


if (NOT WIN32 AND NOT APPLE)
  # install a desktop file so wsjtx appears in the application start
  # menu with an icon
  install (
    FILES wsjtx.desktop message_aggregator.desktop
    DESTINATION share/applications
    #COMPONENT runtime
    )
  install (
    FILES icons/Unix/wsjtx_icon.png
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
set (CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/package_description.txt")
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

configure_file ("${PROJECT_SOURCE_DIR}/CMakeCPackOptions.cmake.in"
  "${PROJECT_BINARY_DIR}/CMakeCPackOptions.cmake" @ONLY)
set (CPACK_PROJECT_CONFIG_FILE "${PROJECT_BINARY_DIR}/CMakeCPackOptions.cmake")

include (CPack)
