#include <filesystem>
#include <initializer_list>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "mmml/document_content_behavior.hpp"
#include "mmml/util/annotated_string.hpp"
#include "mmml/util/assert.hpp"

#include "mmml/builtin_directive_set.hpp"
#include "mmml/content_behavior.hpp"
#include "mmml/diagnostic.hpp"
#include "mmml/diagnostic_highlight.hpp"
#include "mmml/directive_behavior.hpp"
#include "mmml/directive_processing.hpp"
#include "mmml/document_generation.hpp"
#include "mmml/fwd.hpp"
#include "mmml/parse.hpp"
#include "mmml/print.hpp"

#include "collecting_logger.hpp"
#include "diff.hpp"
#include "io.hpp"
#include "test_highlighter.hpp"

namespace mmml {
namespace {

using Suppress_Unused_Include_Annotated_String = Basic_Annotated_String<void, void>;

struct Trivial_Content_Behavior final : Content_Behavior {

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

struct Empty_Head_Behavior final : Head_Body_Content_Behavior {

    void generate_head(HTML_Writer&, std::span<const ast::Content>, Context&) const final { }

    void generate_body(HTML_Writer& out, std::span<const ast::Content> content, Context& context)
        const final
    {
        to_html(out, content, context, To_HTML_Mode::paragraphs);
    }
};

constinit Trivial_Content_Behavior trivial_behavior {};
constinit Paragraphs_Behavior paragraphs_behavior {};
constinit Empty_Head_Behavior empty_head_behavior {};
[[maybe_unused]]
constinit Document_Content_Behavior document_content_behavior {};

struct Doc_Gen_Test : testing::Test {
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> out { &memory };
    Builtin_Directive_Set builtin_directives {};
    std::filesystem::path file_path;
    std::pmr::vector<char8_t> source { &memory };
    std::u8string_view source_string {};
    std::pmr::vector<ast::Content> content { &memory };

    Collecting_Logger logger { &memory };

    [[nodiscard]]
    bool load_document(const std::filesystem::path& path)
    {
        file_path = "test" / path;
        if (!load_utf8_file_or_error(source, file_path.c_str(), &memory)) {
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

    std::u8string_view generate(Content_Behavior& root_behavior)
    {
        Directive_Behavior& error_behavior = builtin_directives.get_error_behavior();
        const Generation_Options options { .output = out,
                                           .root_behavior = root_behavior,
                                           .root_content = content,
                                           .builtin_behavior = builtin_directives,
                                           .error_behavior = &error_behavior,
                                           .path = file_path,
                                           .source = source_string,
                                           .logger = logger,
                                           .highlighter = test_highlighter,
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
    const std::u8string_view actual = generate(trivial_behavior);
    EXPECT_EQ(expected, actual);
}

TEST_F(Doc_Gen_Test, text)
{
    constexpr std::u8string_view expected = u8"Hello, world!\n";
    ASSERT_TRUE(load_document("text.mmml"));
    const std::u8string_view actual = generate(trivial_behavior);
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
    const std::u8string_view actual = generate(paragraphs_behavior);
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
    std::variant<Path, Source> expected_html;
    std::initializer_list<std::u8string_view> expected_diagnostics = {};
    Content_Behavior& behavior = trivial_behavior;
};

// clang-format off
constexpr Basic_Test basic_tests[] {
    { Source {u8"\\c{#x41}\\c{#x42}\\c{#x43}\n"},
      Source{ u8"&#x41;&#x42;&#x43;\n" } },
    { Source{ u8"\\c{#x00B6}\n" },
      Source { u8"&#x00B6;\n" } },
    { Source{ u8"\\c{}\n" },
      Source { u8"<error->\\c{}</error->\n" },
      { diagnostic::c_blank } },
    { Source{ u8"\\c{ }\n" },
      Source { u8"<error->\\c{ }</error->\n" },
      { diagnostic::c_blank } },
    { Source{ u8"\\c{#zzz}\n" },
      Source { u8"<error->\\c{#zzz}</error->\n" },
    { diagnostic::c_digits } },
      { Source{ u8"\\c{#xD800}\n" },
      Source { u8"<error->\\c{#xD800}</error->\n" },
      { diagnostic::c_nonscalar } },
    
    { Path { "U/ascii.mmml" },
      Source { u8"ABC\n" } },
    { Source { u8"\\U{00B6}\n" },
      Source { u8"¶\n" } },
    { Source { u8"\\U{}\n" },
      Source { u8"<error->\\U{}</error->\n" },
      { diagnostic::U_blank } },
    { Source { u8"\\U{ }\n" },
      Source { u8"<error->\\U{ }</error->\n" },
      { diagnostic::U_blank } },
    { Source { u8"\\U{zzz}\n" },
      Source { u8"<error->\\U{zzz}</error->\n" },
      { diagnostic::U_digits } },
    { Source { u8"\\U{D800}\n" },
      Source { u8"<error->\\U{D800}</error->\n" },
      { diagnostic::U_nonscalar } },

    { Source { u8"\\h1{Heading}\n" },
      Source { u8"<h1 id=heading><a class=para href=#heading></a>Heading</h1>\n" } },
    { Source { u8"\\h2{ }\n" },
      Source { u8"<h2> </h2>\n" } },
    { Source { u8"\\h3[id=user id]{Heading}\n" },
      Source { u8"<h3 id=\"user id\"><a class=para href=\"#user id\"></a>Heading</h3>\n" } },
    { Source { u8"\\h4[id=user-id]{Heading}\n" },
      Source { u8"<h3 id=user-id><a class=para href=#user-id></a>Heading</h3>\n" } },

    { Source { u8"\\code{}\n" },
      Source { u8"<code></code>\n" },
      { diagnostic::highlight_language } },
    { Source { u8"\\code[x]{}\n" },
      Source { u8"<code></code>\n" } },
    { Source { u8"\\code[x]{ }\n" },
      Source { u8"<code> </code>\n" } },
    { Source { u8"\\code[x]{xxx}\n" },
      Source { u8"<code><h- data-h=kw>xxx</h-></code>\n" } },
    { Source { u8"\\code[x]{xxx123}\n" },
      Source { u8"<code><h- data-h=kw>xxx</h->123</code>\n" } },
    { Source { u8"\\code[x]{ 123 }\n" },
      Source { u8"<code> 123 </code>\n" } },
    { Source { u8"\\code[x]{ \\b{123} }\n" },
      Source { u8"<code> <b>123</b> </code>\n" } },
    { Source { u8"\\code[x]{ \\b{xxx} }\n" },
      Source { u8"<code> <b><h- data-h=kw>xxx</h-></b> </code>\n" } },
    { Source { u8"\\code[x]{ \\b{x}xx }\n" },
      Source { u8"<code> <b><h- data-h=kw>x</h-></b><h- data-h=kw>xx</h-> </code>\n" } },
    { Path { "codeblock/trim.mmml" },
      Path { "codeblock/trim.html" } },

    { Source { u8"" },
      Path { "document/empty.html" },
      {},
      empty_head_behavior }
};
// clang-format on

[[nodiscard]]
bool load_basic_test_input(Doc_Gen_Test& fixture, const Basic_Test& test)
{
    if (const auto* const path = std::get_if<Path>(&test.document)) {
        return fixture.load_document(path->value);
    }
    const auto& source = std::get<Source>(test.document);
    fixture.load_source(source.contents);
    return true;
}

[[nodiscard]]
std::u8string_view load_basic_test_expectations(
    std::pmr::vector<char8_t>& storage,
    const Basic_Test& test,
    std::pmr::memory_resource* memory
)
{
    if (const auto* const path = std::get_if<Path>(&test.expected_html)) {
        storage.clear();
        std::pmr::string full_path { "test/", memory };
        full_path += path->value;
        if (!load_utf8_file_or_error(storage, full_path, memory)) {
            return u8"";
        }
        return std::u8string_view { storage.data(), storage.size() };
    }
    return std::get<Source>(test.expected_html).contents;
}

TEST_F(Doc_Gen_Test, basic_directive_tests)
{
    for (const Basic_Test& test : basic_tests) {
        clear();

        const bool load_success = load_basic_test_input(*this, test);
        EXPECT_TRUE(load_success);
        if (!load_success) {
            continue;
        }

        std::pmr::vector<char8_t> expectation_storage { &memory };
        const std::u8string_view expected
            = load_basic_test_expectations(expectation_storage, test, &memory);
        EXPECT_FALSE(expected.empty());
        if (expected.empty()) {
            continue;
        }

        const std::u8string_view actual = generate(test.behavior);
        if (expected != actual) {
            Diagnostic_String error { &memory };
            error.append(
                u8"Test failed because generated HTML does not match expected HTML.\n",
                Diagnostic_Highlight::error_text
            );
            print_lines_diff(error, actual, expected);
            error.append(u8'\n');
            print_code_string_stdout(error);
        }
        EXPECT_TRUE(expected == actual);

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
