cmake_minimum_required(VERSION 3.22)
project(cowel)

option(COWEL_ENABLE_ASAN "Enable AddressSanitizer (and UBSan where configured)" OFF)
option(COWEL_USE_LIBCXX "Use libc++" OFF)
option(COWEL_ENABLE_COVERAGE "Enable LLVM source-based coverage" OFF)

if(COWEL_ENABLE_ASAN)
    set(ULIGHT_ENABLE_ASAN ON)
endif()

if(COWEL_USE_LIBCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_CXX_LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++ -lc++abi")
endif()

add_subdirectory(ulight)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BOOST_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/boost")

if(NOT DEFINED EMSCRIPTEN)
    if(NOT EXISTS "${BOOST_SOURCE_DIR}")
        message(STATUS "Boost not found. Installing boost dependencies...")
        execute_process(
            COMMAND bash "${CMAKE_SOURCE_DIR}/tools/boost-install.sh" "${BOOST_SOURCE_DIR}"
            RESULT_VARIABLE BOOST_INSTALL_RESULT
        )
        if(NOT BOOST_INSTALL_RESULT EQUAL 0)
            message(FATAL_ERROR "Boost installation failed with exit code ${BOOST_INSTALL_RESULT}")
        endif()
        message(STATUS "Boost installation completed successfully")
    endif()

    # Standalone mode means that Boost libraries won't depend on other Boost libraries.
    set(BOOST_REGEX_STANDALONE ON CACHE BOOL "Enable Boost.Regex standalone mode" FORCE)
    set(BOOST_MP_STANDALONE ON CACHE BOOL "Enable Boost.Multiprecision standalone mode" FORCE)

    add_subdirectory("${BOOST_SOURCE_DIR}/libs/config")
    add_subdirectory("${BOOST_SOURCE_DIR}/libs/multiprecision")
    add_subdirectory("${BOOST_SOURCE_DIR}/libs/regex")

    # Mark Boost includes as system headers to suppress warnings.
    set_target_properties(boost_config boost_multiprecision boost_regex PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:INTERFACE_INCLUDE_DIRECTORIES>
    )

    find_package(ICU COMPONENTS data i18n uc REQUIRED)

    include(FetchContent)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
    )
    set(INSTALL_GTEST OFF)
    set(BUILD_GMOCK OFF)
    set(BUILD_GTEST ON)
    FetchContent_MakeAvailable(googletest)
endif()

include(FetchContent)
FetchContent_Declare(
    tayweeargs
    GIT_REPOSITORY https://github.com/Taywee/args.git
    GIT_TAG        6.4.7
)
set(ARGS_BUILD_EXAMPLE OFF CACHE BOOL "Build args examples" FORCE)
set(ARGS_BUILD_UNITTESTS OFF CACHE BOOL "Build args unit tests" FORCE)
FetchContent_MakeAvailable(tayweeargs)

FetchContent_Declare(
    code_point_names
    GIT_REPOSITORY https://github.com/eisenwave/code-point-names.git
    GIT_TAG        513a21d81ac9cc82690e15cf747ecea6bf1a14ca
)
FetchContent_MakeAvailable(code_point_names)

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
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/file-to-array.py"
                "${INPUT_FILE}" "${OUTPUT_CPP}" "${VAR_NAME}"
        DEPENDS "${INPUT_FILE}" ${CMAKE_CURRENT_LIST_DIR}/tools/file-to-array.py
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

if(DEFINED EMSCRIPTEN)
    set(WARNING_OPTIONS ${LLVM_WARNING_OPTIONS})
    if(COWEL_ENABLE_ASAN)
        set(SANITIZER_OPTIONS -fsanitize=address,undefined)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(WARNING_OPTIONS -Wall -Wimplicit-fallthrough -Wpedantic -Wextra -Wconversion -Wmissing-declarations)
    if(COWEL_ENABLE_ASAN)
        set(SANITIZER_OPTIONS -fsanitize=address,undefined)
    endif()
    add_compile_definitions($<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(WARNING_OPTIONS ${LLVM_WARNING_OPTIONS})
    if(COWEL_ENABLE_ASAN)
        set(SANITIZER_OPTIONS -fsanitize=address,undefined)
    endif()
    add_compile_definitions($<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(WARNING_OPTIONS -W4)
    if(COWEL_ENABLE_ASAN)
        set(SANITIZER_OPTIONS -fsanitize=address,undefined)
    endif()
endif()

if(COWEL_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(COVERAGE_OPTIONS -fprofile-instr-generate -fcoverage-mapping)
    else()
        message(FATAL_ERROR "COWEL_ENABLE_COVERAGE currently requires Clang")
    endif()
endif()

set(INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/engine/include")
include_directories(${INCLUDE_DIRS})

set(COMMON_LIBRARY_SOURCES
    engine/include/cowel/policy/content_policy.hpp
    engine/include/cowel/policy/factory.hpp
    engine/include/cowel/policy/html.hpp
    engine/include/cowel/policy/html_literal.hpp
    engine/include/cowel/policy/ignorant.hpp
    engine/include/cowel/policy/literally.hpp
    engine/include/cowel/policy/paragraph_split.hpp
    engine/include/cowel/policy/phantom.hpp
    engine/include/cowel/policy/plaintext.hpp
    engine/include/cowel/policy/syntax_highlight.hpp
    engine/include/cowel/policy/text_buffer.hpp
    engine/include/cowel/policy/unprocessed.hpp

    engine/include/cowel/util/annotated_string.hpp
    engine/include/cowel/util/annotation_span.hpp
    engine/include/cowel/util/ansi.hpp
    engine/include/cowel/util/ascii_algorithm.hpp
    engine/include/cowel/util/assert.hpp
    engine/include/cowel/util/bit_permutations.hpp
    engine/include/cowel/util/buffer.hpp
    engine/include/cowel/util/case_transform.hpp
    engine/include/cowel/util/char_sequence.hpp
    engine/include/cowel/util/char_sequence_factory.hpp
    engine/include/cowel/util/char_sequence_ops.hpp
    engine/include/cowel/util/chars.hpp
    engine/include/cowel/util/code_point_names.hpp
    engine/include/cowel/util/draft_uris.hpp
    engine/include/cowel/util/fixed_string.hpp
    engine/include/cowel/util/from_chars.hpp
    engine/include/cowel/util/function_ref.hpp
    engine/include/cowel/util/html.hpp
    engine/include/cowel/util/html_entities.hpp
    engine/include/cowel/util/html_names.hpp
    engine/include/cowel/util/html_writer.hpp
    engine/include/cowel/util/io.hpp
    engine/include/cowel/util/levenshtein.hpp
    engine/include/cowel/util/levenshtein_utf8.hpp
    engine/include/cowel/util/meta.hpp
    engine/include/cowel/util/result.hpp
    engine/include/cowel/util/severity.hpp
    engine/include/cowel/util/source_position.hpp
    engine/include/cowel/util/string_or_char_consumer.hpp
    engine/include/cowel/util/stringify.hpp
    engine/include/cowel/util/strings.hpp
    engine/include/cowel/util/to_chars.hpp
    engine/include/cowel/util/transparent_comparison.hpp
    engine/include/cowel/util/tty.hpp
    engine/include/cowel/util/typo.hpp
    engine/include/cowel/util/unicode.hpp
    engine/include/cowel/util/url_encode.hpp

    engine/include/cowel/syntax/ast.hpp
    engine/include/cowel/syntax/ast_fwd.hpp
    engine/include/cowel/syntax/expression_kind.hpp
    engine/include/cowel/syntax/lex.hpp
    engine/include/cowel/syntax/parse_utils.hpp
    engine/include/cowel/syntax/parse.hpp

    engine/include/cowel/assets.hpp
    engine/include/cowel/big_int.hpp
    engine/include/cowel/builtin_directive_set.hpp
    engine/include/cowel/call_stack.hpp
    engine/include/cowel/collecting_logger.hpp
    engine/include/cowel/content_status.hpp
    engine/include/cowel/context.hpp
    engine/include/cowel/cowel_lib.hpp
    engine/include/cowel/cowel.h
    engine/include/cowel/diagnostic_highlight.hpp
    engine/include/cowel/diagnostic.hpp
    engine/include/cowel/directive_behavior.hpp
    engine/include/cowel/directive_display.hpp
    engine/include/cowel/directive_processing.hpp
    engine/include/cowel/document_generation.hpp
    engine/include/cowel/document_sections.hpp
    engine/include/cowel/fwd.hpp
    engine/include/cowel/integration_testing.hpp
    engine/include/cowel/invocation.hpp
    engine/include/cowel/json.hpp
    engine/include/cowel/lsp.hpp
    engine/include/cowel/memory_resources.hpp
    engine/include/cowel/output_language.hpp
    engine/include/cowel/parameters.hpp
    engine/include/cowel/print.hpp
    engine/include/cowel/regexp.hpp
    engine/include/cowel/relative_file_loader.hpp
    engine/include/cowel/services.hpp
    engine/include/cowel/settings.hpp
    engine/include/cowel/string_kind.hpp
    engine/include/cowel/theme_to_css.hpp
    engine/include/cowel/type.hpp
    engine/include/cowel/ulight_highlighter.hpp
    engine/include/cowel/value.hpp

    engine/src/util/case_transform.cpp
    engine/src/util/charconv.cpp
    engine/src/util/code_point_by_name.cpp
    engine/src/util/code_point_name.cpp
    engine/src/util/draft_uris.cpp
    engine/src/util/html_entities.cpp
    engine/src/util/typo.cpp

    engine/src/directives/alias.cpp
    engine/src/directives/bibliography.cpp
    engine/src/directives/code_point.cpp
    engine/src/directives/files.cpp
    engine/src/directives/heading.cpp
    engine/src/directives/html_entity.cpp
    engine/src/directives/invoke.cpp
    engine/src/directives/literal.cpp
    engine/src/directives/macros.cpp
    engine/src/directives/math.cpp
    engine/src/directives/paragraph.cpp
    engine/src/directives/passthrough.cpp
    engine/src/directives/policies.cpp
    engine/src/directives/reference.cpp
    engine/src/directives/str.cpp
    engine/src/directives/syntax_highlight.cpp
    engine/src/directives/variables.cpp
    engine/src/directives/wg21.cpp

    engine/src/syntax/build_ast.cpp
    engine/src/syntax/lex.cpp
    engine/src/syntax/parse_utils.cpp
    engine/src/syntax/parse.cpp

    engine/src/cli_options.cpp
    engine/src/content_policies.cpp
    engine/src/cowel_lib.cpp
    engine/src/directive_processing.cpp
    engine/src/builtin_directive_set.cpp
    engine/src/document_generation.cpp
    engine/src/json.cpp
    engine/src/parameters.cpp
    engine/src/print.cpp
    engine/src/semantics.cpp
    engine/src/services.cpp
    engine/src/theme_to_css.cpp

    # generated sources
    "${LIGHT_DARK_JS_CPP}"
    "${MAIN_CSS_CPP}"
    "${SETTINGS_WIDGET_HTML_CPP}"
    "${WG21_JSON_CPP}"
)

set(NATIVE_LIBRARY_SOURCES
    ${COMMON_LIBRARY_SOURCES}
    engine/src/util/io.cpp
    engine/src/util/tty.cpp

    engine/src/big_int_boost.cpp
    engine/src/regexp_boost.cpp
    engine/src/relative_file_loader.cpp
)

if(DEFINED EMSCRIPTEN)
    set(COMMON_WASM_SOURCES
        ${COMMON_LIBRARY_SOURCES}
        bindings/node/src/cpp/big_int_wasm.cpp
        bindings/node/src/cpp/regexp_wasm.cpp
        bindings/node/src/cpp/wasm_stubs.cpp
    )
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
        ${COMMON_WASM_SOURCES}
        bindings/node/src/cpp/wasm.cpp
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
    target_compile_definitions(cowel-wasm PUBLIC COWEL_BUILD_WASM=1)

    target_compile_options(ulight PRIVATE
        "-fno-exceptions"
    )
    target_link_options(ulight PRIVATE
        "-fno-exceptions"
    )
    target_link_libraries(cowel-wasm PRIVATE ulight args)
    target_link_libraries(cowel-wasm PRIVATE get_code_point_name::get_code_point_name)

    add_executable(cowel-lsp
        ${COMMON_WASM_SOURCES}
        bindings/lsp/lsp.cpp
    )
    set_target_properties(cowel-lsp PROPERTIES
        OUTPUT_NAME "cowel-lsp"
        SUFFIX ".wasm"
    )
    target_compile_options(cowel-lsp PRIVATE ${WASM_COMPILE_OPTIONS})
    target_link_options(cowel-lsp PRIVATE ${WASM_LINK_OPTIONS})
    target_compile_definitions(cowel-lsp PUBLIC COWEL_BUILD_WASM=1)
    target_link_libraries(cowel-lsp PRIVATE ulight args)
    target_link_libraries(cowel-lsp PRIVATE get_code_point_name::get_code_point_name)

    set(NPM_DIR "${CMAKE_BINARY_DIR}/npm")
    set(LSP_WASM_DIR "${CMAKE_BINARY_DIR}/lsp-wasm")
    set(NPM_TEST_DIR "${CMAKE_BINARY_DIR}/test")
    set(NODE_BINDINGS_DIR "${CMAKE_CURRENT_LIST_DIR}/bindings/node")
    find_program(NPX_EXECUTABLE npx REQUIRED)
    find_program(NODE_EXECUTABLE node REQUIRED)

    add_custom_target(cowel-npm ALL
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${NPM_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${NPM_TEST_DIR}"
        COMMAND "${NPX_EXECUTABLE}" --prefix "${NODE_BINDINGS_DIR}" tsc
            -p "${NODE_BINDINGS_DIR}/tsconfig.json"
            --outDir "${NPM_DIR}"
            --tsBuildInfoFile "${NPM_DIR}/.tsbuildinfo"
        COMMAND "${NPX_EXECUTABLE}" --prefix "${NODE_BINDINGS_DIR}" tsc
            -p "${NODE_BINDINGS_DIR}/tsconfig.test.json"
            --outDir "${NPM_TEST_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/CHANGELOG.md"
            "${CMAKE_CURRENT_BINARY_DIR}/cowel.wasm"
            "${NPM_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_LIST_DIR}/docs/npm"
            "${NPM_DIR}"
        COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/transform-package-json.js"
            "${NODE_BINDINGS_DIR}/package.json"
            "${NPM_DIR}/package.json"
        BYPRODUCTS
            "${NPM_DIR}"
            "${NPM_TEST_DIR}"
            "${NPM_DIR}/cowel.wasm"
            "${CMAKE_BINARY_DIR}/npm/.tsbuildinfo"
        DEPENDS cowel-wasm
        COMMENT "Preparing npm package in ${NPM_DIR}"
    )

    add_custom_target(cowel-lsp-wasm ALL
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${LSP_WASM_DIR}"
        COMMAND "${NPX_EXECUTABLE}" --prefix "${NODE_BINDINGS_DIR}" tsc
            -p "${NODE_BINDINGS_DIR}/tsconfig.json"
            --outDir "${LSP_WASM_DIR}"
            --tsBuildInfoFile "${LSP_WASM_DIR}/.tsbuildinfo"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/cowel-lsp.wasm"
            "${LSP_WASM_DIR}"
        BYPRODUCTS
            "${LSP_WASM_DIR}"
            "${LSP_WASM_DIR}/cowel-lsp.wasm"
            "${LSP_WASM_DIR}/lsp-wasm-runner.js"
            "${LSP_WASM_DIR}/.tsbuildinfo"
        DEPENDS cowel-lsp
        COMMENT "Preparing LSP WASM artifacts in ${LSP_WASM_DIR}"
    )
else()
    add_library(cowel STATIC ${NATIVE_LIBRARY_SOURCES})
    target_link_libraries(cowel ulight args)
    target_link_libraries(cowel get_code_point_name::get_code_point_name)
    target_link_libraries(cowel boost_config boost_multiprecision boost_regex)
    target_link_libraries(cowel ICU::data ICU::i18n ICU::uc)
    target_compile_options(cowel PUBLIC ${WARNING_OPTIONS} ${SANITIZER_OPTIONS} ${COVERAGE_OPTIONS})
    target_link_options(cowel PUBLIC ${SANITIZER_OPTIONS} ${COVERAGE_OPTIONS})
    target_compile_definitions(cowel PUBLIC COWEL_BUILD_NATIVE=1)

    add_executable(cowel-cli ${HEADERS}
        bindings/native/src/cli.cpp
    )
    target_link_libraries(cowel-cli cowel ulight args)

    add_executable(cowel-test
        ${HEADERS}
        engine/test/src/document_file_testing.cpp
        engine/test/src/main.cpp
        engine/test/src/test_char_sequence.cpp
        engine/test/src/test_chars_strings.cpp
        engine/test/src/test_code_point_names.cpp
        engine/test/src/test_diff.cpp
        engine/test/src/test_document_generation.cpp
        engine/test/src/test_draft_uris.cpp
        engine/test/src/test_gc_ref.cpp
        engine/test/src/test_html_writer.cpp
        engine/test/src/test_json.cpp
        engine/test/src/test_levenshtein.cpp
        engine/test/src/test_lsp.cpp
        engine/test/src/test_lexing.cpp
        engine/test/src/test_math.cpp
        engine/test/src/test_parsing.cpp
        engine/test/src/test_regexp.cpp
        engine/test/src/test_small_vector.cpp
        engine/test/src/test_to_chars.cpp
        engine/test/src/test_typo.cpp
        engine/test/src/test_valid.cpp
        engine/test/src/test_value.cpp
    )

    include(GoogleTest)
    enable_testing()

    target_compile_options(cowel-test PRIVATE ${COVERAGE_OPTIONS})
    target_link_options(cowel-test PRIVATE ${COVERAGE_OPTIONS})
    target_link_libraries(cowel-test cowel ulight gtest gtest_main)
    gtest_discover_tests(cowel-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        DISCOVERY_TIMEOUT 30
        PROPERTIES LABELS cowel
    )
endif()
