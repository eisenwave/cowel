#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/ulight_highlighter.hpp"
#include "cowel/util/annotated_string.hpp"
#include "cowel/util/io.hpp"
#include "cowel/util/severity.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/collecting_logger.hpp"
#include "cowel/cowel.h"
#include "cowel/cowel_lib.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/diagnostic_highlight.hpp"
#include "cowel/fwd.hpp"
#include "cowel/memory_resources.hpp"
#include "cowel/print.hpp"
#include "cowel/relative_file_loader.hpp"
#include "cowel/x_highlighter.hpp"

#include "diff.hpp"
#include "io.hpp"

namespace cowel {
namespace {

/// @brief The directory which the test fixtures are located in.
/// Each fixture is a `.cow` file next to a diff of the same name whose suffix is `.diff`,
/// which states the auto-fixes that are expected to be applied to the file.
constexpr std::string_view autofix_directory = "engine/test/autofix";

/// @brief The path of the theme which the fixtures are generated with.
constexpr std::u8string_view theme_path = u8"ulight/themes/wg21.json";

/// @brief The ID of the diagnostic which the deprecation of `\cowel_macro` is reported with.
constexpr std::u8string_view deprecated_id = u8"deprecated";

/// @brief Returns the contents of @p str as a string view.
[[nodiscard]]
std::u8string_view as_view(const std::pmr::u8string& str)
{
    return { str.data(), str.size() };
}

/// @brief Appends the decimal representation of @p value to @p out.
void append_number(std::pmr::u8string& out, const std::size_t value)
{
    std::size_t magnitude = 1;
    while (value / magnitude >= 10) {
        magnitude *= 10;
    }
    for (; magnitude != 0; magnitude /= 10) {
        out.push_back(char8_t(u8'0' + (value / magnitude) % 10));
    }
}

/// @brief Consumes @p expected from the beginning of @p text.
/// @return Whether @p text started with @p expected.
[[nodiscard]]
bool consume_text(std::u8string_view& text, const std::u8string_view expected)
{
    if (!text.starts_with(expected)) {
        return false;
    }
    text.remove_prefix(expected.size());
    return true;
}

/// @brief Consumes a sequence of decimal digits from the beginning of @p text.
/// @return Whether at least one digit was consumed.
[[nodiscard]]
bool consume_number(std::u8string_view& text, std::size_t& out)
{
    std::size_t value = 0;
    bool consumed = false;
    while (!text.empty() && text.front() >= u8'0' && text.front() <= u8'9') {
        value = value * 10 + std::size_t(text.front() - u8'0');
        text.remove_prefix(1);
        consumed = true;
    }
    out = value;
    return consumed;
}

/// @brief A single hunk of a unified diff.
struct Diff_Hunk {
    /// @brief The line number of the first line which the hunk consumes in the original document.
    /// The line number is 1-based,
    /// and zero if the hunk inserts lines at the very beginning of the document.
    std::size_t old_start = 0;

    /// @brief The number of lines which the hunk consumes in the original document.
    std::size_t old_count = 0;

    /// @brief The number of lines which the hunk produces for the patched document.
    std::size_t new_count = 0;

    /// @brief The lines of the hunk body, including their `' '`, `'-'`, or `'+'` prefix.
    std::pmr::vector<std::u8string_view> lines;

    explicit Diff_Hunk(std::pmr::memory_resource* const memory)
        : lines { memory }
    {
    }
};

/// @brief Parses a hunk header such as `@@ -1,2 +1,2 @@`.
/// @return Whether @p line is a well-formed hunk header.
[[nodiscard]]
bool parse_hunk_header(Diff_Hunk& hunk, const std::u8string_view line)
{
    std::u8string_view text = line;
    if (!consume_text(text, u8"@@ -")) {
        return false;
    }
    if (!consume_number(text, hunk.old_start)) {
        return false;
    }
    hunk.old_count = 1;
    if (consume_text(text, u8",") && !consume_number(text, hunk.old_count)) {
        return false;
    }
    if (!consume_text(text, u8" +")) {
        return false;
    }
    std::size_t new_start = 0;
    if (!consume_number(text, new_start)) {
        return false;
    }
    hunk.new_count = 1;
    if (consume_text(text, u8",") && !consume_number(text, hunk.new_count)) {
        return false;
    }
    // Text following the closing `@@`, such as the name of the enclosing function,
    // is ignored.
    return text.empty() || text.starts_with(u8" @@");
}

/// @brief Applies a unified diff to @p source.
///
/// Only hunk headers and hunk bodies are considered;
/// the file headers `---` and `+++` as well as all other text outside of a hunk are ignored.
/// Hunks must be in ascending order and must not overlap.
/// The lines which a hunk keeps or removes are verified against the source code.
///
/// The source code and the patch must end with a newline,
/// which is the case for any diff that is produced by `diff -u`.
///
/// @param source The source code which the patch is applied to.
/// @param patch The unified diff.
/// @param memory The memory resource.
/// @param error Appended to if the patch could not be applied.
/// @return The patched source code, or nothing if the patch could not be applied.
[[nodiscard]]
std::optional<std::pmr::u8string> apply_unified_diff(
    const std::u8string_view source,
    const std::u8string_view patch,
    std::pmr::memory_resource* const memory,
    std::pmr::u8string& error
)
{
    if (!source.ends_with(u8"\n")) {
        error.append(u8"The source code does not end with a newline.\n");
        return std::nullopt;
    }

    std::pmr::vector<std::u8string_view> source_lines { memory };
    split_lines(source_lines, source);

    std::pmr::vector<std::u8string_view> patch_lines { memory };
    split_lines(patch_lines, patch);

    // `split_lines` reports an additional, empty line for the trailing newline,
    // which must not be mistaken for a line of the patch.
    while (!patch_lines.empty() && patch_lines.back().empty()) {
        patch_lines.pop_back();
    }

    // Parse the patch into hunks.
    std::pmr::vector<Diff_Hunk> hunks { memory };
    for (std::size_t index = 0; index < patch_lines.size();) {
        if (!patch_lines[index].starts_with(u8"@@")) {
            ++index;
            continue;
        }

        Diff_Hunk hunk { memory };
        if (!parse_hunk_header(hunk, patch_lines[index])) {
            error.append(u8"Failed to parse the hunk header `");
            error.append(patch_lines[index]);
            error.append(u8"`.\n");
            return std::nullopt;
        }
        ++index;

        std::size_t consumed = 0;
        std::size_t produced = 0;
        while (index < patch_lines.size() && !patch_lines[index].starts_with(u8"@@")) {
            const std::u8string_view line = patch_lines[index];
            if (line.starts_with(u8"-")) {
                ++consumed;
            }
            else if (line.starts_with(u8"+")) {
                ++produced;
            }
            else if (line.starts_with(u8" ")) {
                ++consumed;
                ++produced;
            }
            else {
                error.append(u8"Unsupported line in the body of a hunk: `");
                error.append(line);
                error.append(u8"`.\n");
                return std::nullopt;
            }
            hunk.lines.push_back(line);
            ++index;
        }

        if (consumed != hunk.old_count || produced != hunk.new_count) {
            error.append(u8"The hunk `-");
            append_number(error, hunk.old_count);
            error.append(u8" +");
            append_number(error, hunk.new_count);
            error.append(u8"` does not match its body of ");
            append_number(error, consumed);
            error.append(u8" consumed and ");
            append_number(error, produced);
            error.append(u8" produced lines.\n");
            return std::nullopt;
        }
        hunks.push_back(std::move(hunk));
    }

    // Apply the hunks in order.
    // The result is built by joining lines with newlines.
    // Note that `split_lines` reports the trailing newline of the source code
    // as an additional, empty line,
    // so appending all lines faithfully reproduces the source code.
    std::pmr::u8string result { memory };
    bool first_line = true;
    const auto append_line = [&result, &first_line](const std::u8string_view line) {
        if (!first_line) {
            result.push_back(u8'\n');
        }
        result.append(line);
        first_line = false;
    };

    // The next line of `source_lines` which is appended to `result`.
    std::size_t source_index = 0;
    for (const Diff_Hunk& hunk : hunks) {
        // A hunk which does not consume any lines inserts after `old_start` lines,
        // which is also how an insertion at the beginning of the document is expressed.
        const std::size_t hunk_index = hunk.old_count == 0 ? hunk.old_start : hunk.old_start - 1;
        if (hunk_index < source_index) {
            error.append(u8"The hunks overlap.\n");
            return std::nullopt;
        }
        if (hunk_index + hunk.old_count > source_lines.size()) {
            error.append(u8"A hunk consumes lines beyond the end of the source code.\n");
            return std::nullopt;
        }

        // The lines which the hunk keeps or removes must match the source code.
        std::size_t check_index = hunk_index;
        for (const std::u8string_view line : hunk.lines) {
            if (line.starts_with(u8"+")) {
                continue;
            }
            if (source_lines[check_index] != line.substr(1)) {
                error.append(u8"The source code does not match the hunk at line ");
                append_number(error, check_index + 1);
                error.append(u8":\n  in the source code: ");
                error.append(source_lines[check_index]);
                error.append(u8"\n  in the patch: ");
                error.append(line.substr(1));
                error.append(u8"\n");
                return std::nullopt;
            }
            ++check_index;
        }

        while (source_index < hunk_index) {
            append_line(source_lines[source_index]);
            ++source_index;
        }
        for (const std::u8string_view line : hunk.lines) {
            if (!line.starts_with(u8"-")) {
                append_line(line.substr(1));
            }
        }
        source_index = hunk_index + hunk.old_count;
    }

    while (source_index < source_lines.size()) {
        append_line(source_lines[source_index]);
        ++source_index;
    }
    return result;
}

/// @brief Applies @p patch to @p source, and verifies that it produces @p expected.
/// The first line of the output is the expectation if the check fails.
/// @param label A description of the test case which is printed if the check fails.
/// @return Whether the patch could be applied and produced @p expected.
[[nodiscard]]
bool check_apply_patch(
    const std::u8string_view label,
    const std::u8string_view source,
    const std::u8string_view patch,
    const std::u8string_view expected,
    std::pmr::memory_resource* const memory
)
{
    std::pmr::u8string error { memory };
    const std::optional<std::pmr::u8string> result
        = apply_unified_diff(source, patch, memory, error);
    if (!result.has_value()) {
        Diagnostic_String out { memory };
        out.append(label, Diagnostic_Highlight::code_position);
        out.append(u8" FAILED: ", Diagnostic_Highlight::error);
        out.append(u8"the patch could not be applied: ", Diagnostic_Highlight::error_text);
        out.append(error);
        print_flush_code_string_stdout(out);
        return false;
    }
    if (as_view(*result) != expected) {
        Diagnostic_String out { memory };
        out.append(label, Diagnostic_Highlight::code_position);
        out.append(u8" FAILED: ", Diagnostic_Highlight::error);
        out.append(
            u8"the patched source code deviates from the expectation:",
            Diagnostic_Highlight::error_text
        );
        out.append(u8'\n');
        print_lines_diff(out, expected, as_view(*result));
        print_flush_code_string_stdout(out);
        return false;
    }
    return true;
}

/// @brief Applies @p patch to @p source, and verifies that it cannot be applied.
/// @return Whether the patch could not be applied.
[[nodiscard]]
bool check_apply_failure(
    const std::u8string_view source,
    const std::u8string_view patch,
    std::pmr::memory_resource* const memory
)
{
    std::pmr::u8string error { memory };
    return !apply_unified_diff(source, patch, memory, error).has_value();
}

/// @brief Prints that the test fixture @p test_path passed.
void report_ok(const std::u8string_view test_path, std::pmr::memory_resource* const memory)
{
    Diagnostic_String out { memory };
    out.append(test_path, Diagnostic_Highlight::code_position);
    out.append(u8" OK\n", Diagnostic_Highlight::success);
    print_flush_code_string_stdout(out);
}

/// @brief Prints that the test fixture @p test_path failed, appending @p message.
/// @param message must end with a newline.
void report_error(
    const std::u8string_view message,
    const std::u8string_view test_path,
    std::pmr::memory_resource* const memory
)
{
    Diagnostic_String out { memory };
    out.append(test_path, Diagnostic_Highlight::code_position);
    out.append(u8" FAILED: ", Diagnostic_Highlight::error);
    out.append(message, Diagnostic_Highlight::error_text);
    print_flush_code_string_stdout(out);
}

/// @brief Prints @p diagnostic with its location in @p file.
void print_diagnostic(
    Diagnostic_String& out,
    const Collected_Diagnostic& d,
    const std::u8string_view file
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

/// @brief Prints all @p diagnostics with their location in @p file.
void print_diagnostics(
    const std::span<const Collected_Diagnostic> diagnostics,
    const std::u8string_view file,
    std::pmr::memory_resource* const memory
)
{
    if (diagnostics.empty()) {
        return;
    }
    Diagnostic_String out { memory };
    for (const Collected_Diagnostic& d : diagnostics) {
        print_diagnostic(out, d, file);
    }
    print_flush_code_string_stdout(out);
}

/// @brief Generates documents from COWEL source code,
/// with the ability to apply auto-fixes and to inspect the collected diagnostics.
struct Autofix_Runner {
    Global_Memory_Resource memory;
    Allocator_Options alloc_options = Allocator_Options::from_memory_resource(&memory);
    std::pmr::vector<char8_t> theme_source { &memory };
    Collecting_Logger logger { &memory };

    /// @brief Loads the theme which documents are generated with.
    /// @return Whether the theme could be loaded.
    [[nodiscard]]
    bool load_theme()
    {
        return load_utf8_file_or_error(theme_source, theme_path, &memory);
    }

    /// @brief Generates a document from @p source.
    /// The diagnostics which are emitted during generation are collected in @ref logger,
    /// which must be cleared by the caller before a call.
    /// @param source The source code of the main document.
    /// @param directory The directory which files are loaded relative to.
    /// @param flags The `cowel_gen_flags` which to generate with.
    /// @return The output of the generation, or nothing if generation failed.
    [[nodiscard]]
    std::optional<std::pmr::u8string>
    run(const std::u8string_view source,
        const std::filesystem::path& directory,
        const std::uint32_t flags)
    {
        Relative_File_Loader file_loader { std::filesystem::path { directory }, &memory };
        const auto load_file_fn = file_loader.as_cowel_load_file_fn();
        const auto log_fn = logger.as_cowel_log_fn();

        static const auto syntax_highlighter = x_highlighter.as_cowel_syntax_highlighter();

        const cowel_options_u8 options {
            .source = as_cowel_string_view(source),
            .highlight_theme_json = as_cowel_string_view(as_u8string_view(theme_source)),
            .mode = COWEL_MODE_DOCUMENT,
            .flags = flags,
            .min_log_severity = COWEL_SEVERITY_DEBUG,
            .preserved_variables = nullptr,
            .preserved_variables_size = 0,
            .consume_variables = nullptr,
            .consume_variables_data = nullptr,
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
            .preamble = {},
        };

        cowel_gen_result_u8 result = cowel_generate_html_u8(&options);
        const bool ok = result.status == COWEL_PROCESSING_OK;
        std::pmr::u8string output { &memory };
        output.assign(as_u8string_view(result.output));
        cowel_free_gen_result_u8(&options, &result);
        if (!ok) {
            return std::nullopt;
        }
        return output;
    }
};

TEST(Autofix, apply_unified_diff)
{
    Global_Memory_Resource memory;

    // An empty diff leaves the source code unchanged.
    EXPECT_TRUE(check_apply_patch(u8"empty diff", u8"one\ntwo\n", u8"", u8"one\ntwo\n", &memory));

    // A single hunk which replaces the first line.
    EXPECT_TRUE(check_apply_patch(
        u8"first line", u8"one\ntwo\nthree\n", u8R"(--- a/sample.cow
+++ b/sample.cow
@@ -1,3 +1,2 @@
-one
+TWO
 two
-three
)",
        u8"TWO\ntwo\n", &memory
    ));

    // Multiple hunks which are separated by lines that the patch does not mention.
    EXPECT_TRUE(check_apply_patch(
        u8"multiple hunks", u8"a\nb\nc\nd\ne\nf\n", u8R"(@@ -1,2 +1,2 @@
-a
+A
 b
@@ -5,2 +5,3 @@
 e
-f
+F
+G
)",
        u8"A\nb\nc\nd\ne\nF\nG\n", &memory
    ));

    // A hunk which inserts lines at the end of the document.
    EXPECT_TRUE(check_apply_patch(
        u8"insertion at the end", u8"a\nb\n", u8R"(@@ -2,0 +3,1 @@
+c
)",
        u8"a\nb\nc\n", &memory
    ));

    // A hunk whose context does not match the source code.
    EXPECT_TRUE(check_apply_failure(
        u8"a\nb\n", u8R"(@@ -1,2 +1,2 @@
 x
-y
+Y
)",
        &memory
    ));

    // A hunk body which does not match the counts of its header.
    EXPECT_TRUE(check_apply_failure(
        u8"a\nb\n", u8R"(@@ -1,1 +1,1 @@
-a
+A
+B
)",
        &memory
    ));

    // A hunk which references lines beyond the end of the source code.
    EXPECT_TRUE(check_apply_failure(
        u8"a\nb\n", u8R"(@@ -5,2 +5,2 @@
 a
-b
+B
)",
        &memory
    ));

    // The source code must end with a newline, which is verified explicitly.
    EXPECT_TRUE(check_apply_failure(
        u8"a", u8R"(@@ -1,1 +1,1 @@
-a
+A
)",
        &memory
    ));
}

TEST(Autofix, file_tests)
{
    Autofix_Runner runner;
    ASSERT_TRUE(runner.load_theme());

    std::pmr::vector<fs::path> test_paths { &runner.memory };
    const auto filter = [](const fs::directory_entry& entry) {
        return entry.path().generic_u8string().ends_with(u8".cow");
    };
    find_files_recursively(test_paths, autofix_directory, filter);
    std::ranges::sort(test_paths);
    ASSERT_FALSE(test_paths.empty());

    std::pmr::vector<char8_t> source { &runner.memory };
    std::pmr::vector<char8_t> expected_diff { &runner.memory };

    bool success = true;
    for (const fs::path& test_path : test_paths) {
        const std::u8string test_path_string = test_path.generic_u8string();

        // Every fixture is paired with a diff which states the auto-fixes that are expected.
        // An empty diff expresses that no auto-fixes are expected,
        // which is used to test definitions of `\cowel_macro` that cannot be converted.
        fs::path diff_path = test_path;
        diff_path.replace_extension(".diff");
        const std::u8string diff_path_string = diff_path.generic_u8string();

        if (!fs::is_regular_file(diff_path)) {
            report_error(
                u8"the fixture does not have a companion diff file.\n", test_path_string,
                &runner.memory
            );
            success = false;
            continue;
        }

        source.clear();
        expected_diff.clear();
        ASSERT_TRUE(load_utf8_file_or_error(source, test_path_string, &runner.memory));
        ASSERT_TRUE(load_utf8_file_or_error(expected_diff, diff_path_string, &runner.memory));

        const std::u8string_view source_view = as_u8string_view(source);
        const std::u8string_view diff_view = as_u8string_view(expected_diff);

        // Generate the fixed source code and check the diagnostics.
        runner.logger.diagnostics.clear();
        const std::optional<std::pmr::u8string> fixed_source
            = runner.run(source_view, test_path.parent_path(), COWEL_GEN_FLAGS_FIX_ALL);
        if (!fixed_source.has_value()) {
            report_error(
                u8"document generation with auto-fixes failed:\n", test_path_string, &runner.memory
            );
            print_diagnostics(runner.logger.diagnostics, test_path_string, &runner.memory);
            success = false;
            continue;
        }

        // The fixtures are only expected to trigger deprecation warnings.
        // Any other diagnostic indicates a problem with the fixture,
        // rather than with the auto-fixes that are tested here.
        const auto predicate = [](const Collected_Diagnostic& d) {
            return d.severity == Severity::warning && d.id == deprecated_id;
        };
        if (!std::ranges::all_of(runner.logger.diagnostics, predicate)) {
            report_error(u8"unexpected diagnostics:\n", test_path_string, &runner.memory);
            print_diagnostics(runner.logger.diagnostics, test_path_string, &runner.memory);
            success = false;
            continue;
        }

        const std::u8string_view fixed_view = as_view(*fixed_source);

        // The diff must apply to the source code and must produce the auto-fixed source code.
        bool fixture_success = check_apply_patch(
            test_path_string, source_view, diff_view, fixed_view, &runner.memory
        );

        // Auto-fixes must not alter the generated document.
        runner.logger.diagnostics.clear();
        const std::optional<std::pmr::u8string> expected_html
            = runner.run(source_view, test_path.parent_path(), COWEL_GEN_FLAGS_NONE);
        const std::optional<std::pmr::u8string> fixed_html
            = runner.run(fixed_view, test_path.parent_path(), COWEL_GEN_FLAGS_NONE);
        if (!expected_html.has_value() || !fixed_html.has_value()) {
            report_error(
                u8"document generation of the original or the fixed source code failed:\n",
                test_path_string, &runner.memory
            );
            print_diagnostics(runner.logger.diagnostics, test_path_string, &runner.memory);
            success = false;
            continue;
        }
        if (expected_html != fixed_html) {
            Diagnostic_String out { &runner.memory };
            out.append(test_path_string, Diagnostic_Highlight::code_position);
            out.append(u8" FAILED: ", Diagnostic_Highlight::error);
            out.append(
                u8"the auto-fixes alter the generated document, "
                u8"the first document is generated from the original source code:\n",
                Diagnostic_Highlight::error_text
            );
            print_lines_diff(out, as_view(*expected_html), as_view(*fixed_html));
            print_flush_code_string_stdout(out);
            fixture_success = false;
        }

        if (!fixture_success) {
            success = false;
            continue;
        }
        report_ok(test_path_string, &runner.memory);
    }
    EXPECT_TRUE(success);
}

} // namespace
} // namespace cowel
