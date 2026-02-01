#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/ulight_highlighter.hpp"
#include "cowel/util/annotated_string.hpp"
#include "cowel/util/meta.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/capture.hpp"
#include "cowel/policy/content_policy.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/collecting_logger.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/cowel.h"
#include "cowel/cowel_lib.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/diagnostic_highlight.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/document_generation.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"
#include "cowel/parse.hpp"
#include "cowel/print.hpp"
#include "cowel/relative_file_loader.hpp"

#include "diff.hpp"
#include "io.hpp"
#include "test_highlighter.hpp"

namespace cowel {
namespace {

constexpr std::u8string_view theme_path = u8"ulight/themes/wg21.json";

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

constexpr std::u8string_view preamble = u8R"!(
\cowel_macro(test_input){\cowel_var_let(__test_input,"\cowel_as_text{\cowel_to_html{\cowel_put}}")}
\cowel_macro(test_output){\cowel_var_let(__test_output,"\__arg_source_as_text(cowel_put())")}
\cowel_alias(test_expect_warning){__expect_warning}
\cowel_alias(test_expect_error){__expect_error}
\cowel_alias(test_expect_fatal){__expect_fatal}
)!"sv;

TEST(Document_Generation, file_tests)
{
    Global_Memory_Resource memory;
    const auto alloc_options = Allocator_Options::from_memory_resource(&memory);

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
    const auto log_fn = logger.as_cowel_log_fn();

    std::pmr::vector<char8_t> theme_source { &memory };
    ASSERT_TRUE(load_utf8_file_or_error(theme_source, theme_path, &memory));

    static constexpr cowel_string_view_u8 preserved_variables[] {
        as_cowel_string_view(u8"__test_input"sv),
        as_cowel_string_view(u8"__test_output"sv),
    };
    const auto consume_variables_lambda = [&](const cowel_string_view_u8* const variables,
                                              const std::size_t size) { //
        COWEL_ASSERT(size == 2);
        const auto test_input = as_u8string_view(variables[0]);
        const auto test_output = as_u8string_view(variables[1]);

        actual_html.assign(test_input.data(), test_input.data() + test_input.size());
        expected_html.assign(test_output.data(), test_output.data() + test_output.size());
    };
    const Function_Ref<void(const cowel_string_view_u8*, std::size_t)> consume_variables
        = consume_variables_lambda;

    static const auto syntax_highlighter = x_highlighter.as_cowel_syntax_highlighter();

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

        Relative_File_Loader file_loader { test_path.parent_path(), &memory };
        const auto load_file_fn = file_loader.as_cowel_load_file_fn();

        const cowel_options_u8 cowel_options {
            .source = as_cowel_string_view(as_u8string_view(source)),
            .highlight_theme_json = as_cowel_string_view(as_u8string_view(theme_source)),
            .mode = COWEL_MODE_MINIMAL,
            .min_log_severity = COWEL_SEVERITY_DEBUG,
            .preserved_variables = preserved_variables,
            .preserved_variables_size = std::size(preserved_variables),
            .consume_variables = consume_variables.get_invoker(),
            .consume_variables_data = consume_variables.get_entity(),
            .alloc = alloc_options.alloc,
            .alloc_data = alloc_options.alloc_data,
            .free = alloc_options.free,
            .free_data = alloc_options.free_data,
            .load_file = load_file_fn.get_invoker(),
            .load_file_data = load_file_fn.get_entity(),
            .log = log_fn.get_invoker(),
            .log_data = log_fn.get_entity(),
            .highlighter = &syntax_highlighter,
            .highlight_policy = COWEL_SYNTAX_HIGHLIGHT_POLICY_FALL_BACK,
            .preamble = as_cowel_string_view(preamble),
        };

        const cowel_gen_result_u8 result = cowel_generate_html_u8(&cowel_options);
        if (result.output.text != nullptr) {
            alloc_options.free(
                alloc_options.free_data, result.output.text, result.output.length, alignof(char8_t)
            );
        }

        if (result.status != COWEL_PROCESSING_OK) {
            success = false;
            Diagnostic_String error { &memory };
            append_test_details(error, test_path_string);
            error.build(Diagnostic_Highlight::error_text)
                .append(u8"Test failed because the status ("sv)
                .append(status_name(Processing_Status(result.status)))
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
        .highlighter = ulight_syntax_highlighter,
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
        .highlighter = ulight_syntax_highlighter,
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
