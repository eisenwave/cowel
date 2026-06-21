#include <string>
#include <unordered_map>
#include <vector>

#define ARGS_NOEXCEPT
#include "args.hxx"

#include "cowel/cowel.h"

namespace cowel {
namespace {

[[nodiscard]]
cowel_mutable_string_view_u8 alloc_str(const std::string_view s) noexcept
{
    return cowel_alloc_text_u8({ reinterpret_cast<const char8_t*>(s.data()), s.size() });
}

} // namespace
} // namespace cowel

extern "C" {

cowel_parsed_cli_options_u8
cowel_parse_cli_options_u8(const char* const* const args, const std::size_t arg_count) noexcept
{
    if (arg_count == 0) {
        return {
            .command = COWEL_CLI_COMMAND_NONE,
            .input = {},
            .output = {},
            .min_severity = COWEL_SEVERITY_INFO,
            .no_color = false,
            .ok = true,
            .error_message = {},
        };
    }

    static const std::unordered_map<std::string, cowel_severity> severity_arg_map {
        { "min", COWEL_SEVERITY_MIN },
        { "trace", COWEL_SEVERITY_TRACE },
        { "debug", COWEL_SEVERITY_DEBUG },
        { "info", COWEL_SEVERITY_INFO },
        { "soft_warning", COWEL_SEVERITY_SOFT_WARNING },
        { "warning", COWEL_SEVERITY_WARNING },
        { "error", COWEL_SEVERITY_ERROR },
        { "fatal", COWEL_SEVERITY_FATAL },
        { "none", COWEL_SEVERITY_NONE },
    };

    args::ArgumentParser parser { "Processes COWEL documents into HTML." };
    parser.helpParams.width = 100;
    parser.helpParams.addChoices = true;

    args::HelpFlag help_arg {
        parser, "help", "Display this help menu", { 'h', "help" }, args::Options::Global
    };
    args::Flag version_arg {
        parser, "version", "Show version", { 'v', "version" }, args::Options::Global
    };

    args::Flag no_color_arg {
        parser, "no-color", "Disable colored output", { "no-color" }, args::Options::Global
    };

    std::string input_path;
    std::string output_path;
    cowel_severity severity = COWEL_SEVERITY_INFO;
    std::string subparser_error_msg;

    args::Command run_cmd {
        parser,
        "run",
        "Processes a COWEL document",
        [&](args::Subparser& sub) {
            args::Positional<std::string> input_arg { sub, "input", "Input COWEL file",
                                                      args::Options::Required };
            args::Positional<std::string> output_arg { sub, "output", "Output HTML file",
                                                       args::Options::Required };
            args::MapFlag<std::string, cowel_severity> severity_arg {
                sub,
                "severity",
                "Minimum (>=) severity for log messages",
                { 'l', "severity" },
                severity_arg_map,
                COWEL_SEVERITY_INFO,
            };
            sub.Parse();
            if (sub.GetError() != args::Error::None) {
                subparser_error_msg = sub.GetErrorMsg();
                return;
            }
            input_path = args::get(input_arg);
            output_path = args::get(output_arg);
            severity = args::get(severity_arg);
        },
    };

    args::Command tokenize_cmd {
        parser,
        "tokenize",
        "Dumps the tokens of a COWEL document",
        [&](args::Subparser& sub) {
            args::Positional<std::string> input_arg { sub, "input", "Input COWEL file",
                                                      args::Options::Required };
            args::Positional<std::string> output_arg {
                sub,
                "output",
                "Output token dump file",
                std::string {},
            };
            args::MapFlag<std::string, cowel_severity> severity_arg {
                sub,
                "severity",
                "Minimum (>=) severity for log messages",
                { 'l', "severity" },
                severity_arg_map,
                COWEL_SEVERITY_INFO,
            };
            sub.Parse();
            if (sub.GetError() != args::Error::None) {
                subparser_error_msg = sub.GetErrorMsg();
                return;
            }
            input_path = args::get(input_arg);
            output_path = args::get(output_arg);
            severity = args::get(severity_arg);
        },
    };

    // ParseCLI expects a range; build one from the raw pointers.
    std::vector<std::string> argv { args, args + arg_count };
    parser.ParseCLI(argv);

    // Check help/version before the error check:
    // in ARGS_NOEXCEPT mode, HelpFlag sets an error even on success.
    if (help_arg.Matched()) {
        return {
            .command = COWEL_CLI_COMMAND_HELP,
            .input = {},
            .output = {},
            .min_severity = COWEL_SEVERITY_INFO,
            .no_color = false,
            .ok = true,
            .error_message = {},
        };
    }
    if (version_arg.Matched()) {
        return {
            .command = COWEL_CLI_COMMAND_VERSION,
            .input = {},
            .output = {},
            .min_severity = COWEL_SEVERITY_INFO,
            .no_color = false,
            .ok = true,
            .error_message = {},
        };
    }
    if (parser.GetError() != args::Error::None) {
        const std::string msg
            = !subparser_error_msg.empty() ? subparser_error_msg : parser.GetErrorMsg();
        return {
            .command = COWEL_CLI_COMMAND_NONE,
            .input = {},
            .output = {},
            .min_severity = COWEL_SEVERITY_INFO,
            .no_color = false,
            .ok = false,
            .error_message = cowel::alloc_str(msg),
        };
    }

    if (run_cmd) {
        return {
            .command = COWEL_CLI_COMMAND_RUN,
            .input = cowel::alloc_str(input_path),
            .output = cowel::alloc_str(output_path),
            .min_severity = severity,
            .no_color = no_color_arg.Get(),
            .ok = true,
            .error_message = {},
        };
    }
    if (tokenize_cmd) {
        return {
            .command = COWEL_CLI_COMMAND_TOKENIZE,
            .input = cowel::alloc_str(input_path),
            .output = cowel::alloc_str(output_path),
            .min_severity = severity,
            .no_color = no_color_arg.Get(),
            .ok = true,
            .error_message = {},
        };
    }

    return {
        .command = COWEL_CLI_COMMAND_NONE,
        .input = {},
        .output = {},
        .min_severity = COWEL_SEVERITY_INFO,
        .no_color = false,
        .ok = true,
        .error_message = {},
    };
}

void cowel_free_cli_options_u8(cowel_parsed_cli_options_u8* const options) noexcept
{
    static constexpr auto free_str = [](cowel_mutable_string_view_u8& s) noexcept {
        if (s.text) {
            cowel_free(s.text, s.length, alignof(char8_t));
            s = {};
        }
    };
    if (!options) {
        return;
    }
    free_str(options->input);
    free_str(options->output);
    free_str(options->error_message);
}

} // extern "C"
