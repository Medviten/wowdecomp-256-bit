# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 4.5.0-rc1)

# Reject any attempt to use a toolchain file. We must not use one because
# we could be downloading it here. If the CMAKE_TOOLCHAIN_FILE environment
# variable is set, the cache variable will have been initialized from it.
unset(CMAKE_TOOLCHAIN_FILE CACHE)
unset(ENV{CMAKE_TOOLCHAIN_FILE})

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(wow_casclib-populate NONE)



include(ExternalProject)
ExternalProject_Add(wow_casclib-populate
                     "UPDATE_DISCONNECTED" "False" "DOWNLOAD_EXTRACT_TIMESTAMP" "TRUE" "EXTERNALPROJECT_INTERNAL_ARGUMENT_SEPARATOR" "URL" "https://codeload.github.com/ladislav-zezula/CascLib/zip/38a34665624b8775bb875274b36191b21c38d97b" "URL_HASH" "SHA256=1b59deebc82a4eb75c6e9ef463a7644b22231a4b3b0657ad9afe0dd5205fee3f"
                    SOURCE_DIR          "D:/GitHub/wowdecomp-256-bit/build/retail/_deps/wow_casclib-src"
                    BINARY_DIR          "D:/GitHub/wowdecomp-256-bit/build/retail/_deps/wow_casclib-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
                    USES_TERMINAL_PATCH     YES
)


