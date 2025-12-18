#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory_resource>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#define ARGS_NOEXCEPT
#include "args.hxx"

#include "cowel/memory_resources.hpp"
#include "cowel/util/annotated_string.hpp"
#include "cowel/util/ansi.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/meta.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/assets.hpp"
#include "cowel/cowel.h"
#include "cowel/fwd.hpp"
#include "cowel/print.hpp"
#include "cowel/relative_file_loader.hpp"
#include "cowel/services.hpp"

namespace cowel {
namespace {

using cowel::as_u8string_view;

[[nodiscard]]
cowel_string_view_u8 as_cowel_string_view(std::u8string_view str)
{
    return { str.data(), str.length() };
}

[[nodiscard]]
std::u8string_view as_u8string_view(cowel_string_view_u8 str)
{
    return { str.text, str.length };
}

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
std::u8string_view severity_tag(Severity severity)
{
    using enum Severity;
    switch (severity) {
    case min: return u8"MIN";
    case trace: return u8"TRACE";
    case debug: return u8"DEBUG";
    case info: return u8"INFO";
    case soft_warning: return u8"SOFTWARN";
    case warning: return u8"WARNING";
    case error: return u8"ERROR";
    case fatal: return u8"FATAL";
    case none: break;
    }
    return u8"???";
}

[[nodiscard]]
constexpr File_Source_Span as_file_source_span(const cowel_diagnostic_u8& diagnostic)
{
    return {
        {
            { .line = diagnostic.line, .column = diagnostic.column, .begin = diagnostic.begin },
            diagnostic.length,
        },
        File_Id(diagnostic.file_id),
    };
}

struct Stderr_Logger {
    const Relative_File_Loader& file_loader;
    const std::u8string_view main_file_name;
    const std::u8string_view main_file_source;
    Diagnostic_String out;
    bool any_errors = false;

    [[nodiscard]]
    constexpr Stderr_Logger(
        Relative_File_Loader& file_loader,
        std::u8string_view main_file_name,
        std::u8string_view main_file_source,
        std::pmr::memory_resource* memory
    )
        : file_loader { file_loader }
        , main_file_name { main_file_name }
        , main_file_source { main_file_source }
        , out { memory }
    {
    }

    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    void operator()(const cowel_diagnostic_u8& diagnostic)
    {
        COWEL_ASSERT(diagnostic.file_id >= -1);

        const auto severity = Severity(diagnostic.severity);
        any_errors |= severity >= Severity::error;

        const auto file_entry = [&] -> File_Entry {
            if (diagnostic.file_id < 0) {
                return {
                    .id = File_Id(diagnostic.file_id),
                    .source = main_file_source,
                    .name = main_file_name,
                };
            }
            const Owned_File_Entry& result = file_loader.at(File_Id(diagnostic.file_id));
            return {
                .id = File_Id(diagnostic.file_id),
                .source = as_u8string_view(result.text),
                .name = as_u8string_view(result.path_string),
            };
        }();

        const File_Source_Span location = as_file_source_span(diagnostic);

        out.append(severity_highlight(severity));
        out.append(severity_tag(severity));
        out.append(ansi::reset);
        out.append(u8": ");
        if (diagnostic.length == 0) {
            print_location_of_file(out, file_entry.name);
        }
        else {
            print_file_position(out, file_entry.name, location);
        }
        out.append(u8' ');
        out.append(as_u8string_view(diagnostic.message));
        out.append(ansi::h_black);
        out.append(u8" [");
        out.append(as_u8string_view(diagnostic.id));
        out.append(u8']');
        out.append(ansi::reset);
        out.append(u8'\n');
        if (diagnostic.length != 0) {
            print_affected_line(out, file_entry.source, location);
        }
        print_code_string_stderr(out);
        out.clear();
    }
};

int main(int argc, const char* const* const argv)
{
    static const std::unordered_map<std::string, Severity> severity_arg_map {
        { "min", Severity::min },
        { "trace", Severity::trace },
        { "debug", Severity::debug },
        { "info", Severity::info },
        { "soft_warning", Severity::soft_warning },
        { "warning", Severity::warning },
        { "error", Severity::error },
        { "fatal", Severity::fatal },
        { "none", Severity::none },
    };

    args::ArgumentParser parser { "Processes COWEL documents into HTML." };
    parser.helpParams.width = 100;
    parser.helpParams.addChoices = true;
    args::Positional<std::string> input_arg {
        parser,
        "input",
        "Input COWEL file",
        args::Options::Required,
    };
    args::Positional<std::string> output_arg {
        parser,
        "output",
        "Output HTML file",
        args::Options::Required,
    };
    args::MapFlag<std::string, Severity> severity_arg {
        parser,
        "severity",
        "Minimum (>=) severity for log messages",
        { 'l', "severity" },
        severity_arg_map,
        Severity::info,
    };
    args::HelpFlag help_arg {
        parser, "help", "Display this help menu", { 'h', "help" }, args::Options::Global
    };

    if (argc <= 1) {
        parser.Help(std::cout);
        return EXIT_FAILURE;
    }
    if (!parser.ParseCLI(argc, argv) || parser.GetError() != args::Error::None) {
        std::cerr << parser.GetErrorMsg() << '\n';
        return EXIT_FAILURE;
    }
    if (help_arg.Matched()) {
        parser.Help(std::cout);
        return EXIT_SUCCESS;
    }

    const std::string in_path = input_arg.Get();
    const std::string out_path = output_arg.Get();

    Global_Memory_Resource global_memory;
    std::pmr::unsynchronized_pool_resource memory { &global_memory };

    const std::u8string_view in_path_u8 = as_u8string_view(in_path);
    auto in_path_directory = std::filesystem::path { in_path }.parent_path();

    const std::u8string_view out_path_u8 = as_u8string_view(out_path);

    const Result<std::pmr::vector<char8_t>, IO_Error_Code> in_text
        = load_utf8_file(in_path_u8, &memory);
    if (!in_text) {
        Diagnostic_String error { &memory };
        print_io_error(error, in_path_u8, in_text.error());
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }
    const auto in_source = as_u8string_view(*in_text);

    Relative_File_Loader file_loader { std::move(in_path_directory), &memory };
    Stderr_Logger logger { file_loader, in_path_u8, in_source, &memory };

    // TODO: allow custom ulight themes instead of hardcoding wg21.json

    constexpr auto alloc_fn = [](std::pmr::memory_resource* memory, std::size_t size,
                                 std::size_t alignment) noexcept -> void* {
#ifdef ULIGHT_EXCEPTIONS
        try {
#endif
            return memory->allocate(size, alignment);
#ifdef ULIGHT_EXCEPTIONS
        } catch (...) {
            return nullptr;
        }
#endif
    };
    const Function_Ref<void*(std::size_t, std::size_t) noexcept> alloc_ref
        = { const_v<alloc_fn>, &memory };

    constexpr auto free_fn = [](std::pmr::memory_resource* memory, void* pointer, std::size_t size,
                                std::size_t alignment) noexcept -> void {
        memory->deallocate(pointer, size, alignment);
    };
    const Function_Ref<void(void*, std::size_t, std::size_t) noexcept> free_ref
        = { const_v<free_fn>, &memory };

    constexpr auto load_file_fn = [](Relative_File_Loader* file_loader, cowel_string_view_u8 path,
                                     cowel_file_id relative_to) noexcept -> cowel_file_result_u8 {
        return file_loader->do_load(as_u8string_view(path), File_Id(relative_to)).file_result;
    };
    const Function_Ref<cowel_file_result_u8(cowel_string_view_u8, cowel_file_id) noexcept>
        load_file_ref = { const_v<load_file_fn>, &file_loader };

    constexpr auto log_fn = [](Stderr_Logger* logger, const cowel_diagnostic_u8* diagnostic
                            ) noexcept -> void { (*logger)(*diagnostic); };
    const Function_Ref<void(const cowel_diagnostic_u8*) noexcept> log_ref
        = { const_v<log_fn>, &logger };

    const cowel_options_u8 options {
        .source = as_cowel_string_view(in_source),
        .highlight_theme_json = as_cowel_string_view(assets::wg21_json),
        .mode = COWEL_MODE_DOCUMENT,
        .min_log_severity = cowel_severity(severity_arg.Get()),
        .reserved_0 {},
        .alloc = alloc_ref.get_invoker(),
        .alloc_data = alloc_ref.get_entity(),
        .free = free_ref.get_invoker(),
        .free_data = free_ref.get_entity(),
        .load_file = load_file_ref.get_invoker(),
        .load_file_data = load_file_ref.get_entity(),
        .log = log_ref.get_invoker(),
        .log_data = log_ref.get_entity(),
        .reserved_1 {},
    };

    const cowel_gen_result_u8 result = cowel_generate_html_u8(&options);

    const auto out_file = fopen_unique(out_path.data(), "wb");
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
