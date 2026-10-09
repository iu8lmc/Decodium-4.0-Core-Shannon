# JTTY (WSJT-X 3.2) in C++ nativo: il modem senza Qt e, sopra, il controllo
# che lo collega a Decodium. Il riferimento Fortran non entra nella build:
# tools/jtty_bench serve a confrontare le due implementazioni.

set (CMAKE_AUTOMOC ON)

add_library (decodium_jtty_core STATIC
  JttyCodec.cpp       JttyCodec.hpp
  JttyTbcc.cpp        JttyTbcc.hpp
  JttyWave.cpp        JttyWave.hpp
  JttyDecoder.cpp     JttyDecoder.hpp
  )
target_include_directories (decodium_jtty_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_SOURCE_DIR} ${FFTW3_INCLUDE_DIRS})
target_link_libraries (decodium_jtty_core PUBLIC ${FFTW3_LIBRARIES})
set_target_properties (decodium_jtty_core PROPERTIES
  AUTOMOC OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)

add_library (decodium_jtty STATIC
  JttyController.cpp  JttyController.h
  JttyMessages.hpp
  )
target_link_libraries (decodium_jtty PUBLIC decodium_jtty_core Qt6::Core)
set_target_properties (decodium_jtty PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)

if (WSJT_BUILD_UTILS)
  add_executable (jtty_bench tools/jtty_bench.cpp)
  target_link_libraries (jtty_bench decodium_jtty_core)
endif ()

# Vettori d'oro del riferimento Fortran e un giro completo TX -> RX.
if (BUILD_TESTING)
  add_executable (jtty_selftest tools/jtty_selftest.cpp)
  target_link_libraries (jtty_selftest decodium_jtty_core)
  add_test (NAME jtty_selftest COMMAND jtty_selftest)
endif ()
