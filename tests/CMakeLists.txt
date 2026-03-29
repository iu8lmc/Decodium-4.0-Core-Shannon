find_package (Qt5Test 5 REQUIRED)

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
include_directories (${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR})

add_executable (test_qt_helpers test_qt_helpers.cpp)
target_link_libraries (test_qt_helpers wsjt_qt wsjt_cxx wsjt_fort Qt5::Test)
add_test (NAME test_qt_helpers COMMAND $<TARGET_FILE:test_qt_helpers>)

add_executable (ft4_stage_compare ft4_stage_compare.cpp)
target_link_libraries (ft4_stage_compare wsjt_qt wsjt_cxx wsjt_fort Qt5::Core)

add_executable (ft8_stage_compare ft8_stage_compare.cpp)
target_link_libraries (ft8_stage_compare wsjt_qt wsjt_cxx wsjt_fort Qt5::Core)

add_executable (ft2_stage_compare ft2_stage_compare.cpp)
target_link_libraries (ft2_stage_compare wsjt_qt wsjt_cxx wsjt_fort Qt5::Core)
add_dependencies (test_qt_helpers ft2_stage_compare)

add_executable (q65_stage_compare q65_stage_compare.cpp ../Detector/Q65DecodeWorker.cpp)
target_include_directories (q65_stage_compare PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries (q65_stage_compare wsjt_qt wsjt_cxx wsjt_fort Qt5::Core)

add_executable (ft2_equalized_compare ft2_equalized_compare.cpp)
target_link_libraries (ft2_equalized_compare wsjt_qt wsjt_cxx wsjt_fort Qt5::Core)
add_dependencies (test_qt_helpers ft2_equalized_compare)

add_executable (ft2_make_test_wav ft2_make_test_wav.cpp)
target_link_libraries (ft2_make_test_wav wsjt_qt wsjt_cxx wsjt_fort Qt5::Core)
add_dependencies (test_qt_helpers ft2_make_test_wav)

add_executable (test_secure_settings test_secure_settings.cpp)
target_link_libraries (test_secure_settings wsjt_qt wsjt_cxx Qt5::Test)
add_test (NAME test_secure_settings COMMAND $<TARGET_FILE:test_secure_settings>)

add_executable (test_filedownload test_filedownload.cpp)
target_link_libraries (test_filedownload wsjt_qt wsjt_cxx Qt5::Test)
add_test (NAME test_filedownload COMMAND $<TARGET_FILE:test_filedownload>)

if (WSJT_BUILD_UTILS)
  # CLI smoke tests for utility binaries that are built in the main project.
  add_test (NAME test_q65_usage COMMAND $<TARGET_FILE:test_q65>)
  set_tests_properties (test_q65_usage PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")

  add_test (NAME test_snr_usage COMMAND $<TARGET_FILE:test_snr>)
  set_tests_properties (test_snr_usage PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")

  add_test (NAME testEchoCall_usage COMMAND $<TARGET_FILE:testEchoCall>)
  set_tests_properties (testEchoCall_usage PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")
endif ()
