#include <filesystem>
#include <memory_resource>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "mmml/util/annotated_string.hpp"
#include "mmml/util/assert.hpp"
#include "mmml/util/function_ref.hpp"
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

struct Doc_Gen_Test : testing::Test {
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> out { &memory };
    Builtin_Directive_Set builtin_directives {};
    std::filesystem::path file_path;
    std::pmr::vector<char8_t> source { &memory };
    std::pmr::vector<ast::Content> content;

    Function_Ref<void(Diagnostic&&)> emit_diagnostic = {};
    Severity min_severity = Severity::none;
    std::pmr::vector<Diagnostic> caught_diagnostics { &memory };

    Content_Behavior* root_behavior = &trivial_behavior;
    Directive_Behavior* error_behavior = nullptr;

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

    void listen_for_diagnostics(Severity min_severity)
    {
        this->min_severity = min_severity;
        emit_diagnostic = [&](Diagnostic&& d) { caught_diagnostics.push_back(std::move(d)); };
    }

    std::u8string_view generate()
    {
        MMML_ASSERT(root_behavior);
        const Generation_Options options { .output = out,
                                           .root_behavior = *root_behavior,
                                           .root_content = content,
                                           .builtin_behavior = builtin_directives,
                                           .error_behavior = error_behavior,
                                           .path = file_path,
                                           .source = source_string(),
                                           .emit_diagnostic = emit_diagnostic,
                                           .min_severity = min_severity,
                                           .memory = &memory };
        generate_document(options);
        return { out.data(), out.size() };
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

} // namespace
} // namespace mmml
