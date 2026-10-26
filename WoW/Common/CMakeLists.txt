file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_library(WoWCommon STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(WoWCommon
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

# Custom: retail race models and customization (ComponentCore/RetailRace.cpp)
if (WOWDECOMP_RETAIL_ASSETS)
    target_link_libraries(WoWCommon PRIVATE RetailAssets)
endif ()
