#include <filesystem>
#include <memory_resource>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "mmml/util/annotated_string.hpp"
#include "mmml/util/assert.hpp"
#include "mmml/util/io.hpp"
#include "mmml/util/result.hpp"

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

struct Doc_Gen_Test : testing::Test {
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> out { &memory };
    Builtin_Directive_Set builtin_directives {};
    std::filesystem::path file_path;
    std::pmr::vector<char8_t> source { &memory };
    std::pmr::vector<ast::Content> content { &memory };

    Collecting_Logger logger { &memory };

    Content_Behavior* root_behavior = &trivial_behavior;

    [[nodiscard]]
    std::u8string_view source_string() const
    {
        return { source.data(), source.size() };
    }

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
        content = parse_and_build(source_string(), &memory);
        return true;
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
                                           .source = source_string(),
                                           .logger = logger,
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

struct Basic_Test {
    std::string_view path;
    std::u8string_view expected_html;
    std::initializer_list<std::u8string_view> expected_diagnostics;
};

// clang-format off
constexpr Basic_Test basic_tests[] {
    { "c/ascii.mmml", u8"&#x41;&#x42;&#x43;\n" , {} },
    { "c/pilcrow.mmml", u8"&#x00B6;\n", {} },
    { "c/empty.mmml", u8"<error->\\c{}</error->\n", { diagnostic::c_blank } },
    { "c/blank.mmml", u8"<error->\\c{ }</error->\n", { diagnostic::c_blank } },
    { "c/digits.mmml", u8"<error->\\c{#zzz}</error->\n", { diagnostic::c_digits } },
    { "c/nonscalar.mmml", u8"<error->\\c{#xD800}</error->\n", {diagnostic::c_nonscalar} },
    
    { "U/ascii.mmml", u8"ABC\n", {} },
    { "U/pilcrow.mmml", u8"¶\n", {} },
    { "U/empty.mmml", u8"<error->\\U{}</error->\n", { diagnostic::U_blank } },
    { "U/blank.mmml", u8"<error->\\U{ }</error->\n", { diagnostic::U_blank } },
    { "U/digits.mmml", u8"<error->\\U{zzz}</error->\n", { diagnostic::U_digits } },
    { "U/nonscalar.mmml", u8"<error->\\U{D800}</error->\n", {diagnostic::U_nonscalar} },
};
// clang-format on

TEST_F(Doc_Gen_Test, basic_directive_tests)
{
    for (const Basic_Test& test : basic_tests) {
        clear();
        const bool load_success = load_document(test.path);
        EXPECT_TRUE(load_success);
        if (!load_success) {
            continue;
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
