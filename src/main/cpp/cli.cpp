#include <cstdio>
#include <memory_resource>
#include <string_view>
#include <vector>

#include "cowel/util/annotated_string.hpp"
#include "cowel/util/ansi.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/document_content_behavior.hpp"
#include "cowel/document_generation.hpp"
#include "cowel/parse.hpp"
#include "cowel/print.hpp"
#include "cowel/ulight_highlighter.hpp"

namespace cowel {
namespace {

struct Relative_File_Loader final : File_Loader {
    std::filesystem::path base;

    explicit Relative_File_Loader(std::filesystem::path&& base)
        : base { std::move(base) }
    {
    }

    [[nodiscard]]
    bool operator()(std::pmr::vector<char8_t>& out, std::u8string_view u8path) final
    {
        const std::filesystem::path relative { u8path, std::filesystem::path::generic_format };
        const std::filesystem::path resolved = base / relative;
        return load_utf8_file(out, resolved.native()).has_value();
    }
};

[[nodiscard]]
std::u8string_view severity_highlight(Severity severity)
{
    using enum Severity;
    switch (severity) {
    case debug: return ansi::h_black;
    case soft_warning: return ansi::green;
    case warning: return ansi::h_yellow;
    case error: return ansi::h_red;
    default: return ansi::magenta;
    }
}

[[nodiscard]]
std::u8string_view severity_tag(Severity severity)
{
    using enum Severity;
    switch (severity) {
    case debug: return u8"DEBUG";
    case soft_warning: return u8"SOFTWARN";
    case warning: return u8"WARNING";
    case error: return u8"ERROR";
    default: return u8"???";
    }
}

struct Stderr_Logger final : Logger {
    Diagnostic_String out;
    std::string_view file;
    std::u8string_view source;
    bool any_errors = false;

    [[nodiscard]]
    constexpr Stderr_Logger(
        std::pmr::memory_resource* memory,
        std::string_view file,
        std::u8string_view source
    )
        : Logger { Severity::min }
        , out { memory }
        , file { file }
        , source { source }
    {
    }

    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    void operator()(const Diagnostic& diagnostic) final
    {
        any_errors |= diagnostic.severity >= Severity::error;

        out.append(severity_highlight(diagnostic.severity));
        out.append(severity_tag(diagnostic.severity));
        out.append(ansi::reset);
        out.append(u8": ");
        if (diagnostic.location.empty()) {
            print_location_of_file(out, file);
        }
        else {
            print_file_position(out, file, diagnostic.location);
        }
        out.append(u8' ');
        for (const std::u8string_view part : diagnostic.message) {
            out.append(part);
        }
        out.append(ansi::h_black);
        out.append(u8" [");
        out.append(diagnostic.id);
        out.append(u8']');
        out.append(ansi::reset);
        out.append(u8'\n');
        if (!diagnostic.location.empty()) {
            print_affected_line(out, source, diagnostic.location);
        }
        print_code_string_stderr(out);
        out.clear();
    }
};

int main(int argc, const char* const* argv)
{
    if (argc < 1) {
        return EXIT_FAILURE;
    }
    const std::u8string_view program_name { reinterpret_cast<const char8_t*>(argv[0]) };

    std::pmr::unsynchronized_pool_resource memory;

    if (argc < 3) {
        Basic_Annotated_String<char8_t, Diagnostic_Highlight> error { &memory };
        error.append(u8"Usage: ");
        error.append(program_name);
        error.append(u8" IN_FILE.cowel OUT_FILE.html\n");
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }

    const std::string_view in_path = argv[1];
    const std::u8string_view in_path_u8 { reinterpret_cast<const char8_t*>(in_path.data()),
                                          in_path.size() };
    auto in_path_directory = std::filesystem::path { in_path }.parent_path();

    const std::string_view out_path = argv[2];
    constexpr std::string_view theme_path = "ulight/themes/wg21.json";

    const Result<std::pmr::vector<char8_t>, IO_Error_Code> in_text
        = load_utf8_file(in_path, &memory);
    if (!in_text) {
        Diagnostic_String error { &memory };
        print_io_error(error, in_path, in_text.error());
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }

    const Result<std::pmr::vector<char8_t>, IO_Error_Code> theme_json
        = load_utf8_file(theme_path, &memory);
    if (!theme_json) {
        Diagnostic_String error { &memory };
        print_io_error(error, in_path, in_text.error());
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }

    std::pmr::vector<char8_t> out_text { &memory };
    const std::u8string_view in_source { in_text->data(), in_text->size() };
    const std::u8string_view theme_source { theme_json->data(), theme_json->size() };

    Builtin_Directive_Set builtin_directives {};
    Document_Content_Behavior behavior { builtin_directives.get_macro_behavior() };
    Relative_File_Loader file_loader { std::move(in_path_directory) };
    Stderr_Logger logger { &memory, in_path, in_source };
    static constinit Ulight_Syntax_Highlighter highlighter;

    const std::pmr::vector<ast::Content> root_content = parse_and_build(
        in_source, &memory,
        [&](std::u8string_view id, Source_Span pos, std::u8string_view message) {
            logger(Diagnostic {
                Severity::error, id, File_Source_Span8 { pos, in_path_u8 }, { &message, 1 } });
        }
    );

    const Generation_Options options { .output = out_text,
                                       .root_behavior = behavior,
                                       .root_content = root_content,
                                       .builtin_behavior = builtin_directives,
                                       .path = in_path,
                                       .source = in_source,
                                       .highlight_theme_source = theme_source,
                                       .file_loader = file_loader,
                                       .logger = logger,
                                       .highlighter = highlighter,
                                       .memory = &memory };
    generate_document(options);

    std::FILE* const out_file = std::fopen(out_path.data(), "wb");
    if (!out_file) {
        Diagnostic_String error { &memory };
        print_location_of_file(error, out_path);
        error.append(u8" Failed to open file.");
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }

    std::fwrite(out_text.data(), 1, out_text.size(), out_file);

    return logger.any_errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

} // namespace
} // namespace cowel

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, const char* const* argv)
{
    return cowel::main(argc, argv);
}
