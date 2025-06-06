#include <filesystem>
#include <initializer_list>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/diagnostic_highlight.hpp"
#include "cowel/document_content_behavior.hpp"
#include "cowel/util/annotated_string.hpp"
#include "cowel/util/assert.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_behavior.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_behavior.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/document_generation.hpp"
#include "cowel/fwd.hpp"
#include "cowel/parse.hpp"

#include "collecting_logger.hpp"
#include "diff.hpp"
#include "io.hpp"
#include "test_highlighter.hpp"

namespace cowel {
namespace {

using Suppress_Unused_Include_Annotated_String = Basic_Annotated_String<void, void>;

struct Trivial_Content_Behavior final : Content_Behavior {
private:
    Macro_Name_Resolver m_macro_resolver;

public:
    constexpr explicit Trivial_Content_Behavior(Directive_Behavior& macro_behavior)
        : m_macro_resolver { macro_behavior }
    {
    }

    void generate_plaintext(
        std::pmr::vector<char8_t>& out,
        std::span<const ast::Content> content,
        Context& context
    ) const final
    {
        context.add_resolver(m_macro_resolver);
        to_plaintext(out, content, context);
    }

    void generate_html(HTML_Writer& out, std::span<const ast::Content> content, Context& context)
        const final
    {
        context.add_resolver(m_macro_resolver);
        to_html(out, content, context);
    }
};

struct Paragraphs_Behavior final : Content_Behavior {

    void generate_plaintext(std::pmr::vector<char8_t>&, std::span<const ast::Content>, Context&)
        const final
    {
        COWEL_ASSERT_UNREACHABLE(u8"Unimplemented, not needed.");
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

enum struct Test_Behavior : Default_Underlying {
    trivial,
    paragraphs,
    empty_head,
};

struct Doc_Gen_Test : testing::Test {
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> out { &memory };

    Builtin_Directive_Set builtin_directives {};
    Trivial_Content_Behavior trivial_behavior { builtin_directives.get_macro_behavior() };
    Paragraphs_Behavior paragraphs_behavior {};
    Empty_Head_Behavior empty_head_behavior {};

    std::filesystem::path file_path;
    std::pmr::vector<char8_t> source { &memory };
    std::pmr::vector<char8_t> theme_source { &memory };
    std::u8string_view source_string {};
    std::u8string_view theme_source_string {};
    std::pmr::vector<ast::Content> content { &memory };

    Collecting_Logger logger { &memory };

    Doc_Gen_Test()
    {
        const bool theme_loaded = load_theme();
        COWEL_ASSERT(theme_loaded);
    }

    [[nodiscard]]
    Content_Behavior& get_behavior(Test_Behavior behavior)
    {
        switch (behavior) {
        case Test_Behavior::trivial: return trivial_behavior;
        case Test_Behavior::paragraphs: return paragraphs_behavior;
        case Test_Behavior::empty_head: return empty_head_behavior;
        default: COWEL_ASSERT_UNREACHABLE(u8"Invalid test behavior.");
        }
    }

    [[nodiscard]]
    bool load_document(const std::filesystem::path& path)
    {
        file_path = "test" / path;
        if (!load_utf8_file_or_error(source, file_path.generic_u8string(), &memory)) {
            return false;
        }
        source_string = { source.data(), source.size() };
        content = parse_and_build(
            source_string, path.generic_u8string(), &memory, make_parse_error_consumer()
        );
        return true;
    }

    [[nodiscard]]
    bool load_theme()
    {
        constexpr std::u8string_view theme_path = u8"ulight/themes/wg21.json";

        if (!load_utf8_file_or_error(theme_source, theme_path, &memory)) {
            return false;
        }
        theme_source_string = { source.data(), source.size() };
        return true;
    }

    void load_source(std::u8string_view source)
    {
        source_string = source;
        content = parse_and_build(source, u8"<no file>", &memory, make_parse_error_consumer());
    }

    std::u8string_view generate(Content_Behavior& root_behavior)
    {
        Directive_Behavior& error_behavior = builtin_directives.get_error_behavior();
        const Generation_Options options { .output = out,
                                           .root_behavior = root_behavior,
                                           .root_content = content,
                                           .builtin_behavior = builtin_directives,
                                           .error_behavior = &error_behavior,
                                           .highlight_theme_source = theme_source_string,
                                           .logger = logger,
                                           .highlighter = test_highlighter,
                                           .memory = &memory };
        generate_document(options);
        return { out.data(), out.size() };
    }

    Parse_Error_Consumer make_parse_error_consumer()
    {
        constexpr auto invoker = [](void* entity, std::u8string_view id,
                                    const File_Source_Span8& location, std::u8string_view message) {
            auto* const self = static_cast<Doc_Gen_Test*>(entity);
            Diagnostic d { Severity::error, id, location, { &message, 1 } };
            self->logger(d);
        };
        return Parse_Error_Consumer { invoker, this };
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
    ASSERT_TRUE(load_document("empty.cow"));
    const std::u8string_view actual = generate(trivial_behavior);
    EXPECT_EQ(expected, actual);
}

TEST_F(Doc_Gen_Test, text)
{
    constexpr std::u8string_view expected = u8"Hello, world!\n";
    ASSERT_TRUE(load_document("text.cow"));
    const std::u8string_view actual = generate(trivial_behavior);
    EXPECT_EQ(expected, actual);
}

TEST_F(Doc_Gen_Test, paragraphs)
{
    constexpr std::u8string_view expected = u8R"(<p>This is
a paragraph.
</p>
<p>This is another paragraph.
</p>)";
    ASSERT_TRUE(load_document("paragraphs.cow"));
    const std::u8string_view actual = generate(paragraphs_behavior);
    EXPECT_EQ(expected, actual);
}

struct Path {
    std::u8string_view value;
};

struct Source {
    std::u8string_view contents;
};

struct Basic_Test {
    std::variant<Path, Source> document;
    std::variant<Path, Source> expected_html;
    std::initializer_list<std::u8string_view> expected_diagnostics = {};
    Test_Behavior behavior = Test_Behavior::trivial;
};

// clang-format off
constexpr Basic_Test basic_tests[] {
    { Source {u8"\\c{#x41}\\c{#x42}\\c{#x43}\n"},
      Source{ u8"&#x41;&#x42;&#x43;\n" } },
    { Source{ u8"\\c{#x00B6}\n" },
      Source { u8"&#x00B6;\n" } },
    { Source{ u8"\\c{}\n" },
      Source { u8"<error->\\c{}</error->\n" },
      { diagnostic::c::blank } },
    { Source{ u8"\\c{ }\n" },
      Source { u8"<error->\\c{ }</error->\n" },
      { diagnostic::c::blank } },
    { Source{ u8"\\c{#zzz}\n" },
      Source { u8"<error->\\c{#zzz}</error->\n" },
    { diagnostic::c::digits } },
      { Source{ u8"\\c{#xD800}\n" },
      Source { u8"<error->\\c{#xD800}</error->\n" },
      { diagnostic::c::nonscalar } },
    
    { Path { u8"U/ascii.cow" },
      Source { u8"ABC\n" } },
    { Source { u8"\\U{00B6}\n" },
      Source { u8"¶\n" } },
    { Source { u8"\\U{}\n" },
      Source { u8"<error->\\U{}</error->\n" },
      { diagnostic::U::blank } },
    { Source { u8"\\U{ }\n" },
      Source { u8"<error->\\U{ }</error->\n" },
      { diagnostic::U::blank } },
    { Source { u8"\\U{zzz}\n" },
      Source { u8"<error->\\U{zzz}</error->\n" },
      { diagnostic::U::digits } },
    { Source { u8"\\U{D800}\n" },
      Source { u8"<error->\\U{D800}</error->\n" },
      { diagnostic::U::nonscalar } },

    { Source { u8"\\h1{Heading}\n" },
      Source { u8"<h1 id=heading><a class=para href=#heading></a>Heading</h1>\n" } },
    { Source { u8"\\h2[listed=no]{ }\n" },
      Source { u8"<h2> </h2>\n" } },
    { Source { u8"\\h3[id=user id,listed=no]{Heading}\n" },
      Source { u8"<h3 id=\"user id\"><a class=para href=\"#user%20id\"></a>Heading</h3>\n" } },
    { Source { u8"\\h4[id=user-id,listed=no]{Heading}\n" },
      Source { u8"<h4 id=user-id><a class=para href=#user-id></a>Heading</h4>\n" } },

    { Source { u8"\\html{<b>Bold</b>}\n" },
      Source { u8"<b>Bold</b>\n" } },

    { Source { u8"\\style{b { color: red; }}\n" },
      Source { u8"<style>b { color: red; }</style>\n" } },
    
    { Source { u8"\\script{let x = 3 < 5; let y = true && false;}\n" },
      Source { u8"<script>let x = 3 < 5; let y = true && false;</script>\n" } },

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
    { Path { u8"codeblock/trim.cow" },
      Path { u8"codeblock/trim.html" } },

    { Source { u8"\\hl[keyword]{awoo}\n" },
      Source { u8"<h- data-h=kw>awoo</h->\n" } },
    { Source { u8"\\code[c]{int \\hl[number]{x}}\n" },
      Source { u8"<code><h- data-h=kw_type>int</h-> <h- data-h=num>x</h-></code>\n" } },

    { Source { u8"\\math{\\mi[id=Z]{x}}\n" },
      Source { u8"<math display=inline><mi id=Z>x</mi></math>\n" } },

    { Path { u8"macro/macros.cow" },
      Path { u8"macro/macros.html" } },

    { Source { u8"" },
      Path { u8"document/empty.html" },
      {},
      Test_Behavior::empty_head }
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
        std::pmr::u8string full_path { u8"test/", memory };
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

        const std::u8string_view actual = generate(get_behavior(test.behavior));
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
            if (!logger.diagnostics.empty()) {
                Diagnostic_String error { &memory };
                error.append(
                    u8"Test failed because an unexpected diagnostic was emitted:\n",
                    Diagnostic_Highlight::error_text
                );
                error.append(logger.diagnostics.front().id);
                error.append(u8"\n\n");
                print_code_string_stdout(error);
            }
            EXPECT_TRUE(logger.diagnostics.empty());
            continue;
        }
        for (const std::u8string_view id : test.expected_diagnostics) {
            EXPECT_TRUE(logger.was_logged(id));
        }
    }
}

} // namespace
} // namespace cowel
