#include <filesystem>
#include <initializer_list>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/util/annotated_string.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/meta.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/capture.hpp"
#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/html.hpp"
#include "cowel/policy/paragraph_split.hpp"

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
#include "test_data.hpp"
#include "test_highlighter.hpp"

namespace cowel {
namespace {

using Suppress_Unused_Include_Annotated_String = Basic_Annotated_String<void, void>;

[[nodiscard]]
Processing_Status
write_empty_head_document(Text_Sink& out, std::span<const ast::Content> content, Context& context)
{
    constexpr auto write_head = [](Content_Policy&, std::span<const ast::Content>, Context&) {
        return Processing_Status::ok;
    };
    constexpr auto write_body
        = [](Content_Policy& policy, std::span<const ast::Content> content, Context& context) {
              return consume_all(policy, content, Frame_Index::root, context);
          };

    return write_head_body_document(out, content, context, const_v<write_head>, const_v<write_body>);
}

struct Doc_Gen_Test : testing::Test {
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> out { &memory };

    Builtin_Directive_Set builtin_directives {};

    std::filesystem::path file_path;

private:
    std::pmr::vector<char8_t> source { &memory };
    std::pmr::vector<char8_t> theme_source { &memory };

public:
    std::u8string_view source_string {};
    std::u8string_view theme_source_string {};
    ast::Pmr_Vector<ast::Content> content { &memory };

    Collecting_Logger logger { &memory };

    [[nodiscard]]
    explicit Doc_Gen_Test()
    {
        const bool theme_loaded = load_theme();
        COWEL_ASSERT(theme_loaded);
    }

    [[nodiscard]]
    Processing_Status run_test_with_behavior(Context& context, Test_Behavior behavior)
    {
        Capturing_Ref_Text_Sink sink { out, Output_Language::html };
        switch (behavior) {
        case Test_Behavior::trivial: {
            HTML_Content_Policy policy { sink };
            return consume_all(policy, content, Frame_Index::root, context);
        }

        case Test_Behavior::paragraphs: {
            Vector_Text_Sink buffer { Output_Language::html, context.get_transient_memory() };
            Paragraph_Split_Policy policy { buffer, context.get_transient_memory() };
            const Processing_Status result
                = consume_all(policy, content, Frame_Index::root, context);
            policy.leave_paragraph();
            resolve_references(sink, as_u8string_view(*buffer), context, File_Id::main);
            return result;
        }

        case Test_Behavior::empty_head: {
            return write_empty_head_document(sink, content, context);
        }

        case Test_Behavior::wg21: {
            return write_wg21_document(sink, content, context);
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid behavior.");
    }

    // This needs to be a template so that the Function_Ref only captures this.
    template <Test_Behavior behavior>
    Function_Ref<Processing_Status(Context&)> get_behavior_impl()
    {
        constexpr auto f = [](Doc_Gen_Test* self, Context& context) {
            return self->run_test_with_behavior(context, behavior);
        };
        return { const_v<f>, this };
    }

    [[nodiscard]]
    Function_Ref<Processing_Status(Context&)> get_behavior(Test_Behavior behavior)
    {
        switch (behavior) {
        case Test_Behavior::trivial: return get_behavior_impl<Test_Behavior::trivial>();
        case Test_Behavior::paragraphs: return get_behavior_impl<Test_Behavior::paragraphs>();
        case Test_Behavior::empty_head: return get_behavior_impl<Test_Behavior::empty_head>();
        case Test_Behavior::wg21: return get_behavior_impl<Test_Behavior::wg21>();
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid behavior.");
    }

    [[nodiscard]]
    bool load_document(const std::filesystem::path& path)
    {
        file_path = "test" / path;
        if (!load_utf8_file_or_error(source, file_path.generic_u8string(), &memory)) {
            return false;
        }
        source_string = as_u8string_view(source);
        content
            = parse_and_build(source_string, File_Id::main, &memory, make_parse_error_consumer());
        return true;
    }

    void load_source(std::u8string_view source)
    {
        source_string = source;
        content = parse_and_build(source, File_Id::main, &memory, make_parse_error_consumer());
    }

    [[nodiscard]]
    bool load_theme()
    {
        constexpr std::u8string_view theme_path = u8"ulight/themes/wg21.json";

        if (!load_utf8_file_or_error(theme_source, theme_path, &memory)) {
            return false;
        }
        theme_source_string = as_u8string_view(theme_source);
        return true;
    }

    [[nodiscard]]
    Processing_Status generate(Test_Behavior behavior)
    {
        auto relative_loader = [&] -> std::optional<Relative_File_Loader> {
            if (file_path.empty()) {
                return {};
            }
            return Relative_File_Loader { file_path.parent_path(), &memory };
        }();
        auto& loader = relative_loader ? static_cast<File_Loader&>(*relative_loader)
                                       : static_cast<File_Loader&>(always_failing_file_loader);

        const Directive_Behavior& error_behavior = builtin_directives.get_error_behavior();
        const Generation_Options options {
            .error_behavior = &error_behavior,
            .highlight_theme_source = theme_source_string,
            .builtin_name_resolver = builtin_directives,
            .file_loader = loader,
            .logger = logger,
            .highlighter = test_highlighter,
            .memory = &memory,
        };
        const Function_Ref<Processing_Status(Context&)> f = get_behavior(behavior);
        return run_generation(f, options);
    }

    [[nodiscard]]
    std::u8string_view output_text() const
    {
        return as_u8string_view(out);
    }

    [[nodiscard]]
    Parse_Error_Consumer make_parse_error_consumer()
    {
        constexpr auto invoker = [](Doc_Gen_Test* self, std::u8string_view id,
                                    const File_Source_Span& location, std::u8string_view message) {
            const Diagnostic d { Severity::error, id, location, message };
            self->logger(d);
        };
        return Parse_Error_Consumer { const_v<invoker>, this };
    }

    void clear()
    {
        out.clear();
        source.clear();
        content.clear();
        logger.diagnostics.clear();
    }
};

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

void append_test_details(Diagnostic_String& out, const Basic_Test& test)
{
    if (const auto* const source = std::get_if<Source>(&test.document)) {
        out.append(u8'\"', Diagnostic_Highlight::code_citation);
        append_specials_escaped(out, source->contents, Diagnostic_Highlight::code_citation);
        out.append(u8'\"', Diagnostic_Highlight::code_citation);
    }
    else if (const auto* const path = std::get_if<Path>(&test.document)) {
        out.append(path->value, Diagnostic_Highlight::code_position);
    }
    out.append(u8':', Diagnostic_Highlight::punctuation);
    out.append(u8' ');
}

TEST_F(Doc_Gen_Test, basic_directive_tests)
{
    bool success = true;
    for (const Basic_Test& test : basic_tests) {
        clear();

        const bool load_success = load_basic_test_input(*this, test);
        if (!load_success) {
            success = false;
            continue;
        }

        std::pmr::vector<char8_t> expectation_storage { &memory };
        const std::u8string_view expected
            = load_basic_test_expectations(expectation_storage, test, &memory);
        COWEL_ASSERT(source_string.empty() || !expected.empty());

        const Processing_Status status = generate(test.behavior);
        if (status != test.expected_status) {
            success = false;
            Diagnostic_String error { &memory };
            append_test_details(error, test);
            error.build(Diagnostic_Highlight::error_text)
                .append(u8"Test failed because the status ("sv)
                .append(status_name(status))
                .append(u8") was not as expected ("sv)
                .append(status_name(test.expected_status))
                .append(u8')');
            error.append(u8'\n');
            print_flush_code_string_stdout(error);
        }
        if (expected != output_text()) {
            success = false;
            Diagnostic_String error { &memory };
            append_test_details(error, test);
            error.append(
                u8"Test output HTML deviates from expected HTML as follows:\n"sv,
                Diagnostic_Highlight::error_text
            );
            if (expected.length() <= 2000) {
                print_lines_diff(error, expected, output_text());
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

        if (test.expected_diagnostics.size() == 0) {
            if (!logger.diagnostics.empty()) {
                success = false;
                Diagnostic_String error { &memory };
                append_test_details(error, test);
                error.append(
                    u8"Test failed because unexpected diagnostics were emitted:\n"sv,
                    Diagnostic_Highlight::error_text
                );
                for (const Collected_Diagnostic& d : logger.diagnostics) {
                    error.append(d.message, Diagnostic_Highlight::text);
                    error.append(u8' ');
                    error.build(Diagnostic_Highlight::code_position)
                        .append(u8'[')
                        .append(logger.diagnostics.front().id)
                        .append(u8']');
                    error.append(u8'\n');
                }
                print_flush_code_string_stdout(error);
            }
            continue;
        }
        for (const std::u8string_view id : test.expected_diagnostics) {
            if (logger.was_logged(id)) {
                continue;
            }
            success = false;
            Diagnostic_String error { &memory };
            append_test_details(error, test);
            error.append(
                u8"Test failed because an expected diagnostic was not emitted: "sv,
                Diagnostic_Highlight::error_text
            );
            error.append(id, Diagnostic_Highlight::code_citation);
            error.append(u8"\n");
            print_flush_code_string_stdout(error);
        }
    }
    EXPECT_TRUE(success);
}

} // namespace
} // namespace cowel
