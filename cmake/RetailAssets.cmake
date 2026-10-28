# Keep upstream storage code out of default builds. A source checkout can be supplied for
# offline builds; otherwise download an immutable revision, never an upstream branch.
set(WOWDECOMP_CASCLIB_SOURCE_DIR "" CACHE PATH "Existing CascLib source checkout")
if (NOT WOWDECOMP_CASCLIB_SOURCE_DIR)
    if (CMAKE_VERSION VERSION_LESS 3.14)
        message(FATAL_ERROR "Retail assets require CMake 3.14+ or WOWDECOMP_CASCLIB_SOURCE_DIR")
    endif ()
    include(FetchContent)
    FetchContent_Declare(wow_casclib
        URL https://codeload.github.com/ladislav-zezula/CascLib/zip/38a34665624b8775bb875274b36191b21c38d97b
        URL_HASH SHA256=1b59deebc82a4eb75c6e9ef463a7644b22231a4b3b0657ad9afe0dd5205fee3f
    )
    FetchContent_GetProperties(wow_casclib)
    if (NOT wow_casclib_POPULATED)
        FetchContent_Populate(wow_casclib)
    endif ()
    set(WOWDECOMP_CASCLIB_SOURCE_DIR "${wow_casclib_SOURCE_DIR}")
endif ()

set(CASC_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(CASC_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
set(CASC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CASC_UNICODE OFF CACHE BOOL "" FORCE)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    # Upstream still declares CMake 3.2; CMake 4 removed those compatibility policies.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif ()
add_subdirectory("${WOWDECOMP_CASCLIB_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/casclib" EXCLUDE_FROM_ALL)

add_library(RetailAssets STATIC
    "${CMAKE_SOURCE_DIR}/Engine/Source/Retail/CascSource.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/Source/Retail/DecodedCache.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailModel.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/Source/Retail/RetailShader.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/Source/Retail/ClientTables.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/Source/Retail/Db2.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/Source/Retail/Appearance.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/Source/Retail/Compose.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/Source/Retail/BusyNotice.cpp"
)
target_include_directories(RetailAssets PUBLIC "${CMAKE_SOURCE_DIR}")
target_compile_definitions(RetailAssets PUBLIC WOWDECOMP_RETAIL_ASSETS=1)
target_link_libraries(RetailAssets PRIVATE casc_static)

add_executable(RetailAssetProbe "${CMAKE_SOURCE_DIR}/Tools/RetailAssets/probe.cpp")
target_link_libraries(RetailAssetProbe PRIVATE RetailAssets)
add_executable(RetailRaceProbe "${CMAKE_SOURCE_DIR}/Tools/RetailAssets/races.cpp")
target_link_libraries(RetailRaceProbe PRIVATE RetailAssets)
add_executable(RetailComposeProbe "${CMAKE_SOURCE_DIR}/Tools/RetailAssets/compose.cpp")
target_link_libraries(RetailComposeProbe PRIVATE RetailAssets)
add_executable(RetailAssetTests "${CMAKE_SOURCE_DIR}/Tools/RetailAssets/tests.cpp")
target_link_libraries(RetailAssetTests PRIVATE RetailAssets)
add_executable(RetailAssetAcquire "${CMAKE_SOURCE_DIR}/Tools/RetailAssets/acquire.cpp")
target_link_libraries(RetailAssetAcquire PRIVATE RetailAssets)
add_executable(CdnAssetDownload "${CMAKE_SOURCE_DIR}/Tools/RetailAssets/download.cpp")
target_link_libraries(CdnAssetDownload PRIVATE RetailAssets)
add_executable(CdnSourceSmoke "${CMAKE_SOURCE_DIR}/Tools/RetailAssets/source_smoke.cpp")
target_link_libraries(CdnSourceSmoke PRIVATE RetailAssets)
# Custom: per-file retail race packs from wago.tools (pack.cpp)
add_executable(RetailPackBuild "${CMAKE_SOURCE_DIR}/Tools/RetailAssets/pack.cpp")
target_link_libraries(RetailPackBuild PRIVATE RetailAssets urlmon)
enable_testing()
add_test(NAME retail-assets COMMAND RetailAssetTests)
