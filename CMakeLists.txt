cmake_minimum_required(VERSION 3.22)
project(mmml)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED EMSCRIPTEN)
    find_package(GTest REQUIRED)
    enable_testing()
endif()

set(LLVM_WARNING_OPTIONS -Wall -Wimplicit-fallthrough -Wpedantic -Wextra -Wconversion -Wmissing-declarations -Wno-bit-int-extension)

if(DEFINED EMSCRIPTEN)
    set(WARNING_OPTIONS ${LLVM_WARNING_OPTIONS})
#    set(SANITIZER_OPTIONS)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(WARNING_OPTIONS -Wall -Wimplicit-fallthrough -Wpedantic -Wextra -Wconversion -Wmissing-declarations)
#    set(SANITIZER_OPTIONS -fsanitize=address,undefined)
    add_compile_definitions($<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(WARNING_OPTIONS ${LLVM_WARNING_OPTIONS})
#    set(SANITIZER_OPTIONS -fsanitize=address,undefined)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(WARNING_OPTIONS -W4)
#    set(SANITIZER_OPTIONS -fsanitize:address)
endif()

set(INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
include_directories(${INCLUDE_DIRS})

add_library(mmml STATIC
    src/main/cpp/util/html_writer.cpp
    src/main/cpp/util/io.cpp
    src/main/cpp/util/strings.cpp
    src/main/cpp/util/tty.cpp

    src/main/cpp/argument_matching.cpp
    src/main/cpp/build_ast.cpp
    src/main/cpp/directive_processing.cpp
    src/main/cpp/directives.cpp
    src/main/cpp/document_generation.cpp
    src/main/cpp/parse_utils.cpp
    src/main/cpp/parse.cpp
    src/main/cpp/print.cpp
)

target_compile_options(mmml PUBLIC ${WARNING_OPTIONS} ${SANITIZER_OPTIONS})
target_link_options(mmml PUBLIC ${SANITIZER_OPTIONS})

if(DEFINED EMSCRIPTEN)
    # https://stunlock.gg/posts/emscripten_with_cmake/
    add_executable(mmml-wasm ${HEADERS}
        src/main/cpp/wasm.cpp
    )
    target_link_libraries(mmml-wasm bm)
    set_target_properties(mmml-wasm PROPERTIES
        OUTPUT_NAME "bm"
        SUFFIX ".wasm"
    )
    target_compile_options(mmml-wasm PRIVATE
        # the SHELL option group prevents de-duplication
        # https://cmake.org/cmake/help/latest/prop_tgt/COMPILE_OPTIONS.html#option-de-duplication
        "SHELL:-s SIDE_MODULE=0"
        "-stdlib=libc++"
        "-Os"
#       "-fno-exceptions"
    )
    target_link_options(mmml-wasm PRIVATE
        "-Os"
#       "-fno-exceptions"
        "-static-libc++"
        "-stdlib=libc++"
        "SHELL:-s WASM=1"
        "SHELL:-s ENVIRONMENT=web"
        "SHELL:-s AUTO_JS_LIBRARIES=0"
        "SHELL:-s SIDE_MODULE=0"
        "SHELL:-s STANDALONE_WASM=1"
        "SHELL:-s \"EXPORTED_FUNCTIONS=['_bm_foreign_alloc','_bm_foreign_free','_bm_plus','_bm_length_as_string','_bm_length_as_string_result','_bm_translate_code','_bm_translate_code_result','_bm_syntax_highlight','_bm_syntax_highlight_result']\""
        "SHELL:-s \"EXPORTED_RUNTIME_METHODS=[]\""
        "--no-entry"
    )

else(NOT DEFINED EMSCRIPTEN)
    add_executable(mmml-cli ${HEADERS}
        src/main/cpp/cli.cpp
    )

    add_executable(mmml-test ${HEADERS}
        src/test/cpp/document_file_testing.cpp
        src/test/cpp/main.cpp
        src/test/cpp/test_chars_strings.cpp
        src/test/cpp/test_parsing.cpp
        src/test/cpp/test_unicode.cpp
        src/test/cpp/test_valid.cpp
    )

    target_link_libraries(mmml-cli mmml)
    target_link_libraries(mmml-test mmml GTest::GTest GTest::Main)
endif()
