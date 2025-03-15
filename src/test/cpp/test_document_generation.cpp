#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <memory_resource>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "mmml/util/annotated_string.hpp"
#include "mmml/util/assert.hpp"
#include "mmml/util/hljs_scope.hpp"
#include "mmml/util/io.hpp"
#include "mmml/util/levenshtein_utf8.hpp"
#include "mmml/util/result.hpp"
#include "mmml/util/typo.hpp"

#include "mmml/diagnostic.hpp"
#include "mmml/directive_processing.hpp"
#include "mmml/directives.hpp"
#include "mmml/document_generation.hpp"
#include "mmml/fwd.hpp"
#include "mmml/parse.hpp"
#include "mmml/print.hpp"

namespace mmml {
namespace {

using Suppress_Unused_Include_Annotated_String = Basic_Annotated_String<void, void>;

struct Trivial_Content_Behavior final : Content_Behavior {
    constexpr Trivial_Content_Behavior()
        : Content_Behavior { Directive_Category::mixed, Directive_Display::block }
    {
    }

    void generate_plaintext(
        std::pmr::vector<char8_t>& out,
        std::span<const ast::Content> content,
        Context& context
    ) const final
    {
        to_plaintext(out, content, context);
    }

    void generate_html(HTML_Writer& out, std::span<const ast::Content> content, Context& context)
        const final
    {
        to_html(out, content, context);
    }
};

struct Paragraphs_Behavior final : Content_Behavior {
    constexpr Paragraphs_Behavior()
        : Content_Behavior { Directive_Category::mixed, Directive_Display::block }
    {
    }

    void generate_plaintext(std::pmr::vector<char8_t>&, std::span<const ast::Content>, Context&)
        const final
    {
        MMML_ASSERT_UNREACHABLE(u8"Unimplemented, not needed.");
    }

    void generate_html(HTML_Writer& out, std::span<const ast::Content> content, Context& context)
        const final
    {
        to_html(out, content, context, To_HTML_Mode::paragraphs);
    }
};

[[maybe_unused]]
constinit Trivial_Content_Behavior trivial_behavior {};
constinit Paragraphs_Behavior paragraphs_behavior {};

struct Collecting_Logger final : Logger {
    mutable std::pmr::vector<Diagnostic> diagnostics;

    [[nodiscard]]
    Collecting_Logger(std::pmr::memory_resource* memory)
        : Logger { Severity::min }
        , diagnostics { memory }
    {
    }

    void operator()(Diagnostic&& diagnostic) const final
    {
        diagnostics.push_back(std::move(diagnostic));
    }

    [[nodiscard]]
    bool nothing_logged() const
    {
        return diagnostics.empty();
    }

    [[nodiscard]]
    bool was_logged(std::u8string_view id) const
    {
        return std::ranges::find(diagnostics, id, &Diagnostic::id) != diagnostics.end();
    }
};

/// @brief Runs syntax highlighting for code of a test-only language
/// where sequences of the character `x` are considered keywords.
/// Nothing else is highlighted.
void syntax_highlight_x(std::pmr::vector<HLJS_Annotation_Span>& out, std::u8string_view code)
{
    char8_t prev = 0;
    std::size_t begin = 0;
    for (std::size_t i = 0; i < code.size(); ++i) {
        if (code[i] == u8'x' && prev != u8'x') {
            begin = i;
        }
        if (code[i] != u8'x' && prev == u8'x') {
            const HLJS_Annotation_Span span { .begin = begin,
                                              .length = i - begin,
                                              .value = HLJS_Scope::keyword };
            out.push_back(span);
        }
        prev = code[i];
    }
    if (prev == u8'x') {
        const HLJS_Annotation_Span span { .begin = begin,
                                          .length = code.size() - begin,
                                          .value = HLJS_Scope::keyword };
        out.push_back(span);
    }
}

struct X_Highlighter final : Syntax_Highlighter {
    static constexpr std::u8string_view language_name = u8"x";

    [[nodiscard]]
    std::span<const std::u8string_view> get_supported_languages() const final
    {
        static constexpr std::u8string_view supported[] { language_name };
        return supported;
    }

    [[nodiscard]]
    Distant<std::u8string_view>
    match_supported_language(std::u8string_view language, std::pmr::memory_resource* memory)
        const final
    {
        const std::size_t distance
            = code_point_levenshtein_distance(language_name, language, memory);
        return { language_name, distance };
    }

    [[nodiscard]]
    Result<void, Syntax_Highlight_Error> operator()(
        std::pmr::vector<HLJS_Annotation_Span>& out,
        std::u8string_view code,
        std::u8string_view language
    ) const final
    {
        if (language != language_name) {
            return Syntax_Highlight_Error::unsupported_language;
        }
        syntax_highlight_x(out, code);
        return {};
    }
};

constexpr X_Highlighter x_highlighter {};

struct Doc_Gen_Test : testing::Test {
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> out { &memory };
    Builtin_Directive_Set builtin_directives {};
    std::filesystem::path file_path;
    std::pmr::vector<char8_t> source { &memory };
    std::u8string_view source_string {};
    std::pmr::vector<ast::Content> content { &memory };

    Collecting_Logger logger { &memory };

    Content_Behavior* root_behavior = &trivial_behavior;

    [[nodiscard]]
    bool load_document(const std::filesystem::path& path)
    {
        file_path = "test" / path;
        Result<void, IO_Error_Code> result = load_utf8_file(source, file_path.c_str());

        if (!result) {
            Diagnostic_String error { &memory };
            print_io_error(error, file_path.c_str(), result.error());
            print_code_string_stdout(error);
            return false;
        }
        source_string = { source.data(), source.size() };
        content = parse_and_build(source_string, &memory);
        return true;
    }

    void load_source(std::u8string_view source)
    {
        source_string = source;
        content = parse_and_build(source, &memory);
    }

    std::u8string_view generate()
    {
        MMML_ASSERT(root_behavior);
        Directive_Behavior& error_behavior = builtin_directives.get_error_behavior();
        const Generation_Options options { .output = out,
                                           .root_behavior = *root_behavior,
                                           .root_content = content,
                                           .builtin_behavior = builtin_directives,
                                           .error_behavior = &error_behavior,
                                           .path = file_path,
                                           .source = source_string,
                                           .logger = logger,
                                           .highlighter = x_highlighter,
                                           .memory = &memory };
        generate_document(options);
        return { out.data(), out.size() };
    }

    void clear()
    {
        out.clear();
        source.clear();
        content.clear();
        logger.diagnostics.clear();
    }
};

TEST_F(Doc_Gen_Test, empty)
{
    constexpr std::u8string_view expected;
    ASSERT_TRUE(load_document("empty.mmml"));
    const std::u8string_view actual = generate();
    EXPECT_EQ(expected, actual);
}

TEST_F(Doc_Gen_Test, text)
{
    constexpr std::u8string_view expected = u8"Hello, world!\n";
    ASSERT_TRUE(load_document("text.mmml"));
    const std::u8string_view actual = generate();
    EXPECT_EQ(expected, actual);
}

TEST_F(Doc_Gen_Test, paragraphs)
{
    constexpr std::u8string_view expected = u8R"(<p>
This is
a paragraph.
</p>
<p>
This is another paragraph.
</p>
)";
    ASSERT_TRUE(load_document("paragraphs.mmml"));
    root_behavior = &paragraphs_behavior;
    const std::u8string_view actual = generate();
    EXPECT_EQ(expected, actual);
}

struct Path {
    std::string_view value;
};

struct Source {
    std::u8string_view contents;
};

struct Basic_Test {
    std::variant<Path, Source> document;
    std::u8string_view expected_html;
    std::initializer_list<std::u8string_view> expected_diagnostics;
};

// clang-format off
constexpr Basic_Test basic_tests[] {
    { Source {u8"\\c{#x41}\\c{#x42}\\c{#x43}\n"}, u8"&#x41;&#x42;&#x43;\n" , {} },
    { Source{ u8"\\c{#x00B6}\n" }, u8"&#x00B6;\n", {} },
    { Source{ u8"\\c{}\n" }, u8"<error->\\c{}</error->\n", { diagnostic::c_blank } },
    { Source{ u8"\\c{ }\n" }, u8"<error->\\c{ }</error->\n", { diagnostic::c_blank } },
    { Source{ u8"\\c{#zzz}\n" }, u8"<error->\\c{#zzz}</error->\n", { diagnostic::c_digits } },
    { Source{ u8"\\c{#xD800}\n" }, u8"<error->\\c{#xD800}</error->\n", {diagnostic::c_nonscalar} },
    
    { Path { "U/ascii.mmml" }, u8"ABC\n", {} },
    { Source { u8"\\U{00B6}\n" }, u8"¶\n", {} },
    { Source { u8"\\U{}\n" }, u8"<error->\\U{}</error->\n", { diagnostic::U_blank } },
    { Source { u8"\\U{ }\n" }, u8"<error->\\U{ }</error->\n", { diagnostic::U_blank } },
    { Source { u8"\\U{zzz}\n" }, u8"<error->\\U{zzz}</error->\n", { diagnostic::U_digits } },
    { Source { u8"\\U{D800}\n" }, u8"<error->\\U{D800}</error->\n", {diagnostic::U_nonscalar} },
};
// clang-format on

TEST_F(Doc_Gen_Test, basic_directive_tests)
{
    for (const Basic_Test& test : basic_tests) {
        clear();
        if (const auto* const path = std::get_if<Path>(&test.document)) {
            const bool load_success = load_document(path->value);
            EXPECT_TRUE(load_success);
            if (!load_success) {
                continue;
            }
        }
        else {
            const auto& source = std::get<Source>(test.document);
            load_source(source.contents);
        }

        const std::u8string_view actual = generate();
        EXPECT_EQ(test.expected_html, actual);
        if (test.expected_html != actual) {
            __builtin_debugtrap();
        }

        if (test.expected_diagnostics.size() == 0) {
            EXPECT_TRUE(logger.diagnostics.empty());
            continue;
        }
        for (const std::u8string_view id : test.expected_diagnostics) {
            EXPECT_TRUE(logger.was_logged(id));
        }
    }
}

} // namespace
} // namespace mmml
