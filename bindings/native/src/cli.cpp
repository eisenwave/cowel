#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory_resource>
#include <optional>
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
        std::optional<File_Source_Span> primary_location;
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
                print_file_position(out, primary_file_entry.name, *primary_location);
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
            print_affected_line(out, primary_file_entry.source, *primary_location);
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

constexpr std::string_view help_text = R"(Usage: cowel <command> <input> [output] [options]

Commands:
  run <input> <output>        Processes a COWEL document
  tokenize <input> [output]   Dumps the tokens of a COWEL document

Options:
  -h, --help                  Display this help menu
  -v, --version               Show version
  -l, --severity              Minimum (>=) severity for log messages
                              Choices: min, trace, debug, info, soft_warning,
                                       warning, error, fatal, none
                              Default: info
      --no-color              Disable colored output
)";

constexpr std::string_view version_text = "11.0.0-pre\n";

void log_cli_diagnostic(
    const Function_Ref<void(const cowel_diagnostic_u8*) noexcept>& log_ref,
    cowel_severity severity,
    std::u8string_view message,
    std::u8string_view id = u8"cli",
    std::u8string_view file = {}
)
{
    const std::u8string message_storage { message };
    const std::u8string id_storage { id };
    const std::u8string file_storage { file };
    const cowel_diagnostic_location_u8 location {
        .file_name = as_cowel_string_view(file_storage),
        .file_id = -1,
        .begin = 0,
        .length = 0,
        .line = 0,
        .column = 0,
    };
    const cowel_diagnostic_u8 diagnostic {
        .severity = severity,
        .id = as_cowel_string_view(id_storage),
        .message = as_cowel_string_view(message_storage),
        .stack = &location,
        .stack_size = 1,
    };
    log_ref(&diagnostic);
}

int run_run_command(
    const std::u8string_view in_source,
    const std::u8string_view in_path_u8,
    const std::u8string_view out_path_u8,
    const Allocator_Options& alloc_options,
    const Function_Ref<cowel_file_result_u8(cowel_string_view_u8, cowel_file_id) noexcept>
        load_file_ref,
    Stderr_Logger& logger,
    const Function_Ref<void(const cowel_diagnostic_u8*) noexcept> log_ref,
    const cowel_severity min_log_severity
)
{
    const cowel_options_u8 options {
        .source = as_cowel_string_view(in_source),
        .highlight_theme_json = as_cowel_string_view(assets::wg21_json),
        .mode = COWEL_MODE_DOCUMENT,
        .flags = 0,
        .min_log_severity = min_log_severity,
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

    cowel_gen_result_u8 result = cowel_generate_html_u8(&options);

    if (result.status != COWEL_PROCESSING_OK) {
        cowel_free_gen_result_u8(&options, &result);

        const auto status_name = processing_status_name(result.status);
        std::string status_message = "Generation exited with status ";
        status_message += std::to_string(int(result.status));
        status_message += " (";
        status_message += std::string(as_string_view(status_name));
        status_message += ").";
        const std::u8string message { status_message.begin(), status_message.end() };
        log_cli_diagnostic(log_ref, COWEL_SEVERITY_FATAL, message, u8"run", in_path_u8);
        return EXIT_FAILURE;
    }

    const std::string out_path = std::string(as_string_view(out_path_u8));
    const auto out_file = fopen_unique(out_path.c_str(), "wb");
    if (!out_file) {
        cowel_free_gen_result_u8(&options, &result);
        log_cli_diagnostic(log_ref, COWEL_SEVERITY_FATAL, u8"Failed to open output file.", u8"run");
        return EXIT_FAILURE;
    }

    if (result.output.text != nullptr) {
        std::fwrite(result.output.text, 1, result.output.length, out_file.get());
    }
    cowel_free_gen_result_u8(&options, &result);

    return logger.any_errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

int run_tokenize_command(
    const std::u8string_view in_source,
    const std::u8string_view out_path_u8,
    const Function_Ref<void(const cowel_diagnostic_u8*) noexcept> log_ref,
    const Allocator_Options& alloc_options,
    const bool colors_enabled,
    const cowel_severity min_log_severity
)
{
    const cowel_dump_tokens_options_u8 dump_options {
        .source = as_cowel_string_view(in_source),
        .alloc = alloc_options.alloc,
        .alloc_data = alloc_options.alloc_data,
        .free = alloc_options.free,
        .free_data = alloc_options.free_data,
        .log = log_ref.get_invoker(),
        .log_data = log_ref.get_entity(),
        .min_log_severity = min_log_severity,
        .no_color = !colors_enabled,
    };
    cowel_dump_tokens_result_u8 dump_result = cowel_dump_tokens_u8(&dump_options);
    if (dump_result.status != COWEL_PROCESSING_OK) {
        const auto status_name = processing_status_name(dump_result.status);
        std::string status_message = "Token dump exited with status ";
        status_message += std::to_string(int(dump_result.status));
        status_message += " (";
        status_message += std::string(as_string_view(status_name));
        status_message += ").";
        log_cli_diagnostic(
            log_ref, COWEL_SEVERITY_FATAL, as_u8string_view(status_message), u8"tokenize"
        );
        cowel_free_dump_tokens_result_u8(&dump_options, &dump_result);
        return EXIT_FAILURE;
    }

    if (out_path_u8.length() != 0) {
        const std::string out_path = std::string(as_string_view(out_path_u8));
        const auto out_file = fopen_unique(out_path.c_str(), "wb");
        if (!out_file) {
            log_cli_diagnostic(
                log_ref, COWEL_SEVERITY_FATAL, u8"Failed to open output file.", u8"tokenize"
            );
            cowel_free_dump_tokens_result_u8(&dump_options, &dump_result);
            return EXIT_FAILURE;
        }
        if (dump_result.output.text != nullptr) {
            std::fwrite(dump_result.output.text, 1, dump_result.output.length, out_file.get());
        }
    }
    else if (dump_result.output.text != nullptr) {
        std::fwrite(dump_result.output.text, 1, dump_result.output.length, stdout);
    }
    cowel_free_dump_tokens_result_u8(&dump_options, &dump_result);
    return EXIT_SUCCESS;
}

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
    case COWEL_CLI_COMMAND_HELP: {
        std::cout << help_text;
        return opts.command == COWEL_CLI_COMMAND_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    case COWEL_CLI_COMMAND_VERSION: {
        std::cout << version_text;
        return EXIT_SUCCESS;
    }
    // The remaining commands require too much shared setup work,
    // so we handled them outside the switch.
    case COWEL_CLI_COMMAND_RUN:
    case COWEL_CLI_COMMAND_TOKENIZE: break;
    }

    const auto in_path_u8 = as_u8string_view(opts.input);
    const auto out_path_u8 = as_u8string_view(opts.output);
    const bool colors_enabled = !opts.no_color;

    Global_Memory_Resource global_memory;
    std::pmr::unsynchronized_pool_resource memory { &global_memory };

    auto in_path_directory = std::filesystem::path { as_string_view(in_path_u8) }.parent_path();
    Relative_File_Loader file_loader { std::move(in_path_directory), &memory };

    // TODO: allow custom ulight themes instead of hardcoding wg21.json

    const auto alloc_options = Allocator_Options::from_memory_resource(&memory);
    const auto load_file_ref = file_loader.as_cowel_load_file_fn();

    const Result<std::pmr::vector<char8_t>, IO_Error_Code> in_text
        = load_utf8_file(in_path_u8, &memory);
    if (!in_text) {
        Stderr_Logger logger { file_loader, in_path_u8, u8"", &memory, colors_enabled };
        constexpr auto log_fn = [](Stderr_Logger* logger, const cowel_diagnostic_u8* diagnostic
                                ) noexcept -> void { (*logger)(*diagnostic); };
        const Function_Ref<void(const cowel_diagnostic_u8*) noexcept> log_ref
            = { const_v<log_fn>, &logger };
        const std::u8string message = u8"Failed to open input file.";
        log_cli_diagnostic(log_ref, COWEL_SEVERITY_FATAL, message, u8"cli", in_path_u8);
        return EXIT_FAILURE;
    }

    const auto in_source = as_u8string_view(*in_text);
    Stderr_Logger logger { file_loader, in_path_u8, in_source, &memory, colors_enabled };
    constexpr auto log_fn = [](Stderr_Logger* logger, const cowel_diagnostic_u8* diagnostic
                            ) noexcept -> void { (*logger)(*diagnostic); };
    const Function_Ref<void(const cowel_diagnostic_u8*) noexcept> log_ref
        = { const_v<log_fn>, &logger };

    switch (opts.command) {
    case COWEL_CLI_COMMAND_NONE:
    case COWEL_CLI_COMMAND_HELP:
    case COWEL_CLI_COMMAND_VERSION: {
        COWEL_ASSERT_UNREACHABLE(u8"Simple commands should have been handled above.");
    }
    case COWEL_CLI_COMMAND_RUN: {
        return run_run_command(
            in_source, in_path_u8, out_path_u8, //
            alloc_options, load_file_ref, logger, log_ref, opts.min_severity
        );
    }
    case COWEL_CLI_COMMAND_TOKENIZE: {
        return run_tokenize_command(
            in_source, out_path_u8, //
            log_ref, alloc_options, colors_enabled, opts.min_severity
        );
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid command obtained from CLI options parser.");
}

} // namespace
} // namespace cowel

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, const char* const* argv)
{
    return cowel::main(argc, argv);
}
