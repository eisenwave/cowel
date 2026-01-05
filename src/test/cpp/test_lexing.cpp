#include <algorithm>
#include <cstddef>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/util/annotated_string.hpp"
#include "cowel/util/ascii_algorithm.hpp"
#include "cowel/util/char_sequence_ops.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/io.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/tty.hpp"
#include "cowel/util/unicode.hpp"

#include "cowel/diagnostic_highlight.hpp"
#include "cowel/fwd.hpp"
#include "cowel/lex.hpp"
#include "cowel/memory_resources.hpp"
#include "cowel/print.hpp"

#include "diff.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

[[nodiscard]]
constexpr bool is_ascii_printing(char8_t c)
{
    // TODO: move this into ulight
    static constexpr ulight::Charset256 chars //
        = ulight::is_ascii_alphanumeric_set //
        | ulight::is_ascii_punctuation_set //
        | ulight::detail::to_charset256(u8' ');
    return chars.contains(c);
}

[[nodiscard]]
constexpr bool is_ascii_printing(char32_t c)
{
    return is_ascii(c) && is_ascii_printing(char8_t(c));
}

struct Lex_Actual_Error {
    Source_Span location;
    std::pmr::u8string message;
};

struct Text_Token {
    Token_Kind kind;
    std::pmr::u8string text;

    [[nodiscard]]
    friend bool operator==(const Text_Token&, const Text_Token&)
        = default;
};

struct [[nodiscard]] Lex_Actual {
    std::pmr::vector<char8_t> source;
    std::pmr::vector<Text_Token> tokens;
    std::pmr::vector<Lex_Actual_Error> diagnostics;
    bool success = false;

    [[nodiscard]]
    std::u8string_view source_string() const
    {
        return { source.data(), source.size() };
    }
};

struct Expected_Error {
    std::size_t begin;
    std::size_t length;
};

struct [[nodiscard]] Lex_Expectations {
    std::pmr::vector<char8_t> source;
    std::pmr::vector<Text_Token> tokens;
    std::pmr::vector<Expected_Error> diagnostics;
    bool success = false;

    [[nodiscard]]
    std::u8string_view source_string() const
    {
        return { source.data(), source.size() };
    }
};

[[nodiscard]]
std::optional<Lex_Actual> lex_file(
    const std::u8string_view file, //
    std::pmr::memory_resource* const memory
)
{
    Lex_Actual result {
        .source = std::pmr::vector<char8_t> { memory },
        .tokens = std::pmr::vector<Text_Token> { memory },
        .diagnostics = std::pmr::vector<Lex_Actual_Error> { memory },
    };

    const Result<void, cowel::IO_Error_Code> r = load_utf8_file(result.source, file);
    if (!r) {
        Diagnostic_String out { memory };
        print_io_error(out, file, r.error());
        print_code_string_stdout(out);
        return {};
    }

    const std::convertible_to<Lex_Error_Consumer> auto consumer
        = [&](std::u8string_view /* id */, const Source_Span& location, Char_Sequence8 message) {
              Lex_Actual_Error error { location, to_string(message, memory) };
              result.diagnostics.push_back(std::move(error));
          };

    std::pmr::vector<Token> lex_tokens { memory };
    result.success = lex(lex_tokens, result.source_string(), consumer);

    for (std::size_t pos = 0; const auto& token : lex_tokens) {
        const auto* const token_start = result.source.data() + pos;
        std::pmr::u8string current_text { token_start, token.location.length, memory };
        result.tokens.push_back({ token.kind, std::move(current_text) });
        pos += token.location.length;
    }
    return result;
}

void decode_expectation_argument(std::pmr::u8string& out, const std::u8string_view arg)
{
    for (std::size_t i = 0; i < arg.length();) {
        if (arg[i] != u8'\\') {
            out += arg[i];
            ++i;
            continue;
        }

        ++i;
        COWEL_ASSERT(i < arg.length());
        if (arg[i] != u8'u' && arg[i] != u8'U') {
            out += arg[i];
            ++i;
            continue;
        }

        const std::size_t digits_length = arg[i] == u8'u' ? 4 : 8;
        COWEL_ASSERT(i + digits_length <= arg.length());

        const std::u8string_view hex_digits = arg.substr(i + 1, digits_length);
        const std::optional<std::uint32_t> code_point
            = from_characters<std::uint32_t>(hex_digits, 16);
        COWEL_ASSERT(code_point);
        const utf8::Code_Units_And_Length encoded
            = utf8::encode8_unchecked(char32_t { *code_point });
        out += encoded.as_string();
        i += 1 + digits_length;
    }
}

[[nodiscard]]
std::u8string_view token_kind_name(const Token_Kind kind)
{
#define COWEL_TOKEN_KIND_SWITCH_CASE(id, name, first)                                              \
    case Token_Kind::id: return u8##name##sv;

    switch (kind) {
        COWEL_TOKEN_KIND_ENUM_DATA(COWEL_TOKEN_KIND_SWITCH_CASE)
    }
    return {};
}

[[nodiscard]]
constexpr std::optional<Token_Kind> token_by_name(const std::u8string_view name)
{
    struct Name_And_Token {
        std::u8string_view name;
        Token_Kind kind;
    };
#define COWEL_TOKEN_KIND_TABLE_ENTRY(id, name, first) { u8##name##sv, Token_Kind::id },

    static constexpr Name_And_Token table[] {
        COWEL_TOKEN_KIND_ENUM_DATA(COWEL_TOKEN_KIND_TABLE_ENTRY)

    };
    static_assert(std::ranges::is_sorted(table, {}, &Name_And_Token::name));

    const auto* const result = std::ranges::lower_bound(table, name, {}, &Name_And_Token::name);
    if (result == std::ranges::end(table) || result->name != name) {
        return {};
    }
    return result->kind;
}

[[nodiscard]]
std::u8string_view token_kind_source(const Token_Kind kind)
{
    using enum Token_Kind;
    switch (kind) {
    case binary_int:
    case block_comment:
    case block_text:
    case decimal_float:
    case decimal_int:
    case directive_splice_name:
    case document_text:
    case error:
    case escape:
    case hexadecimal_int_literal:
    case line_comment:
    case octal_int:
    case quoted_identifier:
    case quoted_string_text:
    case reserved_escape:
    case reserved_number:
    case unquoted_identifier:
    case whitespace: return {};

    case brace_left: return u8"{"sv;
    case brace_right: return u8"}"sv;
    case comma: return u8","sv;
    case ellipsis: return u8"..."sv;
    case equals: return u8"="sv;
    case false_: return u8"false"sv;
    case infinity: return u8"infinity"sv;
    case negative_infinity: return u8"-infinity"sv;
    case null: return u8"null"sv;
    case parenthesis_left: return u8"("sv;
    case parenthesis_right: return u8")"sv;
    case string_quote: return u8"\""sv;
    case true_: return u8"true"sv;
    case unit: return u8"unit"sv;
    }

    COWEL_ASSERT_UNREACHABLE(u8"Invalid token kind.");
}

[[nodiscard]]
constexpr bool token_kind_is_reserved(const Token_Kind kind)
{
    using enum Token_Kind;
    switch (kind) {
    case reserved_escape:
    case reserved_number: return true;
    default: break;
    }
    return false;
}

[[nodiscard]]
std::optional<Lex_Expectations> load_expectations(
    const std::u8string_view file, //
    std::pmr::memory_resource* const memory
)
{
    Lex_Expectations result {
        .source = std::pmr::vector<char8_t> { memory },
        .tokens = std::pmr::vector<Text_Token> { memory },
        .diagnostics = std::pmr::vector<Expected_Error> { memory },
        .success = true,
    };

    const Result<void, cowel::IO_Error_Code> r = load_utf8_file(result.source, file);
    if (!r) {
        Diagnostic_String out { memory };
        print_io_error(out, file, r.error());
        print_code_string_stdout(out);
        return {};
    }

    std::u8string_view remainder = result.source_string();
    for (std::size_t line_index = 0; !remainder.empty(); ++line_index) {
        const std::size_t line_length = ascii::length_before(remainder, '\n');
        if (line_length == 0) {
            remainder.remove_prefix(1);
            continue;
        }
        const std::u8string_view line = remainder.substr(0, line_length);
        const std::size_t instruction_length
            = ascii::length_if(line, [](char8_t c) { return is_ascii_alpha(c) || c == u8'-'; });
        if (instruction_length == 0) {
            Diagnostic_String out { memory };
            print_file_position(out, file, line_index);
            out.append(u8' ');
            out.build(Diagnostic_Highlight::error_text)
                .append(u8"Malformed line \"")
                .append(line)
                .append(u8"\".\n"sv);
            print_code_string_stdout(out);
            return {};
        }
        const std::u8string_view instruction_str = line.substr(0, instruction_length);

        const std::optional<Token_Kind> kind = token_by_name(instruction_str);
        if (!kind) {
            Diagnostic_String out { memory };
            print_file_position(out, file, line_index);
            out.append(u8' ');
            out.build(Diagnostic_Highlight::error_text)
                .append(u8"Invalid token \"")
                .append(instruction_str)
                .append(u8"\".\n"sv);
            print_code_string_stdout(out);
            return {};
        }
        if (token_kind_is_reserved(*kind)) {
            result.success = false;
        }

        const std::u8string_view argument = trim_ascii_blank(line.substr(instruction_length));
        auto token_text = [&] -> std::optional<std::pmr::u8string> {
            if (argument.empty()) {
                const std::u8string_view result = token_kind_source(*kind);
                if (result.empty()) {
                    Diagnostic_String out { memory };
                    print_file_position(out, file, line_index);
                    out.append(u8' ');
                    out.build(Diagnostic_Highlight::error_text)
                        .append(u8"Token of kind \"")
                        .append(instruction_str)
                        .append(
                            u8"\" must have explicitly specified text, but none was provided.\n"sv
                        );
                    print_code_string_stdout(out);
                    return {};
                }
                return std::pmr::u8string { result, memory };
            }
            if (argument.length() < 2 || !argument.starts_with(u8'"')
                || !argument.ends_with(u8'"')) {
                Diagnostic_String out { memory };
                print_file_position(out, file, line_index);
                out.append(u8' ');
                out.append(u8"Malformed token specification:"sv, Diagnostic_Highlight::error_text);
                out.append(u8' ');
                out.append(line, Diagnostic_Highlight::code_citation);
                out.append(u8'\n');
                print_code_string_stdout(out);
                return {};
            }

            const std::u8string_view argument_content = argument.substr(1, argument.length() - 2);
            std::pmr::u8string result { memory };
            decode_expectation_argument(result, argument_content);
            return result;
        }();
        if (!token_text) {
            return {};
        }
        result.tokens.push_back({ .kind = *kind, .text = std::move(*token_text) });

        remainder.remove_prefix(line_length);
        if (remainder.starts_with(u8'\n')) {
            remainder.remove_prefix(1);
        }
    }

    return result;
}

void append_token(Diagnostic_String& out, const Text_Token& token)
{
    out.append(token_kind_name(token.kind), Diagnostic_Highlight::tag);
    out.append(u8' ');

    const std::u8string_view source = token_kind_source(token.kind);
    const auto highlight = source.empty() || source == token.text
        ? Diagnostic_Highlight::text
        : Diagnostic_Highlight::error_text;
    auto builder = out.build(highlight);
    builder.append(u8'"');
    for (const char32_t c : utf8::Code_Point_View { token.text }) {
        if (c == u8'\\') {
            builder.append(u8"\\\\");
            continue;
        }
        if (is_ascii_printing(c)) {
            builder.append(char8_t(c));
            continue;
        }
        const auto chars = to_characters8(std::uint32_t(c), 16);
        if (c == std::uint16_t(c)) {
            builder.append(u8"\\u");
            builder.append(4 - chars.length(), u8'0');
            builder.append(chars.as_string());
        }
        else {
            builder.append(u8"\\U");
            builder.append(8 - chars.length(), u8'0');
            builder.append(chars.as_string());
        }
    }
    builder.append(u8'"');
}

void dump_tokens(
    Diagnostic_String& out,
    const std::span<const Text_Token> tokens,
    const std::u8string_view indent = {}
)
{
    for (const auto& i : tokens) {
        out.append(indent);
        append_token(out, i);
        out.append(u8'\n');
    }
}

constexpr bool print_expected_and_actual_on_failure = true;

bool run_lex_test(
    const std::u8string_view source_path, //
    const std::u8string_view expectation_path
)
{
    bool overall_success = true;
    constexpr std::u8string_view indent = u8"    ";
    std::pmr::monotonic_buffer_resource memory;

    const std::optional<Lex_Actual> actual = lex_file(source_path, &memory);
    if (!actual) {
        Diagnostic_String error;
        print_location_of_file(error, source_path);
        error.append(u8' ');
        error.append(
            u8"Test failed because input file couldn't be loaded and lexed.\n",
            Diagnostic_Highlight::error_text
        );
        print_code_string_stdout(error);
        return false;
    }

    const std::optional<Lex_Expectations> expectations
        = load_expectations(expectation_path, &memory);
    if (!expectations) {
        Diagnostic_String error;
        print_location_of_file(error, expectation_path);
        error.append(u8' ');
        error.append(
            u8"Test failed because expectations file couldn't be loaded and parsed.\n",
            Diagnostic_Highlight::error_text
        );
        print_code_string_stdout(error);
        return false;
    }

    if (actual->success != expectations->success) {
        Diagnostic_String error;
        print_location_of_file(error, expectation_path);
        error.append(u8' ');
        const auto message = actual->success
            ? u8"Test failed because lexing was expected to fail, but succeeded with no errors.\n"sv
            : u8"Test failed because lexing was expected to succeed, but failed with errors.\n"sv;
        error.append(message, Diagnostic_Highlight::error_text);
        print_code_string_stdout(error);
        overall_success = false;
    }

    if (actual->tokens != expectations->tokens) {
        Diagnostic_String error;
        print_location_of_file(error, source_path);
        error.append(u8' ');
        error.append(
            u8"Test failed because expected lexer output isn't matched.",
            Diagnostic_Highlight::error_text
        );
        if constexpr (print_expected_and_actual_on_failure) {
            error.append(u8'\n');
            error.append(u8"Expected:\n", Diagnostic_Highlight::text);
            dump_tokens(error, expectations->tokens, indent);

            error.append(u8"Actual:\n", Diagnostic_Highlight::text);
            dump_tokens(error, actual->tokens, indent);
        }
        else {
            error.append(u8' ');
        }

        Diagnostic_String expected_text;
        dump_tokens(expected_text, expectations->tokens);

        Diagnostic_String actual_text;
        dump_tokens(actual_text, actual->tokens);

        error.append(
            u8"Lexed tokens deviate from expected as follows:\n"sv, Diagnostic_Highlight::error_text
        );
        print_lines_diff(error, expected_text.get_text(), actual_text.get_text());

        print_code_string_stdout(error);
        overall_success = false;
    }

    // TODO: enable testing of emitted diagnostics

    return overall_success;
}

TEST(Lex, file_tests)
{
    constexpr auto filter = [](const fs::directory_entry& entry) -> bool {
        const fs::path& path = entry.path();
        return path.native().ends_with(".cow");
    };

    Global_Memory_Resource memory;

    std::pmr::vector<fs::path> test_paths { &memory };
    find_files_recursively(test_paths, "test/lex", filter);

    bool overall_success = true;
    for (const auto& path : test_paths) {
        const auto test_path = path.generic_u8string();
        const auto expectation_path = test_path + u8".lextest";

        if (!fs::is_regular_file(fs::path(expectation_path))) {
            continue;
        }

        if (!run_lex_test(test_path, expectation_path)) {
            overall_success = false;
        }
        else {
            Diagnostic_String out;
            print_location_of_file(out, test_path);
            out.append(u8' ');
            out.append(u8"OK", Diagnostic_Highlight::success);
            out.append(u8'\n');
            print_code_string_stdout(out);
        }
    }

    EXPECT_TRUE(overall_success);
}

} // namespace
} // namespace cowel
