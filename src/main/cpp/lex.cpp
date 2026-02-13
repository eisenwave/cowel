#include <cstddef>
#include <string_view>
#include <vector>

#include "ulight/impl/lang/cowel.hpp"

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/chars.hpp"

#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/lex.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

using ulight::Common_Number_Result;
using ulight::cowel::Comment_Result;
using ulight::cowel::Escape_Result;
using ulight::cowel::match_block_comment;
using ulight::cowel::match_ellipsis;
using ulight::cowel::match_escape;
using ulight::cowel::match_identifier;
using ulight::cowel::match_line_comment;
using ulight::cowel::match_number;
using ulight::cowel::match_whitespace;

#define COWEL_TOKEN_KIND_FIRST_CHAR(id, name, first)                                               \
    case Token_Kind::id: return u8##first;

[[nodiscard]] [[maybe_unused]]
// workaround for https://github.com/llvm/llvm-project/issues/42943
char8_t token_kind_first_char(const Token_Kind kind)
{
    switch (kind) {
        COWEL_TOKEN_KIND_ENUM_DATA(COWEL_TOKEN_KIND_FIRST_CHAR) // NOLINT(bugprone-branch-clone)
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid token kind.");
}

[[nodiscard]]
std::size_t match_reserved_number(const std::u8string_view str)
{
    if (str.empty() || (str[0] != u8'.' && !is_ascii_digit(str[0]))) {
        return 0;
    }
    std::size_t length = 1;
    while (length < str.length()) {
        const std::u8string_view remainder = str.substr(length);
        if (remainder.starts_with(u8"e+") || remainder.starts_with(u8"E+")
            || remainder.starts_with(u8"e-") || remainder.starts_with(u8"E-")) {
            length += 2;
        }
        else if (is_ascii_alphanumeric(remainder[0]) || remainder[0] == u8'.') {
            length += 1;
        }
        else {
            break;
        }
    }
    return length;
}

enum struct Content_Context : Default_Underlying {
    document,
    block,
    quoted_string,
};

struct [[nodiscard]] Lexer {
private:
    std::pmr::vector<Token>& m_out;
    const std::u8string_view m_source;
    const Lex_Error_Consumer m_on_error;

    Source_Position m_pos {};
    bool m_success = true;

public:
    [[nodiscard]]
    Lexer(std::pmr::vector<Token>& out, std::u8string_view source, Lex_Error_Consumer on_error)
        : m_out { out }
        , m_source { source }
        , m_on_error { on_error }
    {
    }

    bool operator()()
    {
        consume_document();
        return m_success;
    }

private:
    void emit(const Token_Kind kind, const std::size_t length)
    {
        if constexpr (is_debug_build) {
            COWEL_ASSERT(length == std::uint32_t(length));
            if (const char8_t expected_first = token_kind_first_char(kind)) {
                const char8_t actual_first = peek();
                COWEL_ASSERT(expected_first == actual_first);
            }
            COWEL_ASSERT(m_pos.begin + length <= m_source.length());
        }
        m_out.push_back({ kind, Source_Span { m_pos, length } });
    }

    void error(const Source_Span& pos, Char_Sequence8 message)
    {
        if (m_on_error) {
            m_on_error(diagnostic::parse, pos, message);
        }
        m_success = false;
    }

    void advance_by(std::size_t n)
    {
        COWEL_DEBUG_ASSERT(m_pos.begin + n <= m_source.size());
        advance(m_pos, m_source.substr(m_pos.begin, n));
    }

    [[nodiscard]]
    std::u8string_view peek_all() const
    {
        COWEL_DEBUG_ASSERT(m_pos.begin <= m_source.size());
        return m_source.substr(m_pos.begin);
    }

    [[nodiscard]]
    char8_t peek() const
    {
        COWEL_ASSERT(!eof());
        return m_source[m_pos.begin];
    }

    [[nodiscard]]
    bool eof() const
    {
        return m_pos.begin == m_source.length();
    }

    [[nodiscard]]
    bool peek(char8_t c) const
    {
        return !eof() && m_source[m_pos.begin] == c;
    }

    [[nodiscard]]
    bool expect(char8_t c)
    {
        if (!peek(c)) {
            return false;
        }
        advance_by(1);
        return true;
    }

    [[nodiscard]]
    bool expect_and_emit(char8_t c, Token_Kind kind)
    {
        if (!peek(c)) {
            return false;
        }
        emit(kind, 1);
        advance_by(1);
        return true;
    }

    void consume_document()
    {
        consume_markup_sequence(Content_Context::document);
    }

    void consume_markup_sequence(Content_Context context)
    {
        std::size_t brace_level = 0;
        while (expect_markup_element(context, brace_level)) { }
    }

    [[nodiscard]]
    bool expect_markup_element(Content_Context context, std::size_t& brace_level)
    {
        if (peek(u8'\\')) {
            const bool non_escape_matched //
                = expect_line_comment() //
                || expect_block_comment() //
                || expect_directive_splice();
            if (!non_escape_matched) {
                consume_escape();
            }
            return true;
        }

        const std::u8string_view remainder = peek_all();
        std::size_t text_length = 0;
        for (; text_length < remainder.length(); ++text_length) {
            const char8_t c = remainder[text_length];
            if (c == u8'\\') {
                break;
            }
            switch (context) {
            case Content_Context::document: {
                // At the document level, we don't care about brace mismatches,
                // commas, etc.
                continue;
            }
            case Content_Context::quoted_string: {
                // Within strings, braces have no special meaning,
                // but an unescaped quote ends the string.
                if (c == u8'"') {
                    goto done;
                }
                continue;
            }
            case Content_Context::block: {
                if (c == u8'{') {
                    ++brace_level;
                    break;
                }
                if (c == u8'}') {
                    if (brace_level == 0) {
                        goto done;
                    }
                    --brace_level;
                    continue;
                }
            }
            }
        }

    done:
        if (text_length == 0) {
            return false;
        }

        const auto text_kind //
            = context == Content_Context::block         ? Token_Kind::block_text
            : context == Content_Context::quoted_string ? Token_Kind::quoted_string_text
                                                        : Token_Kind::document_text;
        emit(text_kind, text_length);
        advance_by(text_length);
        return true;
    }

    void consume_escape()
    {
        const std::u8string_view remainder = peek_all();
        COWEL_ASSERT(remainder.starts_with(u8'\\'));

        const Escape_Result escape = match_escape(remainder);
        COWEL_ASSERT(escape);

        if (escape.length == 1) {
            error(Source_Span { m_pos, 1 }, u8"Backslash at the end of the file is not valid."sv);
        }

        if (escape.is_reserved) {
            error(
                Source_Span { m_pos, escape.length },
                joined_char_sequence(
                    {
                        u8"Expected comment or escape sequence, but got '"sv,
                        m_source.substr(m_pos.begin, escape.length),
                        u8"' following a backslash."sv,
                    }
                )
            );
        }

        const auto kind = escape.is_reserved ? Token_Kind::reserved_escape : Token_Kind::escape;
        emit(kind, escape.length);
        advance_by(escape.length);
    }

    [[nodiscard]]
    bool expect_whitespace()
    {
        if (const std::size_t space = match_whitespace(peek_all())) {
            emit(Token_Kind::whitespace, space);
            advance_by(space);
            return true;
        }
        return false;
    }

    [[nodiscard]]
    bool expect_line_comment()
    {
        const std::u8string_view remainder = peek_all();
        if (const std::size_t length = match_line_comment(remainder)) {
            COWEL_ASSERT(remainder.starts_with(u8"\\:"sv));
            const std::u8string_view suffix = remainder.substr(length);
            const std::size_t suffix_length = //
                suffix.starts_with(u8"\r\n"sv) ? 2
                : suffix.starts_with(u8'\n')   ? 1
                                               : 0;

            emit(Token_Kind::line_comment, length + suffix_length);
            advance_by(length + suffix_length);
            return true;
        }
        return false;
    }

    [[nodiscard]]
    bool expect_block_comment()
    {
        const std::u8string_view remainder = peek_all();
        if (const Comment_Result c = match_block_comment(remainder)) {
            COWEL_ASSERT(remainder.starts_with(u8"\\*"sv));
            if (!c.is_terminated) {
                COWEL_ASSERT(m_pos.begin + c.length == m_source.length());
                error(Source_Span { m_pos, 2 }, u8"Unterminated block comment."sv);
                advance_by(c.length);
                return true;
            }
            emit(Token_Kind::block_comment, c.length);
            advance_by(c.length);
            return true;
        }
        return false;
    }

    [[nodiscard]]
    bool expect_directive_splice()
    {
        if (!peek('\\')) {
            return {};
        }
        const std::size_t name_length = match_identifier(peek_all().substr(1));
        if (name_length == 0) {
            return false;
        }
        emit(Token_Kind::directive_splice_name, 1 + name_length);
        advance_by(1 + name_length);

        if (peek(u8'(')) {
            consume_group();
        }
        if (peek(u8'{')) {
            consume_block();
        }

        return true;
    }

    void consume_group()
    {
        COWEL_ASSERT(expect_and_emit(u8'(', Token_Kind::parenthesis_left));

        std::size_t depth = 1;
        while (!eof()) {
            switch (peek()) {
            case u8'(': {
                emit(Token_Kind::parenthesis_left, 1);
                advance_by(1);
                ++depth;
                continue;
            }
            case u8')': {
                emit(Token_Kind::parenthesis_right, 1);
                advance_by(1);
                if (--depth == 0) {
                    return;
                }
                continue;
            }
            case u8'{': {
                consume_block();
                continue;
            }
            case u8'=': {
                emit(Token_Kind::equals, 1);
                advance_by(1);
                continue;
            }
            case u8'.': {
                if (const std::size_t ellipsis = match_ellipsis(peek_all())) {
                    emit(Token_Kind::ellipsis, ellipsis);
                    advance_by(ellipsis);
                    continue;
                }
                consume_numeric_literal();
                continue;
            }
            case u8',': {
                emit(Token_Kind::comma, 1);
                advance_by(1);
                continue;
            }
            case u8'"': {
                consume_quoted_string();
                continue;
            }
            case u8'~': {
                emit(Token_Kind::bitwise_not, 1);
                advance_by(1);
                continue;
            }
            case u8'!': {
                emit(Token_Kind::logical_not, 1);
                advance_by(1);
                continue;
            }
            case u8'-': {
                emit(Token_Kind::minus, 1);
                advance_by(1);
                continue;
            }
            case u8'+': {
                emit(Token_Kind::plus, 1);
                advance_by(1);
                continue;
            }
            case u8'0':
            case u8'1':
            case u8'2':
            case u8'3':
            case u8'4':
            case u8'5':
            case u8'6':
            case u8'7':
            case u8'8':
            case u8'9': {
                consume_numeric_literal();
                continue;
            }
            case u8'\\': {
                const bool comment_matched = expect_line_comment() || expect_block_comment();
                if (!comment_matched) {
                    consume_escape();
                }
                continue;
            }
            case u8' ':
            case u8'\t':
            case u8'\r':
            case u8'\v':
            case u8'\n': {
                const bool whitespace_matched = expect_whitespace();
                COWEL_ASSERT(whitespace_matched);
                continue;
            }
            default: break;
            }
            const bool any_matched = expect_identifier_or_literal();
            if (!any_matched) {
                error(Source_Span { m_pos, 1 }, u8"Unable to form a token."sv);
                // FIXME: this should do a Unicode decode to avoid slicing code points
                emit(Token_Kind::error, 1);
                advance_by(1);
            }
        }
    }

    void consume_numeric_literal()
    {
        COWEL_DEBUG_ASSERT(peek(u8'.') || is_ascii_digit(peek()));
        const std::u8string_view remainder = peek_all();

        // Numeric limits are a subset of RESERVED-NUMBER-TOKEN.
        // Therefore, we first match the RESERVED-NUMBER-TOKEN,
        // then a more restrictive form of literal from that token.
        //
        // This is similar to the C++ preprocessor matching a pp-number first,
        // with that pp-number being turned into one or multiple tokens by the parser.
        const std::size_t reserved_length = match_reserved_number(remainder);
        COWEL_ASSERT(reserved_length);

        const Common_Number_Result result = match_number(remainder.substr(0, reserved_length));
        if (!result || result.erroneous || result.length != reserved_length) {
            error(Source_Span { m_pos, reserved_length }, u8"Invalid numeric literal."sv);
            emit(Token_Kind::reserved_number, reserved_length);
            advance_by(reserved_length);
            return;
        }

        const auto type = [&] {
            if (result.is_non_integer()) {
                return Token_Kind::decimal_float;
            }
            if (result.prefix == 0) {
                return Token_Kind::decimal_int;
            }
            COWEL_ASSERT(result.prefix == 2);
            const char8_t prefix_char = remainder[result.sign + 1];
            switch (prefix_char) {
            case u8'b': return Token_Kind::binary_int;
            case u8'o': return Token_Kind::octal_int;
            case u8'x': return Token_Kind::hexadecimal_int_literal;
            default: break;
            }
            COWEL_ASSERT_UNREACHABLE(u8"Invalid prefix.");
        }();
        emit(type, result.length);
        advance_by(result.length);
    }

    bool expect_identifier_or_literal()
    {
        const std::u8string_view remainder = peek_all();
        const std::size_t length = match_identifier(remainder);
        if (length == 0) {
            return false;
        }
        const std::u8string_view match = m_source.substr(m_pos.begin, length);

        if (match == u8"unit"sv) {
            emit(Token_Kind::unit, length);
            advance_by(length);
            return true;
        }
        if (match == u8"null"sv) {
            emit(Token_Kind::null, length);
            advance_by(length);
            return true;
        }
        if (match == u8"true"sv) {
            emit(Token_Kind::true_, length);
            advance_by(length);
            return true;
        }
        if (match == u8"false"sv) {
            emit(Token_Kind::false_, length);
            advance_by(length);
            return true;
        }
        if (match == u8"infinity"sv) {
            emit(Token_Kind::infinity, length);
            advance_by(length);
            return true;
        }

        emit(Token_Kind::identifier, length);
        advance_by(length);
        return true;
    }

    void consume_quoted_string()
    {
        const auto initial_pos = m_pos;
        COWEL_ASSERT(expect_and_emit(u8'"', Token_Kind::string_quote));

        consume_markup_sequence(Content_Context::quoted_string);

        if (!expect_and_emit(u8'"', Token_Kind::string_quote)) {
            error(
                Source_Span { initial_pos, 1 }, u8"No matching '\"'. This string is unterminated."sv
            );
        }
    }

    void consume_block()
    {
        const auto initial_pos = m_pos;
        COWEL_ASSERT(expect_and_emit(u8'{', Token_Kind::brace_left));

        consume_markup_sequence(Content_Context::block);

        if (!expect_and_emit(u8'}', Token_Kind::brace_right)) {
            error(Source_Span { initial_pos, 1 }, u8"No matching '}'. This block is unclosed."sv);
        }
    }
};

} // namespace

bool lex(std::pmr::vector<Token>& out, std::u8string_view source, Lex_Error_Consumer on_error)
{
    return Lexer { out, source, on_error }();
}

} // namespace cowel
