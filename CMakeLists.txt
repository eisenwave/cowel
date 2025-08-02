cmake_minimum_required(VERSION 3.22)
project(cowel)

add_subdirectory(ulight)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED EMSCRIPTEN)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP true
    )

    set(INSTALL_GTEST OFF)
    set(BUILD_GMOCK OFF)
    set(BUILD_GTEST ON)
    FetchContent_MakeAvailable(googletest)
endif()

find_package(Python3 REQUIRED)

set(ASSETS_DIR "${CMAKE_CURRENT_LIST_DIR}/assets")
set(THEMES_DIR "${CMAKE_CURRENT_LIST_DIR}/ulight/themes")
set(ASSET_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

function(embed_asset INPUT_FILE VAR_NAME OUT_VAR)
    get_filename_component(FILENAME_WE "${INPUT_FILE}" NAME_WE)
    set(OUTPUT_CPP "${ASSET_OUTPUT_DIR}/${VAR_NAME}.cpp")

    add_custom_command(
        OUTPUT ${OUTPUT_CPP}
        COMMAND "${CMAKE_COMMAND}" -E make_directory ${ASSET_OUTPUT_DIR}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/src/util/file-to-array.py"
                "${INPUT_FILE}" "${OUTPUT_CPP}" "${VAR_NAME}"
        DEPENDS "${INPUT_FILE}" ${CMAKE_CURRENT_LIST_DIR}/src/util/file-to-array.py
        COMMENT "Embedding asset ${INPUT_FILE} as ${VAR_NAME}"
    )

    set(${OUT_VAR} "${OUTPUT_CPP}" PARENT_SCOPE)
endfunction()

embed_asset("${ASSETS_DIR}/light-dark.js" light_dark_js LIGHT_DARK_JS_CPP)
embed_asset("${ASSETS_DIR}/main.css" main_css MAIN_CSS_CPP)
embed_asset("${ASSETS_DIR}/settings-widget.html" settings_widget_html SETTINGS_WIDGET_HTML_CPP)
embed_asset("${THEMES_DIR}/wg21.json" wg21_json WG21_JSON_CPP)

set(LLVM_WARNING_OPTIONS
  -Wall
  -Wimplicit-fallthrough
  -Wpedantic
  -Wextra
  -Wconversion
  -Wmissing-declarations
  -Wunreachable-code
  -Wunreachable-code-return
  -Wno-bit-int-extension)

set(ASAN_ENABLED OFF)

if(DEFINED EMSCRIPTEN)
    set(WARNING_OPTIONS ${LLVM_WARNING_OPTIONS})
    if(ASAN_ENABLED)
        set(SANITIZER_OPTIONS -fsanitize=address,undefined)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(WARNING_OPTIONS -Wall -Wimplicit-fallthrough -Wpedantic -Wextra -Wconversion -Wmissing-declarations)
    if(ASAN_ENABLED)
        set(SANITIZER_OPTIONS -fsanitize=address,undefined)
    endif()
    add_compile_definitions($<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(WARNING_OPTIONS ${LLVM_WARNING_OPTIONS})
    if(ASAN_ENABLED)
        set(SANITIZER_OPTIONS -fsanitize=address,undefined)
    endif()
    add_compile_definitions($<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(WARNING_OPTIONS -W4)
    if(ASAN_ENABLED)
        set(SANITIZER_OPTIONS -fsanitize=address,undefined)
    endif()
endif()

set(INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
include_directories(${INCLUDE_DIRS})

set(COMMON_LIBRARY_SOURCES
    src/main/cpp/util/code_point_names.cpp
    src/main/cpp/util/draft_uris.cpp
    src/main/cpp/util/html_entities.cpp
    src/main/cpp/util/typo.cpp

    src/main/cpp/directives/bibliography.cpp
    src/main/cpp/directives/code_point.cpp
    src/main/cpp/directives/files.cpp
    src/main/cpp/directives/heading.cpp
    src/main/cpp/directives/html_entity.cpp
    src/main/cpp/directives/literal.cpp
    src/main/cpp/directives/macros.cpp
    src/main/cpp/directives/math.cpp
    src/main/cpp/directives/paragraph.cpp
    src/main/cpp/directives/passthrough.cpp
    src/main/cpp/directives/policies.cpp
    src/main/cpp/directives/reference.cpp
    src/main/cpp/directives/syntax_highlight.cpp
    src/main/cpp/directives/variables.cpp
    src/main/cpp/directives/wg21.cpp

    src/main/cpp/argument_matching.cpp
    src/main/cpp/build_ast.cpp
    src/main/cpp/content_policies.cpp
    src/main/cpp/cowel_lib.cpp
    src/main/cpp/directive_processing.cpp
    src/main/cpp/builtin_directive_set.cpp
    src/main/cpp/document_generation.cpp
    src/main/cpp/json.cpp
    src/main/cpp/parse_utils.cpp
    src/main/cpp/parse.cpp
    src/main/cpp/print.cpp
    src/main/cpp/services.cpp
    src/main/cpp/theme_to_css.cpp

    # generated sources
    "${LIGHT_DARK_JS_CPP}"
    "${MAIN_CSS_CPP}"
    "${SETTINGS_WIDGET_HTML_CPP}"
    "${WG21_JSON_CPP}"
)

set(NATIVE_LIBRARY_SOURCES
    ${COMMON_LIBRARY_SOURCES}

    src/main/cpp/util/io.cpp
    src/main/cpp/util/tty.cpp
)

if(DEFINED EMSCRIPTEN)
    set(WASM_COMPILE_OPTIONS
        # the SHELL option group prevents de-duplication
        # https://cmake.org/cmake/help/latest/prop_tgt/COMPILE_OPTIONS.html#option-de-duplication
        "-stdlib=libc++"
        "-fno-exceptions"
        "-Os"
        ${WARNING_OPTIONS}
    )
    set(WASM_LINK_OPTIONS
        "-fno-exceptions"
        "-Os"
        "-stdlib=libc++"
        "-static-libc++"
        "SHELL:-s WASM=1"
        "SHELL:-s ENVIRONMENT=web"
        "SHELL:-s IMPORTED_MEMORY=0"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s ALLOW_TABLE_GROWTH=1"
        "SHELL:-s AUTO_JS_LIBRARIES=0"
        "SHELL:-s STANDALONE_WASM=0"
        "--no-entry"
    )

    add_executable(cowel-wasm
        ${COMMON_LIBRARY_SOURCES}
        src/main/cpp/wasm.cpp
    )
    set_target_properties(cowel-wasm PROPERTIES
        OUTPUT_NAME "cowel"
        SUFFIX ".wasm"
    )
    target_compile_options(cowel-wasm PRIVATE
        ${WASM_COMPILE_OPTIONS}
        ${SANITIZER_OPTIONS}
    )
    target_link_options(cowel-wasm PRIVATE
        ${WASM_LINK_OPTIONS}
        ${SANITIZER_OPTIONS}
    )

    target_compile_options(ulight PRIVATE
        "-fno-exceptions"
    )
    target_link_options(ulight PRIVATE
        "-fno-exceptions"
    )
    target_link_libraries(cowel-wasm PRIVATE ulight)

    set(NPM_DIR "${CMAKE_BINARY_DIR}/npm")
    find_program(NPX_EXECUTABLE npx REQUIRED)
    find_program(NODE_EXECUTABLE node REQUIRED)

    add_custom_target(cowel-npm ALL
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${NPM_DIR}"
        COMMAND "${NPX_EXECUTABLE}" tsc
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/CHANGELOG.md"
            "${CMAKE_CURRENT_BINARY_DIR}/cowel.wasm"
            "${NPM_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_LIST_DIR}/docs/npm"
            "${NPM_DIR}"
        COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/src/util/transform-package-json.js"
            "${CMAKE_CURRENT_LIST_DIR}/package.json"
            "${NPM_DIR}/package.json"
        BYPRODUCTS
            "${NPM_DIR}"
            "${NPM_DIR}/cowel.wasm"
            "${CMAKE_BINARY_DIR}/.tsbuildinfo"
        DEPENDS cowel-wasm
        COMMENT "Preparing npm package in ${NPM_DIR}"
    )
else()
    add_library(cowel STATIC ${NATIVE_LIBRARY_SOURCES})
    target_link_libraries(cowel ulight)
    target_compile_options(cowel PUBLIC ${WARNING_OPTIONS} ${SANITIZER_OPTIONS})
    target_link_options(cowel PUBLIC ${SANITIZER_OPTIONS})

    add_executable(cowel-cli ${HEADERS}
        src/main/cpp/cli.cpp
    )
    target_link_libraries(cowel-cli cowel ulight)

    add_executable(cowel-test
        ${HEADERS}
        src/test/cpp/document_file_testing.cpp
        src/test/cpp/main.cpp
        src/test/cpp/test_char_sequence.cpp
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

    include(GoogleTest)
    enable_testing()

    target_link_libraries(cowel-test cowel ulight gtest gtest_main)
    gtest_discover_tests(cowel-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        DISCOVERY_TIMEOUT 30
    )
endif()
