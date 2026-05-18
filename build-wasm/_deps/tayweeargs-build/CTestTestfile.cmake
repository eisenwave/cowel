# CMake generated Testfile for 
# Source directory: /home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-src
# Build directory: /home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test "/usr/bin/nodejs" "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-build/argstest.js")
set_tests_properties(test PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-src/CMakeLists.txt;90;add_test;/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-src/CMakeLists.txt;0;")
add_test(test-multiple-inclusion "/usr/bin/nodejs" "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-build/argstest-multiple-inclusion.js")
set_tests_properties(test-multiple-inclusion PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-src/CMakeLists.txt;91;add_test;/home/runner/work/cowel/cowel/build-wasm/_deps/tayweeargs-src/CMakeLists.txt;0;")
