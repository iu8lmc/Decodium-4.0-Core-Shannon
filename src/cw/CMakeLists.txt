# Il modulo CW di Decodium: decodificatore, manipolatori e macro.
#
# Viene da DecoDXLog (iu8lmc/DecoDXLog): stesso decodificatore (ggmorse), stessi
# manipolatori (porta seriale DTR/RTS e K1EL WinKeyer), stesse macro. Qui sono
# in una libreria statica sola, collegata dentro Decodium come RTTY e JTTY.

set (CMAKE_AUTOMOC ON)

# ggmorse (Georgi Gerganov, MIT): non contiene oggetti Qt.
add_library (decodium_ggmorse STATIC
  ggmorse/ggmorse.cpp
  ggmorse/resampler.cpp
  )
target_include_directories (decodium_ggmorse PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions (decodium_ggmorse PRIVATE GGMORSE_QUIET)
set_target_properties (decodium_ggmorse PROPERTIES
  AUTOMOC OFF
  POSITION_INDEPENDENT_CODE ON
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)

add_library (decodium_cw STATIC
  CwDecoder.cpp       CwDecoder.h
  CwKeyer.cpp         CwKeyer.h
  WinKeyer.cpp        WinKeyer.h
  CwSerialPorts.cpp   CwSerialPorts.h
  CwMacros.cpp        CwMacros.h
  CwController.cpp    CwController.h
  )
target_include_directories (decodium_cw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries (decodium_cw PUBLIC Qt6::Core Qt6::SerialPort PRIVATE decodium_ggmorse)
set_target_properties (decodium_cw PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)

if (BUILD_TESTING)
  foreach (name test_cw_decoder test_cw_keyer test_winkeyer test_cw_macros test_cw_controller)
    add_executable (${name} tests/${name}.cpp)
    target_link_libraries (${name} PRIVATE decodium_cw Qt6::Test)
    set_target_properties (${name} PROPERTIES
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON)
    add_test (NAME ${name} COMMAND ${name})
  endforeach ()
endif ()
