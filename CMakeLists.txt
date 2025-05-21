cmake_minimum_required(VERSION 3.22)
project(cowel)

add_subdirectory(ulight)

set(CMAKE_CXX_STANDARD 23)
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
    add_compile_definitions($<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(WARNING_OPTIONS -W4)
#    set(SANITIZER_OPTIONS -fsanitize:address)
endif()

set(INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
include_directories(${INCLUDE_DIRS})

add_library(cowel STATIC
    src/main/cpp/util/draft_uris.cpp
    src/main/cpp/util/html_writer.cpp
    src/main/cpp/util/html_entities.cpp
    src/main/cpp/util/io.cpp
    src/main/cpp/util/tty.cpp
    src/main/cpp/util/typo.cpp

    src/main/cpp/directives/bibliography.cpp
    src/main/cpp/directives/code_point.cpp
    src/main/cpp/directives/heading.cpp
    src/main/cpp/directives/html_entity.cpp
    src/main/cpp/directives/literal.cpp
    src/main/cpp/directives/macros.cpp
    src/main/cpp/directives/math.cpp
    src/main/cpp/directives/passthrough.cpp
    src/main/cpp/directives/reference.cpp
    src/main/cpp/directives/syntax_highlight.cpp
    src/main/cpp/directives/variables.cpp
    src/main/cpp/directives/wg21.cpp

    src/main/cpp/argument_matching.cpp
    src/main/cpp/build_ast.cpp
    src/main/cpp/directive_processing.cpp
    src/main/cpp/builtin_directive_set.cpp
    src/main/cpp/document_generation.cpp
    src/main/cpp/json.cpp
    src/main/cpp/parse_utils.cpp
    src/main/cpp/parse.cpp
    src/main/cpp/print.cpp
    src/main/cpp/services.cpp
    src/main/cpp/theme_to_css.cpp
)

target_link_libraries(cowel ulight)
target_compile_options(cowel PUBLIC ${WARNING_OPTIONS} ${SANITIZER_OPTIONS})
target_link_options(cowel PUBLIC ${SANITIZER_OPTIONS})

if(DEFINED EMSCRIPTEN)
    # https://stunlock.gg/posts/emscripten_with_cmake/
    add_executable(cowel-wasm ${HEADERS}
        src/main/cpp/wasm.cpp
    )
    target_link_libraries(cowel-wasm bm)
    set_target_properties(cowel-wasm PROPERTIES
        OUTPUT_NAME "bm"
        SUFFIX ".wasm"
    )
    target_compile_options(cowel-wasm PRIVATE
        # the SHELL option group prevents de-duplication
        # https://cmake.org/cmake/help/latest/prop_tgt/COMPILE_OPTIONS.html#option-de-duplication
        "SHELL:-s SIDE_MODULE=0"
        "-stdlib=libc++"
        "-Os"
#       "-fno-exceptions"
    )
    target_link_options(cowel-wasm PRIVATE
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
    add_executable(cowel-cli ${HEADERS}
        src/main/cpp/cli.cpp
    )
    target_link_libraries(cowel-cli cowel ulight)

    add_executable(cowel-test ${HEADERS}
        src/test/cpp/document_file_testing.cpp
        src/test/cpp/main.cpp
        src/test/cpp/test_chars_strings.cpp
        src/test/cpp/test_document_generation.cpp
        src/test/cpp/test_draft_uris.cpp
        src/test/cpp/test_html_writer.cpp
        src/test/cpp/test_levenshtein.cpp
        src/test/cpp/test_parsing.cpp
        src/test/cpp/test_to_chars.cpp
        src/test/cpp/test_typo.cpp
        src/test/cpp/test_valid.cpp
    )
    target_link_libraries(cowel-test cowel ulight GTest::GTest GTest::Main)
endif()
