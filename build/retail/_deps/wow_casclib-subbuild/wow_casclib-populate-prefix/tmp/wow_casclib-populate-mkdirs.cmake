# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "D:/GitHub/wowdecomp-256-bit/build/retail/_deps/wow_casclib-src")
  file(MAKE_DIRECTORY "D:/GitHub/wowdecomp-256-bit/build/retail/_deps/wow_casclib-src")
endif()
file(MAKE_DIRECTORY
  "D:/GitHub/wowdecomp-256-bit/build/retail/_deps/wow_casclib-build"
  "D:/GitHub/wowdecomp-256-bit/build/retail/_deps/wow_casclib-subbuild/wow_casclib-populate-prefix"
  "D:/GitHub/wowdecomp-256-bit/build/retail/_deps/wow_casclib-subbuild/wow_casclib-populate-prefix/tmp"
  "D:/GitHub/wowdecomp-256-bit/build/retail/_deps/wow_casclib-subbuild/wow_casclib-populate-prefix/src/wow_casclib-populate-stamp"
  "D:/GitHub/wowdecomp-256-bit/build/retail/_deps/wow_casclib-subbuild/wow_casclib-populate-prefix/src"
  "D:/GitHub/wowdecomp-256-bit/build/retail/_deps/wow_casclib-subbuild/wow_casclib-populate-prefix/src/wow_casclib-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/GitHub/wowdecomp-256-bit/build/retail/_deps/wow_casclib-subbuild/wow_casclib-populate-prefix/src/wow_casclib-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/GitHub/wowdecomp-256-bit/build/retail/_deps/wow_casclib-subbuild/wow_casclib-populate-prefix/src/wow_casclib-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
