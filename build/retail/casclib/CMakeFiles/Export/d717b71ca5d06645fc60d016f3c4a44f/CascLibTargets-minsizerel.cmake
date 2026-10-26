#----------------------------------------------------------------
# Generated CMake target import file for configuration "MinSizeRel".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "CascLib::casc_static" for configuration "MinSizeRel"
set_property(TARGET CascLib::casc_static APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(CascLib::casc_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_MINSIZEREL "C;CXX"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/lib/casc.lib"
  )

list(APPEND _cmake_import_check_targets CascLib::casc_static )
list(APPEND _cmake_import_check_files_for_CascLib::casc_static "${_IMPORT_PREFIX}/lib/casc.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
