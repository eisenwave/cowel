#include <filesystem>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/parameters.hpp"
#include "cowel/util/annotated_string.hpp"
#include "cowel/util/meta.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/capture.hpp"
#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/html.hpp"
#include "cowel/policy/ignorant.hpp"
#include "cowel/policy/literally.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/diagnostic_highlight.hpp"
#include "cowel/directive_behavior.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/document_generation.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"
#include "cowel/parse.hpp"
#include "cowel/print.hpp"
#include "cowel/relative_file_loader.hpp"

#include "collecting_logger.hpp"
#include "diff.hpp"
#include "io.hpp"
#include "test_highlighter.hpp"

namespace cowel {
namespace {

constexpr std::u8string_view theme_path = u8"ulight/themes/wg21.json";

using Suppress_Unused_Include_Annotated_String = Basic_Annotated_String<void, void>;

[[nodiscard]]
Processing_Status write_empty_head_document(
    Text_Sink& out,
    std::span<const ast::Markup_Element> content,
    Context& context
)
{
    constexpr auto write_head = [](Content_Policy&, std::span<const ast::Markup_Element>,
                                   Context&) { return Processing_Status::ok; };
    constexpr auto write_body
        = [](Content_Policy& policy, std::span<const ast::Markup_Element> content,
             Context& context) { return splice_all(policy, content, Frame_Index::root, context); };

    return write_head_body_document(
        out, content, context, const_v<write_head>, const_v<write_body>
    );
}

struct Capture_Behavior final : Block_Directive_Behavior {
private:
    std::pmr::vector<char8_t>& m_test_input;
    const bool m_literally;

public:
    [[nodiscard]]
    explicit Capture_Behavior(std::pmr::vector<char8_t>& test_input, bool literally)
        : m_test_input { test_input }
        , m_literally { literally }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy&, const Invocation& call, Context& context) const override
    {
        const auto match_status = match_empty_arguments(call, context);
        if (match_status != Processing_Status::ok) {
            return match_status;
        }
        Capturing_Ref_Text_Sink sink { m_test_input, Output_Language::html };
        HTML_Content_Policy html_policy { sink };
        To_Source_Content_Policy literally_policy { sink };
        auto& policy = m_literally ? static_cast<Content_Policy&>(literally_policy) : html_policy;
        return splice_all(policy, call.get_content_span(), call.content_frame, context);
    }
};

struct Expecting_Logger final : Logger {
private:
    const Severity m_expected_severity;
    const std::u8string_view m_expected_id;
    bool m_logged = false;
    std::pmr::vector<Collected_Diagnostic> m_violations;

public:
    explicit Expecting_Logger(
        Severity min_severity,
        Severity expected_severity,
        std::u8string_view expected_id,
        std::pmr::memory_resource* memory
    )
        : Logger { min_severity }
        , m_expected_severity { expected_severity }
        , m_expected_id { expected_id }
        , m_violations { memory }
    {
    }

    [[nodiscard]]
    std::span<const Collected_Diagnostic> get_violations() const
    {
        return m_violations;
    }

    void operator()(Diagnostic diagnostic) override
    {
        std::pmr::vector<char8_t> id_data;
        append(id_data, diagnostic.id);
        const auto id_string = as_u8string_view(id_data);
        if (diagnostic.severity == m_expected_severity && id_string == m_expected_id) {
            m_logged = true;
            return;
        }
        // Additional warnings or errors are not considered a violation,
        // but getting anything with greater severity should not happen.
        if (diagnostic.severity > m_expected_severity) {
            static_assert(std::is_trivially_copyable_v<Diagnostic>);
            std::pmr::memory_resource* const memory = m_violations.get_allocator().resource();
            m_violations.emplace_back(diagnostic, memory);
        }
    }

    [[nodiscard]]
    bool was_expected_logged() const
    {
        return m_logged;
    }
};

struct Test_Expect_Behavior final : Block_Directive_Behavior {
private:
    const Processing_Status m_expected_status;
    const Severity m_expected_severity;

public:
    [[nodiscard]]
    explicit Test_Expect_Behavior(Processing_Status status, Severity severity)
        : m_expected_status { status }
        , m_expected_severity { severity }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const override
    {
        String_Matcher id_matcher { context.get_transient_memory() };
        Group_Member_Matcher id_member { u8"id"sv, Optionality::mandatory, id_matcher };
        Group_Member_Matcher* parameters[] { &id_member };
        Pack_Usual_Matcher args_matcher { parameters };
        Group_Pack_Matcher group_matcher { args_matcher };
        Call_Matcher call_matcher { group_matcher };

        if (const auto match_status = call_matcher.match_call(
                call, context, make_fail_callback(), Processing_Status::fatal
            );
            match_status != Processing_Status::ok) {
            return match_status;
        }
        const std::u8string_view expected_id = id_matcher.get();

        Logger* const old_logger = &context.get_logger();
        COWEL_ASSERT(old_logger);
        COWEL_ASSERT(old_logger->can_log(Severity::error));

        Processing_Status result = Processing_Status::ok;

        Expecting_Logger expecting_logger {
            old_logger->get_min_severity(),
            m_expected_severity,
            expected_id,
            context.get_transient_memory(),
        };
        context.set_logger(expecting_logger);
        const Processing_Status status
            = splice_all(out, call.get_content_span(), call.content_frame, context);

        if (status != m_expected_status) {
            (*old_logger)(Diagnostic {
                .severity = Severity::error,
                .id = u8"test.diagnostic"sv,
                .location = call.directive.get_source_span(),
                .message = joined_char_sequence(
                    {
                        u8"Expected the block to evaluate to status \""sv,
                        status_name(m_expected_status),
                        u8"\", but got \""sv,
                        status_name(status),
                        u8"\"."sv,
                    }
                ),
            });
            result = Processing_Status::error;
        }
        for (const Collected_Diagnostic& violation : expecting_logger.get_violations()) {
            (*old_logger)(Diagnostic {
                std::max(Severity::error, violation.severity),
                as_u8string_view(violation.id),
                violation.location,
                as_u8string_view(violation.message),
            });
            result = Processing_Status::error;
        }
        if (!expecting_logger.was_expected_logged()) {
            (*old_logger)(Diagnostic {
                .severity = Severity::error,
                .id = u8"test.diagnostic"sv,
                .location = call.directive.get_source_span(),
                .message = joined_char_sequence(
                    {
                        u8"Expected the block to produce the diagnostic \""sv,
                        expected_id,
                        u8"\", but it was not logged (with the expected severity)."sv,
                    }
                ),
            });
            result = Processing_Status::error;
        }

        context.set_logger(*old_logger);
        return result;
    }
};

/// @brief The set of directives available during the test.
/// This includes all builtin directives, as well as a few extra test-only directives
/// such as `\test_input` and `\test_output`.
struct Test_Directives final : Name_Resolver {
    Builtin_Directive_Set builtin {};
    Capture_Behavior test_input;
    Capture_Behavior test_output;
    Test_Expect_Behavior test_expect_warning { Processing_Status::ok, Severity::warning };
    Test_Expect_Behavior test_expect_error { Processing_Status::error, Severity::error };
    Test_Expect_Behavior test_expect_fatal { Processing_Status::fatal, Severity::fatal };

    [[nodiscard]]
    explicit Test_Directives(
        std::pmr::vector<char8_t>& test_input,
        std::pmr::vector<char8_t>& test_output
    )
        : test_input { test_input, false }
        , test_output { test_output, true }
    {
    }

    Test_Directives(const Test_Directives&) = delete;
    Test_Directives& operator=(const Test_Directives&) = delete;
    ~Test_Directives() = default;

    [[nodiscard]]
    Distant<std::u8string_view>
    fuzzy_lookup_name(std::u8string_view name, Context& context) const override
    {
        return builtin.fuzzy_lookup_name(name, context);
    }

    [[nodiscard]]
    const Directive_Behavior* operator()(std::u8string_view name) const override
    {
        if (const auto* const behavior = builtin(name)) {
            return behavior;
        }
        if (name == u8"test_input"sv) {
            return &test_input;
        }
        if (name == u8"test_output"sv) {
            return &test_output;
        }
        if (name == u8"test_expect_warning"sv) {
            return &test_expect_warning;
        }
        if (name == u8"test_expect_error"sv) {
            return &test_expect_error;
        }
        if (name == u8"test_expect_fatal"sv) {
            return &test_expect_fatal;
        }
        return nullptr;
    }
};

[[maybe_unused]]
void append_specials_escaped(
    Diagnostic_String& out,
    std::u8string_view str,
    Diagnostic_Highlight default_highlight
)
{
    for (const char8_t c : str) {
        switch (c) {
        case u8'\n': out.append(u8"\\n", Diagnostic_Highlight::escape); break;
        case u8'\t': out.append(u8"\\t", Diagnostic_Highlight::escape); break;
        default: out.append(c, default_highlight);
        }
    }
}

void append_test_details(Diagnostic_String& out, const std::u8string_view test_path)
{
    out.append(test_path, Diagnostic_Highlight::code_position);
    out.append(u8':', Diagnostic_Highlight::punctuation);
    out.append(u8' ');
}

struct Parse_Error_Logger {
    Logger& logger;

    void
    operator()(std::u8string_view id, const Source_Span& location, Char_Sequence8 message) const
    {
        const File_Source_Span file_location { location, File_Id::main };
        const Diagnostic d { Severity::error, id, file_location, message };
        logger(d);
    }
};

void print_diagnostic(
    Diagnostic_String& out,
    const Collected_Diagnostic& d,
    std::u8string_view file
)
{
    out.append(
        severity_tag(d.severity),
        d.severity >= Severity::error ? Diagnostic_Highlight::error : Diagnostic_Highlight::tag
    );
    out.append(' ');
    print_file_position(out, file, d.location);
    out.append(u8' ');
    out.append(d.message, Diagnostic_Highlight::text);
    out.append(u8' ');
    out.build(Diagnostic_Highlight::code_position).append(u8'[').append(d.id).append(u8']');
    out.append(u8'\n');
}

TEST(Document_Generation, file_tests)
{
    Global_Memory_Resource memory;

    std::pmr::vector<fs::path> test_paths { &memory };
    find_files_recursively(test_paths, "test/semantics");
    std::erase_if(test_paths, [](const fs::path& p) {
        return !p.generic_u8string().ends_with(u8".cow");
    });

    std::pmr::vector<char8_t> source { &memory };
    std::pmr::vector<char8_t> actual_html { &memory };
    std::pmr::vector<char8_t> expected_html { &memory };
    ast::Pmr_Vector<ast::Markup_Element> content { &memory };
    Collecting_Logger logger { &memory };
    Test_Directives directives { actual_html, expected_html };

    std::pmr::vector<char8_t> theme_source { &memory };
    ASSERT_TRUE(load_utf8_file_or_error(theme_source, theme_path, &memory));

    const auto clear = [&] -> void {
        actual_html.clear();
        expected_html.clear();
        source.clear();
        content.clear();
        logger.diagnostics.clear();
    };

    bool success = true;
    for (const auto& test_path : test_paths) {
        const std::u8string test_path_string = test_path.generic_u8string();
        clear();

        ASSERT_TRUE(load_utf8_file_or_error(source, test_path_string, &memory));

        const bool parse_success = lex_and_parse_and_build(
            content, as_u8string_view(source), File_Id::main, &memory, Parse_Error_Logger { logger }
        );
        if (!parse_success) {
            success = false;
            Diagnostic_String error { &memory };
            append_test_details(error, test_path_string);
            error.append(
                u8"Test failed because of parse errors:"sv, Diagnostic_Highlight::error_text
            );
            error.append(u8'\n');
            for (const Collected_Diagnostic& d : logger.diagnostics) {
                print_diagnostic(error, d, test_path_string);
            }
            print_flush_code_string_stdout(error);
            continue;
        }
        ASSERT_FALSE(content.empty());

        Relative_File_Loader file_loader { test_path.parent_path(), &memory };
        const Directive_Behavior& error_behavior = directives.builtin.get_error_behavior();
        const Generation_Options options {
            .error_behavior = &error_behavior,
            .highlight_theme_source = as_u8string_view(theme_source),
            .builtin_name_resolver = directives,
            .file_loader = file_loader,
            .logger = logger,
            .highlighter = test_highlighter,
            .memory = &memory,
        };

        const Processing_Status status = run_generation(
            [&](Context& context) -> Processing_Status {
                Ignorant_Content_Policy ignorant_policy;
                HTML_Content_Policy policy { ignorant_policy };
                return splice_all(policy, content, Frame_Index::root, context);
            },
            options
        );
        if (status != Processing_Status::ok) {
            success = false;
            Diagnostic_String error { &memory };
            append_test_details(error, test_path_string);
            error.build(Diagnostic_Highlight::error_text)
                .append(u8"Test failed because the status ("sv)
                .append(status_name(status))
                .append(u8") is not OK."sv);
            error.append(u8'\n');
            print_flush_code_string_stdout(error);
        }

        const auto actual_html_string = as_u8string_view(actual_html);
        const auto expected_html_string = as_u8string_view(expected_html);
        if (actual_html_string != expected_html_string) {
            success = false;
            Diagnostic_String error { &memory };
            append_test_details(error, test_path_string);
            error.append(
                u8"Test output HTML deviates from expected HTML as follows:\n"sv,
                Diagnostic_Highlight::error_text
            );
            if (expected_html.size() <= 2000) {
                print_lines_diff(error, expected_html_string, actual_html_string);
            }
            else {
                error.append(
                    u8"(Difference is too large to be displayed)"sv,
                    Diagnostic_Highlight::error_text
                );
            }
            error.append(u8'\n');
            print_flush_code_string_stdout(error);
        }

        if (!logger.diagnostics.empty()) {
            success = false;
            Diagnostic_String error { &memory };
            append_test_details(error, test_path_string);
            error.append(
                u8"Test failed because unexpected diagnostics were emitted:\n"sv,
                Diagnostic_Highlight::error_text
            );
            for (const Collected_Diagnostic& d : logger.diagnostics) {
                print_diagnostic(error, d, test_path_string);
            }
            print_flush_code_string_stdout(error);
        }
    }
    EXPECT_TRUE(success);
}

TEST(Document_Generation, empty_document)
{
    const std::u8string_view expected_html = u8R"(<!DOCTYPE html>
<html>
<head>
</head>
<body>
</body>
</html>
)";

    Global_Memory_Resource memory;
    Builtin_Directive_Set directives;
    const Generation_Options options {
        .highlight_theme_source = u8""sv,
        .builtin_name_resolver = directives,
        .highlighter = test_highlighter,
        .memory = &memory,
    };
    Vector_Text_Sink sink { Output_Language::html, &memory };
    const Processing_Status status = run_generation(
        [&](Context& context) -> Processing_Status {
            return write_empty_head_document(sink, {}, context);
        },
        options
    );
    const auto actual_html = as_u8string_view(*sink);

    ASSERT_EQ(status, Processing_Status::ok);
    ASSERT_EQ(actual_html, expected_html);
}

TEST(Document_Generation, documentation)
{
    constexpr auto html_path = u8"docs/index.html"sv;
    constexpr bool output_bytes_for_debugging = false;

    Global_Memory_Resource memory;

    std::pmr::vector<char8_t> source { &memory };
    ASSERT_TRUE(load_utf8_file_or_error(source, u8"docs/index.cow"sv, &memory));
    std::pmr::vector<char8_t> theme_source { &memory };
    ASSERT_TRUE(load_utf8_file_or_error(theme_source, theme_path, &memory));
    std::pmr::vector<char8_t> expected_html { &memory };
    ASSERT_TRUE(load_utf8_file_or_error(expected_html, html_path, &memory));
    ast::Pmr_Vector<ast::Markup_Element> content { &memory };

    Builtin_Directive_Set directives;
    Relative_File_Loader file_loader { "docs/", &memory };
    Collecting_Logger logger { &memory };

    const bool parse_success = lex_and_parse_and_build(
        content, as_u8string_view(source), File_Id::main, &memory, Parse_Error_Logger { logger }
    );
    ASSERT_TRUE(parse_success);

    const Generation_Options options {
        .error_behavior = &directives.get_error_behavior(),
        .highlight_theme_source = as_u8string_view(theme_source),
        .builtin_name_resolver = directives,
        .file_loader = file_loader,
        .logger = logger,
        .highlighter = test_highlighter,
        .memory = &memory,
    };
    Vector_Text_Sink sink { Output_Language::html, &memory };
    const Processing_Status status = run_generation(
        [&](Context& context) -> Processing_Status {
            return write_wg21_document(sink, content, context);
        },
        options
    );

    const auto actual_html_string = as_u8string_view(*sink);
    const auto expected_html_string = as_u8string_view(expected_html);

    if constexpr (output_bytes_for_debugging) {
        ASSERT_TRUE(bytes_to_file(actual_html_string, html_path));
    }

    EXPECT_EQ(status, Processing_Status::ok);
    EXPECT_EQ(actual_html_string, expected_html_string);
    EXPECT_TRUE(logger.diagnostics.empty());
}

} // namespace
} // namespace cowel
