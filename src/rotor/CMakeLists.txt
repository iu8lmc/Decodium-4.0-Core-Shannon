# Il rotore d'antenna PRO.SIS.TEL di Decodium.
#
# Viene da DecoRotor (iu8lmc/decorotor): stesso protocollo Prosistel, stessa
# logica di controllo e stessi server di rete (rotctld, WebSocket, HTTP), qui
# in C++/Qt dentro Decodium. Restano spenti finche' l'operatore non li accende.

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)

add_library (decodium_rotor STATIC
  RotorProtocol.cpp   RotorProtocol.h
  RotorGeo.cpp        RotorGeo.h
  RotorConfig.cpp     RotorConfig.h
  RotorTransport.cpp  RotorTransport.h
  RotorController.cpp RotorController.h
  RotorSpots.cpp      RotorSpots.h
  RotorServers.cpp    RotorServers.h
  RotorModule.cpp     RotorModule.h
  rotor.qrc
  )
target_include_directories (decodium_rotor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries (decodium_rotor PUBLIC Qt6::Core Qt6::Network Qt6::SerialPort Qt6::WebSockets)
set_target_properties (decodium_rotor PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  POSITION_INDEPENDENT_CODE ON)

if (BUILD_TESTING)
  find_package (Qt6 COMPONENTS Quick Qml QUIET)
  add_executable (rotor_qml_smoke tests/rotor_qml_smoke.cpp)
  target_link_libraries (rotor_qml_smoke PRIVATE decodium_rotor Qt6::Gui Qt6::Quick Qt6::Qml)
  set_target_properties (rotor_qml_smoke PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

  foreach (name test_rotor)
    add_executable (${name} tests/${name}.cpp)
    target_link_libraries (${name} PRIVATE decodium_rotor Qt6::Test)
    set_target_properties (${name} PROPERTIES
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON)
    add_test (NAME ${name} COMMAND ${name})
  endforeach ()
endif ()
