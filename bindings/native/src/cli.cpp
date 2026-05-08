#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory_resource>
#include <string>
#include <string_view>
#include <utility>

#include "cowel/util/annotated_string.hpp"
#include "cowel/util/ansi.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/meta.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/assets.hpp"
#include "cowel/cowel.h"
#include "cowel/cowel_lib.hpp"
#include "cowel/fwd.hpp"
#include "cowel/memory_resources.hpp"
#include "cowel/print.hpp"
#include "cowel/relative_file_loader.hpp"
#include "cowel/services.hpp"

namespace cowel {
namespace {

[[nodiscard]]
std::u8string_view severity_highlight(Severity severity)
{
    const auto s = cowel_severity(severity);
    return s <= COWEL_SEVERITY_TRACE       ? ansi::black
        : s <= COWEL_SEVERITY_DEBUG        ? ansi::h_black
        : s <= COWEL_SEVERITY_INFO         ? ansi::blue
        : s <= COWEL_SEVERITY_SOFT_WARNING ? ansi::green
        : s <= COWEL_SEVERITY_WARNING      ? ansi::h_yellow
        : s <= COWEL_SEVERITY_ERROR        ? ansi::h_red
        : s <= COWEL_SEVERITY_FATAL        ? ansi::red
                                           : ansi::magenta;
}

[[nodiscard]]
constexpr File_Source_Span as_file_source_span(const cowel_diagnostic_location_u8& location)
{
    return {
        {
            { .line = location.line, .column = location.column, .begin = location.begin },
            location.length,
        },
        File_Id(location.file_id),
    };
}

[[nodiscard]]
constexpr std::u8string_view processing_status_name(cowel_processing_status status)
{
    switch (status) {
    case COWEL_PROCESSING_OK: return u8"ok";
    case COWEL_PROCESSING_BREAK: return u8"break";
    case COWEL_PROCESSING_ERROR: return u8"error";
    case COWEL_PROCESSING_ERROR_BREAK: return u8"error_break";
    case COWEL_PROCESSING_FATAL: return u8"fatal";
    }
    return u8"unknown";
}

struct Stderr_Logger {
    const Relative_File_Loader& file_loader;
    const std::u8string_view main_file_name;
    const std::u8string_view main_file_source;
    Diagnostic_String out;
    bool any_errors = false;
    bool colors_enabled = true;

    [[nodiscard]]
    constexpr Stderr_Logger(
        Relative_File_Loader& file_loader,
        std::u8string_view main_file_name,
        std::u8string_view main_file_source,
        std::pmr::memory_resource* memory,
        bool colors_enabled = true
    )
        : file_loader { file_loader }
        , main_file_name { main_file_name }
        , main_file_source { main_file_source }
        , out { memory }
        , colors_enabled { colors_enabled }
    {
    }

    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    void operator()(const cowel_diagnostic_u8& diagnostic)
    {
        const auto severity = Severity(diagnostic.severity);
        any_errors |= severity >= Severity::error;

        const auto get_file_entry
            = [&](const cowel_diagnostic_location_u8& location) -> File_Entry {
            COWEL_ASSERT(location.file_id >= -1);
            if (location.file_id < 0) {
                return {
                    .id = File_Id(location.file_id),
                    .source = main_file_source,
                    .name = main_file_name,
                };
            }
            const Owned_File_Entry& result = file_loader.at(File_Id(location.file_id));
            return {
                .id = File_Id(location.file_id),
                .source = as_u8string_view(result.text),
                .name = as_u8string_view(result.path_string),
            };
        };

        const cowel_diagnostic_location_u8* const stack = diagnostic.stack;
        const std::size_t stack_size = diagnostic.stack_size;
        const bool has_primary = stack_size != 0;

        File_Entry primary_file_entry {};
        File_Source_Span primary_location {};
        if (has_primary) {
            primary_file_entry = get_file_entry(stack[0]);
            primary_location = as_file_source_span(stack[0]);
        }

        if (colors_enabled) {
            out.append(severity_highlight(severity));
        }
        out.append(severity_tag(severity));
        if (colors_enabled) {
            out.append(ansi::reset);
        }
        out.append(u8": ");
        if (has_primary) {
            if (stack[0].length == 0) {
                print_location_of_file(out, primary_file_entry.name);
            }
            else {
                print_file_position(out, primary_file_entry.name, primary_location);
            }
        }
        out.append(u8' ');
        out.append(as_u8string_view(diagnostic.message));
        if (colors_enabled) {
            out.append(ansi::h_black);
        }
        out.append(u8" [");
        out.append(as_u8string_view(diagnostic.id));
        out.append(u8']');
        if (colors_enabled) {
            out.append(ansi::reset);
        }
        out.append(u8'\n');
        if (has_primary && stack[0].length != 0) {
            print_affected_line(out, primary_file_entry.source, primary_location);
        }

        for (std::size_t i = 1; i < stack_size; ++i) {
            const auto& stack_location = stack[i];
            const File_Entry stack_file_entry = get_file_entry(stack_location);
            const File_Source_Span stack_span = as_file_source_span(stack_location);

            if (colors_enabled) {
                out.append(ansi::h_white);
            }
            out.append(u8"NOTE");
            if (colors_enabled) {
                out.append(ansi::reset);
            }
            out.append(u8": ");
            if (stack_location.length == 0) {
                print_location_of_file(out, stack_file_entry.name);
            }
            else {
                print_file_position(out, stack_file_entry.name, stack_span);
            }
            out.append(u8" Expanded from here.\n");
            if (stack_location.length != 0) {
                print_affected_line(out, stack_file_entry.source, stack_span);
            }
        }

        print_code_string_stderr(out);
        out.clear();
    }
};

constexpr std::string_view help_text = R"(Usage: cowel run <input> <output> [options]

Commands:
  run <input> <output>  Processes a COWEL document

Options:
  -h, --help            Display this help menu
  -v, --version         Show version
  -l, --severity        Minimum (>=) severity for log messages
                        Choices: min, trace, debug, info, soft_warning,
                                 warning, error, fatal, none
                        Default: info
      --no-color        Disable colored output
)";

constexpr std::string_view version_text = "0.8.0\n";

int main(int argc, const char* const* const argv)
{
    cowel_parsed_cli_options_u8 opts
        = cowel_parse_cli_options_u8(argv + 1, static_cast<size_t>(argc - 1));

    struct Opts_Guard {
        cowel_parsed_cli_options_u8& opts;
        explicit Opts_Guard(cowel_parsed_cli_options_u8& o)
            : opts { o }
        {
        }
        Opts_Guard(const Opts_Guard&) = delete;
        Opts_Guard& operator=(const Opts_Guard&) = delete;
        ~Opts_Guard()
        {
            cowel_free_cli_options_u8(&opts);
        }
    } guard { opts };

    if (!opts.ok) {
        std::cerr << as_string_view(as_u8string_view(opts.error_message)) << '\n';
        return EXIT_FAILURE;
    }

    switch (opts.command) {
    case COWEL_CLI_COMMAND_NONE:
    case COWEL_CLI_COMMAND_HELP:
        std::cout << help_text;
        return opts.command == COWEL_CLI_COMMAND_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
    case COWEL_CLI_COMMAND_VERSION: std::cout << version_text; return EXIT_SUCCESS;
    case COWEL_CLI_COMMAND_RUN: break;
    }

    const auto in_path_u8 = as_u8string_view(opts.input);
    const auto out_path_u8 = as_u8string_view(opts.output);
    const bool colors_enabled = !opts.no_color;

    Global_Memory_Resource global_memory;
    std::pmr::unsynchronized_pool_resource memory { &global_memory };

    const Result<std::pmr::vector<char8_t>, IO_Error_Code> in_text
        = load_utf8_file(in_path_u8, &memory);
    if (!in_text) {
        Diagnostic_String error { &memory };
        print_io_error(error, in_path_u8, in_text.error());
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }
    const auto in_source = as_u8string_view(*in_text);

    auto in_path_directory = std::filesystem::path { as_string_view(in_path_u8) }.parent_path();
    Relative_File_Loader file_loader { std::move(in_path_directory), &memory };
    Stderr_Logger logger { file_loader, in_path_u8, in_source, &memory, colors_enabled };

    // TODO: allow custom ulight themes instead of hardcoding wg21.json

    const auto alloc_options = Allocator_Options::from_memory_resource(&memory);
    const auto load_file_ref = file_loader.as_cowel_load_file_fn();

    constexpr auto log_fn = [](Stderr_Logger* logger, const cowel_diagnostic_u8* diagnostic
                            ) noexcept -> void { (*logger)(*diagnostic); };
    const Function_Ref<void(const cowel_diagnostic_u8*) noexcept> log_ref
        = { const_v<log_fn>, &logger };

    const cowel_options_u8 options {
        .source = as_cowel_string_view(in_source),
        .highlight_theme_json = as_cowel_string_view(assets::wg21_json),
        .mode = COWEL_MODE_DOCUMENT,
        .min_log_severity = opts.min_severity,
        .preserved_variables = nullptr,
        .preserved_variables_size = 0,
        .consume_variables = nullptr,
        .consume_variables_data = nullptr,
        .alloc = alloc_options.alloc,
        .alloc_data = alloc_options.alloc_data,
        .free = alloc_options.free,
        .free_data = alloc_options.free_data,
        .load_file = load_file_ref.get_invoker(),
        .load_file_data = load_file_ref.get_entity(),
        .log = log_ref.get_invoker(),
        .log_data = log_ref.get_entity(),
        .highlighter = nullptr,
        .highlight_policy = COWEL_SYNTAX_HIGHLIGHT_POLICY_FALL_BACK,
        .preamble = {},
    };

    const cowel_gen_result_u8 result = cowel_generate_html_u8(&options);

    if (result.status != COWEL_PROCESSING_OK) {
        if (result.output.text != nullptr) {
            memory.deallocate(result.output.text, result.output.length, alignof(char8_t));
        }

        const auto status_name = processing_status_name(result.status);
        Diagnostic_String error { &memory };
        if (colors_enabled) {
            error.append(ansi::red);
        }
        error.append(u8"FATAL");
        if (colors_enabled) {
            error.append(ansi::reset);
        }
        error.append(u8" ");
        print_location_of_file(error, in_path_u8);
        error.append(u8" Generation exited with status ");
        const std::string status_value = std::to_string(static_cast<int>(result.status));
        error.append(as_u8string_view(status_value));
        error.append(u8" (");
        error.append(status_name);
        error.append(u8").");
        if (colors_enabled) {
            error.append(ansi::h_black);
        }
        error.append(u8" [status.");
        error.append(status_name);
        error.append(u8"]");
        if (colors_enabled) {
            error.append(ansi::reset);
        }
        error.append(u8'\n');
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }

    const std::string out_path = std::string(as_string_view(out_path_u8));
    const auto out_file = fopen_unique(out_path.c_str(), "wb");
    if (!out_file) {
        if (result.output.text != nullptr) {
            memory.deallocate(result.output.text, result.output.length, alignof(char8_t));
        }
        Diagnostic_String error { &memory };
        print_location_of_file(error, out_path_u8);
        error.append(u8" Failed to open file.");
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }

    if (result.output.text != nullptr) {
        std::fwrite(result.output.text, 1, result.output.length, out_file.get());
        memory.deallocate(result.output.text, result.output.length, alignof(char8_t));
    }

    return logger.any_errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

} // namespace
} // namespace cowel

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, const char* const* argv)
{
    return cowel::main(argc, argv);
}
