# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-src")
  file(MAKE_DIRECTORY "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-src")
endif()
file(MAKE_DIRECTORY
  "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-build"
  "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-subbuild/tayweeargs-populate-prefix"
  "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-subbuild/tayweeargs-populate-prefix/tmp"
  "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-subbuild/tayweeargs-populate-prefix/src/tayweeargs-populate-stamp"
  "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-subbuild/tayweeargs-populate-prefix/src"
  "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-subbuild/tayweeargs-populate-prefix/src/tayweeargs-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-subbuild/tayweeargs-populate-prefix/src/tayweeargs-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-subbuild/tayweeargs-populate-prefix/src/tayweeargs-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
