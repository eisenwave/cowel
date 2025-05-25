#include <cstdio>
#include <filesystem>
#include <map>
#include <memory_resource>
#include <string_view>
#include <vector>

#include "cowel/util/annotated_string.hpp"
#include "cowel/util/ansi.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/assets.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/document_content_behavior.hpp"
#include "cowel/document_generation.hpp"
#include "cowel/parse.hpp"
#include "cowel/print.hpp"
#include "cowel/ulight_highlighter.hpp"

namespace cowel {
namespace {

struct File_Loader_Less {
    using is_transparent = void;

    [[nodiscard]]
    bool operator()(const auto& x, const auto& y) const noexcept
    {
        return std::u8string_view { x.data(), x.size() }
        < std::u8string_view { y.data(), y.size() };
    }
};

struct Relative_File_Loader final : File_Loader {
    using map_type
        = std::pmr::map<std::pmr::vector<char8_t>, std::pmr::vector<char8_t>, File_Loader_Less>;

    std::filesystem::path base;
    map_type entries;

    explicit Relative_File_Loader(std::filesystem::path&& base, std::pmr::memory_resource* memory)
        : base { std::move(base) }
        , entries { memory }
    {
    }

    [[nodiscard]]
    std::optional<File_Entry> load(std::u8string_view path) final
    {
        const std::filesystem::path relative { path, std::filesystem::path::generic_format };
        const std::filesystem::path resolved = base / relative;

        if (std::optional<File_Entry> _ = find(path)) {
            return {};
        }

        std::pmr::memory_resource* const memory = entries.get_allocator().resource();
        Result<std::pmr::vector<char8_t>, IO_Error_Code> result
            = load_utf8_file(resolved.generic_u8string(), memory);
        if (!result) {
            return {};
        }
        std::pmr::vector<char8_t> file { path.begin(), path.end(), memory };
        const auto [it, success] = entries.emplace(std::move(file), std::move(*result));
        COWEL_ASSERT(success);

        return to_file_entry(it);
    }

    [[nodiscard]]
    std::optional<File_Entry> find(std::u8string_view path) const final
    {
        const auto it = entries.find(path);
        if (it == entries.end()) {
            return {};
        }
        return to_file_entry(it);
    }

    static File_Entry to_file_entry(map_type::const_iterator it)
    {
        return File_Entry { .source = as_u8string_view(it->second),
                            .name = as_u8string_view(it->first) };
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
    File_Loader& file_loader;
    Diagnostic_String out;
    bool any_errors = false;

    [[nodiscard]]
    constexpr Stderr_Logger(File_Loader& file_loader, std::pmr::memory_resource* memory)
        : Logger { Severity::min }
        , file_loader { file_loader }
        , out { memory }
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
            print_location_of_file(out, diagnostic.location.file_name);
        }
        else {
            print_file_position(out, diagnostic.location.file_name, diagnostic.location);
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
            const std::optional<File_Entry> entry = file_loader.find(diagnostic.location.file_name);
            if (entry) {
                print_affected_line(out, entry->source, diagnostic.location);
            }
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
    const std::u8string_view in_path_u8 = as_u8string_view(in_path);
    auto in_path_directory = std::filesystem::path { in_path }.parent_path();

    const std::string_view out_path = argv[2];
    const std::u8string_view out_path_u8 = as_u8string_view(out_path);

    const Result<std::pmr::vector<char8_t>, IO_Error_Code> in_text
        = load_utf8_file(in_path_u8, &memory);
    if (!in_text) {
        Diagnostic_String error { &memory };
        print_io_error(error, in_path_u8, in_text.error());
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }

    /* TODO: eventually reimplement for user-specified themes.
    const Result<std::pmr::vector<char8_t>, IO_Error_Code> theme_json
        = load_utf8_file(theme_path, &memory);
    if (!theme_json) {
        Diagnostic_String error { &memory };
        print_io_error(error, in_path_u8, in_text.error());
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }
    const std::u8string_view theme_source { theme_json->data(), theme_json->size() };
    */

    std::pmr::vector<char8_t> out_text { &memory };
    const std::u8string_view in_source { in_text->data(), in_text->size() };
    const std::u8string_view theme_source = assets::wg21_json;

    Builtin_Directive_Set builtin_directives {};
    Document_Content_Behavior behavior { builtin_directives.get_macro_behavior() };
    Relative_File_Loader file_loader { std::move(in_path_directory), &memory };
    Stderr_Logger logger { file_loader, &memory };
    static constinit Ulight_Syntax_Highlighter highlighter;

    const std::pmr::vector<ast::Content> root_content = parse_and_build(
        in_source, in_path_u8, &memory,
        [&](std::u8string_view id, File_Source_Span8 pos, std::u8string_view message) {
            logger(Diagnostic { Severity::error, id, pos, { &message, 1 } });
        }
    );

    const Generation_Options options { .output = out_text,
                                       .root_behavior = behavior,
                                       .root_content = root_content,
                                       .builtin_behavior = builtin_directives,
                                       .highlight_theme_source = theme_source,
                                       .file_loader = file_loader,
                                       .logger = logger,
                                       .highlighter = highlighter,
                                       .memory = &memory };
    generate_document(options);

    const auto out_file = fopen_unique(out_path.data(), "wb");
    if (!out_file) {
        Diagnostic_String error { &memory };
        print_location_of_file(error, out_path_u8);
        error.append(u8" Failed to open file.");
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }

    std::fwrite(out_text.data(), 1, out_text.size(), out_file.get());

    return logger.any_errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

} // namespace
} // namespace cowel

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, const char* const* argv)
{
    return cowel::main(argc, argv);
}
