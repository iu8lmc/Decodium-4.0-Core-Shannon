# Find librtlsdr
#
# Defines:
#   RtlSdr_FOUND
#   RtlSdr_INCLUDE_DIR
#   RtlSdr_LIBRARY
#   RtlSdr::RtlSdr

include (FindPackageHandleStandardArgs)

find_path (RtlSdr_INCLUDE_DIR
  NAMES rtl-sdr.h
  PATH_SUFFIXES include
  )
find_library (RtlSdr_LIBRARY
  NAMES rtlsdr librtlsdr
  PATH_SUFFIXES lib lib64
  )

find_package_handle_standard_args (RtlSdr
  REQUIRED_VARS RtlSdr_INCLUDE_DIR RtlSdr_LIBRARY
  )

if (RtlSdr_FOUND AND NOT TARGET RtlSdr::RtlSdr)
  add_library (RtlSdr::RtlSdr UNKNOWN IMPORTED)
  set_target_properties (RtlSdr::RtlSdr PROPERTIES
    IMPORTED_LOCATION "${RtlSdr_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${RtlSdr_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced (RtlSdr_INCLUDE_DIR RtlSdr_LIBRARY)
