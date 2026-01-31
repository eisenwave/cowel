#include <cstddef>
#include <string_view>
#include <vector>

#include "cowel/util/annotated_string.hpp"

#include "cowel/cowel.h"
#include "cowel/diagnostic_highlight.hpp"
#include "cowel/memory_resources.hpp"
#include "cowel/print.hpp"

// The purpose of this cpp file is to accept imports
// which can be used as callbacks within the cowel.h API.
// This is necessary because it is currently not possible to create JS functions
// from scratch which can be used as function pointers within WASM directly;
// it can only be done with WASM exports/imports.

using namespace std::string_view_literals;

extern "C" {

__attribute__((import_module("env"), import_name("load_file"))) // NOLINT
cowel_file_result_u8
cowel_import_load_file_u8(const char8_t* path_text, size_t path_length, cowel_file_id relative_to);

__attribute__((import_module("env"), import_name("log"))) // NOLINT
void cowel_import_log_u8(const cowel_diagnostic_u8*);
}

#ifdef COWEL_EMSCRIPTEN
static_assert(sizeof(cowel_options_u8) == 88);
static_assert(alignof(cowel_options_u8) == 4);
static_assert(sizeof(cowel_gen_result) == 12);
static_assert(alignof(cowel_gen_result) == 4);
#endif

namespace {

cowel_file_result_u8
load_file_callback(const void*, cowel_string_view_u8 path, cowel_file_id relative_to) noexcept
{
    return cowel_import_load_file_u8(path.text, path.length, relative_to);
}

void log_callback(const void*, const cowel_diagnostic_u8* diagnostic) noexcept
{
    cowel_import_log_u8(diagnostic);
}

} // namespace

extern "C" {

COWEL_EXPORT
void init_options(
    cowel_options_u8* result,
    const char8_t* source_text,
    std::size_t source_length,
    cowel_mode mode,
    cowel_severity min_log_severity
) noexcept
{
    *result = cowel_options_u8 {
        .source = { source_text, source_length },
        // FIXME: embed highlight theme in this binary for now perhaps?
        .highlight_theme_json = { nullptr, 0 },
        .mode = mode,
        .min_log_severity = min_log_severity,
        .preserved_variables = nullptr,
        .preserved_variables_size = 0,
        .consume_variables = nullptr,
        .consume_variables_data = nullptr,
        .alloc = nullptr,
        .alloc_data = nullptr,
        .free = nullptr,
        .free_data = nullptr,
        .load_file = load_file_callback,
        .load_file_data = nullptr,
        .log = log_callback,
        .log_data = nullptr,
        .highlighter = nullptr,
        .highlight_policy = COWEL_SYNTAX_HIGHLIGHT_POLICY_FALL_BACK,
        .preamble = {},
    };
}

COWEL_EXPORT
void log_assertion_error(const cowel_assertion_error_u8* error)
{
    const std::u8string_view id
        = error->type == COWEL_ASSERTION_NOT_TRUE ? u8"assert.fail"sv : u8"assert.unreachable"sv;

    const cowel_diagnostic_u8 diagnostic {
        .severity = COWEL_SEVERITY_FATAL,
        .id = { id.data(), id.length() },
        .message = error->message,
        .file_name = error->file_name,
        .file_id = -1,
        .begin = 0,
        .length = 0,
        .line = error->line,
        .column = error->column,
    };
    cowel_import_log_u8(&diagnostic);
}

COWEL_EXPORT
void register_assertion_handler() noexcept
{
    cowel_set_assertion_handler_u8(&log_assertion_error);
}

COWEL_EXPORT
cowel_mutable_string_view_u8 generate_code_citation(
    const char8_t* source_text,
    size_t source_length,
    size_t line,
    size_t column,
    size_t begin,
    size_t length,
    bool colors
)
{
    // We cannot use assertions here
    // because this function is used by our assertion handler,
    // so preconditions need to be handled in a dirtier way.
    if (source_text == nullptr) {
        __builtin_trap();
    }
    if (begin >= source_length || column >= source_length) {
        __builtin_trap();
    }
    if (length == 0) {
        __builtin_trap();
    }

    static constinit cowel::Global_Memory_Resource memory;
    cowel::Basic_Annotated_String<char8_t, cowel::Diagnostic_Highlight> out { &memory };
    cowel::print_affected_line(
        out, { source_text, source_length }, { { line, column, begin }, length }
    );
    std::pmr::vector<char8_t> buffer { &memory };
    cowel::dump_code_string(buffer, out, colors);
    return cowel_alloc_text_u8({ buffer.data(), buffer.size() });
}

//
}
