#include <filesystem>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "mmml/util/annotated_string.hpp"
#include "mmml/util/assert.hpp"
#include "mmml/util/result.hpp"

#include "mmml/ast.hpp"
#include "mmml/diagnostic_highlight.hpp"
#include "mmml/directive_processing.hpp"
#include "mmml/directives.hpp"
#include "mmml/document_generation.hpp"
#include "mmml/fwd.hpp"
#include "mmml/print.hpp"

#include "collecting_logger.hpp"
#include "diff.hpp"
#include "io.hpp"
#include "test_highlighter.hpp"

namespace mmml {
namespace {

struct Highlight_Content_Behavior final : Content_Behavior {
private:
    std::u8string_view m_language;

public:
    constexpr explicit Highlight_Content_Behavior(std::u8string_view language)
        : Content_Behavior { Directive_Category::mixed, Directive_Display::block }
        , m_language { language }
    {
    }

    void generate_plaintext(std::pmr::vector<char8_t>&, std::span<const ast::Content>, Context&)
        const final
    {
        MMML_ASSERT_UNREACHABLE(u8"wtf");
    }

    void generate_html(HTML_Writer& out, std::span<const ast::Content> content, Context& context)
        const final
    {
        const Result<void, Syntax_Highlight_Error> result
            = to_html_syntax_highlighted(out, content, m_language, context, To_HTML_Mode::direct);
        // If syntax highlighting failed during testing, something is seriously messed up ...
        MMML_ASSERT(result);
    }
};

struct Highlight_Test : testing::Test {
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> out { &memory };
    std::pmr::vector<char8_t> expectations { &memory };
    Builtin_Directive_Set builtin_directives {};
    std::filesystem::path file_path;
    std::pmr::vector<char8_t> source { &memory };
    std::u8string_view source_string {};
    std::pmr::vector<ast::Content> content { &memory };

    Collecting_Logger logger { &memory };

    Content_Behavior* root_behavior = nullptr;

    [[nodiscard]]
    bool load_document(const std::filesystem::path& path)
    {
        file_path = path;
        if (!load_utf8_file_or_error(source, file_path.c_str(), &memory)) {
            return false;
        }
        source_string = { source.data(), source.size() };
        // This essentially cheats and bypasses the parser.
        // The entire file contents are treated as a text literal;
        // even escape sequences like \{ are not processed.
        const Source_Span whole_file_span { { 0, 0, 0 }, source.size() };
        content = { ast::Text { whole_file_span } };
        return true;
    }

    [[nodiscard]]
    std::u8string_view load_expectations(const std::filesystem::path& path)
    {
        if (!load_utf8_file_or_error(expectations, path.c_str(), &memory)) {
            return u8"";
        }
        return std::u8string_view { expectations.data(), expectations.size() };
    }

    [[nodiscard]]
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
                                           .highlighter = test_highlighter,
                                           .memory = &memory };
        generate_document(options);
        return { out.data(), out.size() };
    }

    void clear()
    {
        out.clear();
        expectations.clear();
        source.clear();
        content.clear();
        logger.diagnostics.clear();
    }
};

TEST_F(Highlight_Test, basic_directive_tests)
{
    static const std::filesystem::path directory { "test/highlight" };
    ASSERT_TRUE(std::filesystem::is_directory(directory));

    // TODO: replace with std::from_range once supported
    std::filesystem::directory_iterator iterator(directory);
    std::pmr::vector<std::filesystem::path> paths { &memory };
    const auto paths_view = iterator
        | std::views::transform([](const std::filesystem::directory_entry& entry) -> auto& {
                                return entry.path();
                            });
    std::ranges::copy(paths_view, std::back_inserter(paths));
    std::ranges::sort(paths);

    for (const auto& path : paths) {
        const std::u8string extension = path.extension().generic_u8string();
        ASSERT_TRUE(extension.size() > 1);

        std::filesystem::path expectations = path;
        expectations += ".html";
        if (!std::filesystem::is_regular_file(expectations)) {
            if (extension != u8".html") {
                Diagnostic_String success { &memory };
                success.append(u8"NO EXPECTATIONS: ", Diagnostic_Highlight::warning);
                success.append(path.generic_u8string(), Diagnostic_Highlight::code_citation);
                success.append(u8'\n');
                print_code_string_stdout(success);
            }
            continue;
        }

        clear();

        const bool load_success = load_document(path);
        EXPECT_TRUE(load_success);
        if (!load_success) {
            continue;
        }

        const std::u8string_view expected = load_expectations(expectations);
        EXPECT_FALSE(expected.empty());
        if (expected.empty()) {
            continue;
        }

        Highlight_Content_Behavior behavior { extension.substr(1) };
        root_behavior = &behavior;

        const std::u8string_view actual = generate();
        const bool test_succeeded = expected == actual;
        if (!test_succeeded) {
            Diagnostic_String error { &memory };
            error.append(u8"FAIL", Diagnostic_Highlight::error_text);
            error.append(u8' ');
            error.build(Diagnostic_Highlight::text)
                .append(path.generic_u8string())
                .append(u8":\nActual (")
                .append(path.generic_u8string())
                .append(u8") -> expected (")
                .append(expectations.generic_u8string())
                .append(u8") difference:\n");
            print_lines_diff(error, actual, expected);
            error.append(u8'\n');
            print_code_string_stdout(error);
        }
        EXPECT_TRUE(test_succeeded);
        EXPECT_TRUE(logger.diagnostics.empty());

        Diagnostic_String success { &memory };
        success.append(u8"OK: ", Diagnostic_Highlight::success);
        success.append(path.generic_u8string(), Diagnostic_Highlight::code_citation);
        success.append(u8'\n');
        print_code_string_stdout(success);
    }
}

} // namespace
} // namespace mmml
