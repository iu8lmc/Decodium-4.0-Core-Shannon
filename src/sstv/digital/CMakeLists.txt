set (DECODIUM_HAMDRM_CORE_SOURCES
  HamDrmBsrCodec.cpp
  HamDrmCrc.cpp
  HamDrmImageValidator.cpp
  HamDrmMotCodec.cpp
  HamDrmObjectAssembler.cpp
  HamDrmProfileRegistry.cpp
  channel/HamDrmCellPlan.cpp
  channel/HamDrmChannelCoding.cpp
  channel/HamDrmChannelCrc.cpp
  channel/HamDrmFacCodec.cpp
  channel/HamDrmInterleaver.cpp
  channel/HamDrmMlcCodec.cpp
  channel/HamDrmPilotEqualizer.cpp
  phy/HamDrmComplexTransform.cpp
  phy/HamDrmOfdmModem.cpp
  phy/HamDrmOfdmParameters.cpp
  phy/HamDrmQam.cpp
  waveform/HamDrmPacketCodec.cpp
  waveform/HamDrmWaveformCodec.cpp
  )

set (DECODIUM_HAMDRM_CORE_HEADERS
  HamDrmBsrCodec.h
  HamDrmCrc.h
  HamDrmImageValidator.h
  HamDrmMotCodec.h
  HamDrmObjectAssembler.h
  HamDrmProfileRegistry.h
  HamDrmTypes.h
  channel/HamDrmCellPlan.h
  channel/HamDrmChannelCoding.h
  channel/HamDrmChannelCrc.h
  channel/HamDrmFacCodec.h
  channel/HamDrmInterleaver.h
  channel/HamDrmMlcCodec.h
  channel/HamDrmPilotEqualizer.h
  phy/HamDrmComplexTransform.h
  phy/HamDrmOfdmModem.h
  phy/HamDrmOfdmParameters.h
  phy/HamDrmQam.h
  waveform/HamDrmPacketCodec.h
  waveform/HamDrmWaveformCodec.h
  )

add_library (decodium_sstv_hamdrm_core STATIC
  ${DECODIUM_HAMDRM_CORE_SOURCES}
  ${DECODIUM_HAMDRM_CORE_HEADERS}
  )
add_library (Decodium::HamDrmCore ALIAS decodium_sstv_hamdrm_core)
target_include_directories (decodium_sstv_hamdrm_core
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_compile_features (decodium_sstv_hamdrm_core PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_hamdrm_core PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

add_library (decodium_sstv_hamdrm_persistence STATIC
  HamDrmPartialStore.cpp
  HamDrmPartialStore.h
  )
add_library (Decodium::HamDrmPersistence ALIAS
  decodium_sstv_hamdrm_persistence)
target_include_directories (decodium_sstv_hamdrm_persistence
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_hamdrm_persistence
  PUBLIC
    Decodium::HamDrmCore
    Qt6::Core
  )
target_compile_features (decodium_sstv_hamdrm_persistence PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_hamdrm_persistence PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

# OpenJPEG is deliberately target-scoped.  Disabling HAMDRM removes this
# dependency completely and leaves the native analog SSTV targets unchanged.
find_package (OpenJPEG 2.5 CONFIG QUIET)
if (TARGET OpenJPEG::openjp2)
  set (_decodium_hamdrm_openjpeg_target OpenJPEG::openjp2)
elseif (TARGET openjp2)
  set (_decodium_hamdrm_openjpeg_target openjp2)
else ()
  find_package (PkgConfig QUIET)
  if (PkgConfig_FOUND)
    pkg_check_modules (DECODIUM_HAMDRM_OPENJPEG QUIET IMPORTED_TARGET
      libopenjp2>=2.5)
  endif ()
  if (TARGET PkgConfig::DECODIUM_HAMDRM_OPENJPEG)
    set (_decodium_hamdrm_openjpeg_target
      PkgConfig::DECODIUM_HAMDRM_OPENJPEG)
  else ()
    message (FATAL_ERROR
      "DECODIUM_ENABLE_HAMDRM=ON requires audited OpenJPEG >= 2.5. "
      "Install OpenJPEG or configure -DDECODIUM_ENABLE_HAMDRM=OFF for an "
      "analog-only build.")
  endif ()
endif ()

add_library (decodium_sstv_hamdrm_openjpeg STATIC
  HamDrmJpeg2000Codec.cpp
  HamDrmJpeg2000Codec.h
  )
add_library (Decodium::HamDrmOpenJpeg ALIAS
  decodium_sstv_hamdrm_openjpeg)
target_include_directories (decodium_sstv_hamdrm_openjpeg
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_hamdrm_openjpeg
  PUBLIC
    Decodium::HamDrmCore
  PRIVATE
    ${_decodium_hamdrm_openjpeg_target}
  )
target_compile_features (decodium_sstv_hamdrm_openjpeg PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_hamdrm_openjpeg PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

add_library (decodium_sstv_hamdrm_ui STATIC
  HamDrmController.cpp
  HamDrmController.h
  )
add_library (Decodium::HamDrmUi ALIAS decodium_sstv_hamdrm_ui)
target_include_directories (decodium_sstv_hamdrm_ui
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_hamdrm_ui
  PUBLIC
    Decodium::SstvDiagnosticLogging
    Decodium::HamDrmCore
    Decodium::HamDrmPersistence
    Decodium::HamDrmOpenJpeg
    Qt6::Core
    Qt6::Gui
  )
target_compile_features (decodium_sstv_hamdrm_ui PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_hamdrm_ui PROPERTIES
  AUTOMOC ON
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

add_library (decodium_sstv_hamdrm_runtime STATIC
  waveform/HamDrmWaveformAdapters.cpp
  waveform/HamDrmWaveformAdapters.h
  )
add_library (Decodium::HamDrmRuntime ALIAS
  decodium_sstv_hamdrm_runtime)
target_include_directories (decodium_sstv_hamdrm_runtime
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_link_libraries (decodium_sstv_hamdrm_runtime
  PUBLIC
    Decodium::HamDrmUi
    Decodium::SstvIntegration
    Qt6::Core
  PRIVATE
    Threads::Threads
  )
target_compile_features (decodium_sstv_hamdrm_runtime PUBLIC cxx_std_17)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options (decodium_sstv_hamdrm_runtime PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif ()
set_target_properties (decodium_sstv_hamdrm_runtime PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )

message (STATUS
  "Decodium HAMDRM enabled with OpenJPEG target: "
  "${_decodium_hamdrm_openjpeg_target}")
unset (_decodium_hamdrm_openjpeg_target)
