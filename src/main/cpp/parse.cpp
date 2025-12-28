#include <cstddef>
#include <string_view>
#include <vector>

#include "ulight/impl/ascii_algorithm.hpp"
#include "ulight/impl/lang/cowel.hpp"

#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"

#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/parse.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

enum struct Content_Context : Default_Underlying {
    document,
    block,
    string,
};

struct [[nodiscard]] Parser {
private:
    struct [[nodiscard]] Scoped_Attempt {
    private:
        Parser* m_self;
        const Source_Position m_initial_pos;
        const std::size_t m_initial_size;

    public:
        Scoped_Attempt(Parser& self)
            : m_self { &self }
            , m_initial_pos { self.m_pos }
            , m_initial_size { self.m_out.size() }
        {
        }

        Scoped_Attempt(const Scoped_Attempt&) = delete;
        Scoped_Attempt& operator=(const Scoped_Attempt&) = delete;

        void commit()
        {
            COWEL_ASSERT(m_self);
            m_self = nullptr;
        }

        void abort()
        {
            COWEL_ASSERT(m_self);
            COWEL_ASSERT(m_self->m_out.size() >= m_initial_size);

            m_self->m_pos = m_initial_pos;
            m_self->m_out.resize(m_initial_size);

            m_self = nullptr;
        }

        ~Scoped_Attempt() // NOLINT(bugprone-exception-escape)
        {
            if (m_self) {
                abort();
            }
        }
    };

    std::pmr::vector<AST_Instruction>& m_out;
    const std::u8string_view m_source;
    const Parse_Error_Consumer m_on_error;

    Source_Position m_pos {};
    bool m_success = true;

public:
    [[nodiscard]]
    Parser(
        std::pmr::vector<AST_Instruction>& out,
        std::u8string_view source,
        Parse_Error_Consumer on_error
    )
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

    Scoped_Attempt attempt()
    {
        return Scoped_Attempt { *this };
    }

    /// @brief Returns all remaining text as a `std::string_view_type`, from the current parsing
    /// position to the end of the file.
    /// @return All remaining text.
    [[nodiscard]]
    std::u8string_view peek_all() const
    {
        COWEL_DEBUG_ASSERT(m_pos.begin <= m_source.size());
        return m_source.substr(m_pos.begin);
    }

    /// @brief Returns the next character and advances the parser position.
    /// @return The popped character.
    /// @throws Throws if `eof()`.
    char8_t pop()
    {
        const char8_t c = peek();
        advance_by(1);
        return c;
    }

    /// @brief Returns the next character.
    /// @return The next character.
    /// @throws Throws if `eof()`.
    [[nodiscard]]
    char8_t peek() const
    {
        COWEL_ASSERT(!eof());
        return m_source[m_pos.begin];
    }

    /// @return `true` if the parser is at the end of the file, `false` otherwise.
    [[nodiscard]]
    bool eof() const
    {
        return m_pos.begin == m_source.length();
    }

    /// @return `peek_all().starts_with(text)`.
    [[nodiscard]]
    bool peek(std::u8string_view text) const
    {
        return peek_all().starts_with(text);
    }

    /// @brief Checks whether the next character matches an expected value without advancing
    /// the parser.
    /// @param c the character to test
    /// @return `true` if the next character equals `c`, `false` otherwise.
    [[nodiscard]]
    bool peek(char8_t c) const
    {
        return !eof() && m_source[m_pos.begin] == c;
    }

    /// @brief Checks whether the next character satisfies a predicate without advancing
    /// the parser.
    /// @param predicate the predicate to test
    /// @return `true` if the next character satisfies `predicate`, `false` otherwise.
    bool peek(bool predicate(char8_t)) const
    {
        return !eof() && predicate(m_source[m_pos.begin]);
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
    bool expect(std::u8string_view text)
    {
        if (!peek(text)) {
            return false;
        }
        advance_by(text.size());
        return true;
    }

    [[nodiscard]]
    bool expect(bool predicate(char8_t))
    {
        if (eof()) {
            return false;
        }
        const char8_t c = m_source[m_pos.begin];
        if (!predicate(c)) {
            return false;
        }
        // This function is only safe to call when we have expectations towards ASCII characters.
        // Any non-ASCII character should have already been rejected.
        COWEL_ASSERT(is_ascii(c));
        advance_by(1);
        return true;
    }

    void consume_document()
    {
        const std::size_t document_instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type::push_document, 0 });
        const std::size_t content_amount = match_markup_sequence(Content_Context::document);
        m_out[document_instruction_index].n = content_amount;
        m_out.push_back({ AST_Instruction_Type::pop_document, 0 });
    }

    [[nodiscard]]
    std::size_t match_markup_sequence(Content_Context context)
    {
        Bracket_Levels levels {};
        std::size_t elements = 0;

        while (try_match_markup_element(context, levels)) {
            ++elements;
        }

        return elements;
    }

    struct Bracket_Levels {
        std::size_t argument = 0;
        std::size_t brace = 0;
    };

    /// @brief Attempts to match the next piece of content,
    /// which is an escape sequence, directive, or plaintext.
    ///
    /// Returns `false` if none of these could be matched.
    /// This may happen because the parser is located at e.g. a `}` and the given `context`
    /// is terminated by `}`.
    /// It may also happen if the parser has already reached the EOF.
    [[nodiscard]]
    bool try_match_markup_element(Content_Context context, Bracket_Levels& levels)
    {
        if (peek(u8'\\')) {
            const bool non_text_matched //
                = try_match_escape() //
                || try_match_line_comment() //
                || try_match_block_comment() //
                || try_match_directive();
            if (non_text_matched) {
                return true;
            }
        }

        const std::size_t initial_pos = m_pos.begin;

        for (; !eof(); advance_by(1)) {
            const char8_t c = peek();
            if (c == u8'\\') {
                const std::u8string_view remainder { m_source.substr(m_pos.begin + 1) };

                // Trailing \ at the end of the file.
                // No need to break, we'll just run into it next iteration.
                if (remainder.empty()) {
                    continue;
                }
                if (is_cowel_allowed_after_backslash(remainder.front())) {
                    break;
                }
                continue;
            }
            switch (context) {
            case Content_Context::document: {
                // At the document level, we don't care about brace mismatches,
                // commas, etc.
                continue;
            }
            case Content_Context::string: {
                // Within strings, braces have no special meaning,
                // but an unescaped quote ends the string.
                if (c == u8'"') {
                    goto done;
                }
                continue;
            }
            case Content_Context::block: {
                if (c == u8'{') {
                    ++levels.brace;
                    break;
                }
                if (c == u8'}') {
                    if (levels.brace == 0) {
                        goto done;
                    }
                    --levels.brace;
                    continue;
                }
            }
            }
        }

    done:
        COWEL_ASSERT(m_pos.begin >= initial_pos);
        if (m_pos.begin == initial_pos) {
            return false;
        }

        m_out.push_back({ AST_Instruction_Type::text, m_pos.begin - initial_pos });
        return true;
    }

    [[nodiscard]]
    bool try_match_escape()
    {
        const std::u8string_view remainder = peek_all();
        if (const std::size_t length = ulight::cowel::match_escape(remainder)) {
            advance_by(length);
            m_out.push_back({ AST_Instruction_Type::escape, length });
            return true;
        }
        return false;
    }

    [[nodiscard]]
    bool try_match_line_comment()
    {
        const std::u8string_view remainder = peek_all();
        if (const std::size_t length = ulight::cowel::match_line_comment(remainder)) {
            COWEL_ASSERT(remainder.starts_with(u8"\\:"sv));
            const std::u8string_view suffix = remainder.substr(length);
            const std::size_t suffix_length = //
                suffix.starts_with(u8"\r\n"sv) ? 2
                : suffix.starts_with(u8'\n')   ? 1
                                               : 0;

            advance_by(length + suffix_length);
            m_out.push_back({ AST_Instruction_Type::line_comment, length + suffix_length });
            return true;
        }
        return false;
    }

    [[nodiscard]]
    bool try_match_block_comment()
    {
        const std::u8string_view remainder = peek_all();
        if (const ulight::cowel::Comment_Result c = ulight::cowel::match_block_comment(remainder)) {
            COWEL_ASSERT(remainder.starts_with(u8"\\*"sv));
            if (!c.is_terminated) {
                COWEL_ASSERT(m_pos.begin + c.length == m_source.length());
                error(Source_Span { m_pos, 2 }, u8"Unterminated block comment."sv);
                advance_by(c.length);
                return true;
            }
            advance_by(c.length);
            m_out.push_back({ AST_Instruction_Type::block_comment, c.length });
            return true;
        }
        return false;
    }

    [[nodiscard]]
    bool try_match_directive()
    {
        Scoped_Attempt a = attempt();

        if (!expect('\\')) {
            return {};
        }
        const std::size_t name_length = ulight::cowel::match_directive_name(peek_all());
        if (name_length == 0) {
            return false;
        }
        advance_by(name_length);

        m_out.push_back({ AST_Instruction_Type::push_directive, name_length + 1 });

        if (peek(u8'(')) {
            consume_group();
        }
        if (peek(u8'{')) {
            consume_block();
        }

        m_out.push_back({ AST_Instruction_Type::pop_directive });

        a.commit();
        return true;
    }

    bool consume_group()
    {
        COWEL_ASSERT(expect(u8'('));

        const std::size_t instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type::push_group, 0 });

        std::size_t member_count = 0;
        while (!eof()) {
            skip_blank();
            if (expect(u8')')) {
                m_out[instruction_index].n = member_count;
                m_out.push_back({ AST_Instruction_Type::pop_group });
                return true;
            }
            if (expect(u8',')) {
                m_out.push_back({ AST_Instruction_Type::member_comma });
                continue;
            }
            consume_group_member();
            ++member_count;
        }

        return false;
    }

    bool consume_group_member()
    {
        if (eof()) {
            return false;
        }

        const std::size_t argument_instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type {}, 0 });

        skip_blank();

        AST_Instruction_Type push_type;
        AST_Instruction_Type pop_type;

        const bool is_named = try_match_member_name();
        if (is_named) {
            // TODO: This seems to be a historical artifact.
            //       This could probably just be skip_blank().
            //       If not, comment why this skips only whitespace and no comments.
            const std::size_t leading_whitespace = ulight::cowel::match_whitespace(peek_all());
            if (leading_whitespace != 0) {
                m_out.push_back({ AST_Instruction_Type::skip, leading_whitespace });
                advance_by(leading_whitespace);
            }
            push_type = AST_Instruction_Type::push_named_member;
            pop_type = AST_Instruction_Type::pop_named_member;
        }
        else if (try_match_ellipsis()) {
            push_type = AST_Instruction_Type::push_ellipsis_argument;
            pop_type = AST_Instruction_Type::pop_ellipsis_argument;
        }
        else {
            push_type = AST_Instruction_Type::push_positional_member;
            pop_type = AST_Instruction_Type::pop_positional_member;
        }

        skip_blank();

        if (push_type != AST_Instruction_Type::push_ellipsis_argument) {
            if (!consume_member_value()) {
                return false;
            }
            skip_blank();
        }

        if (!peek(u8',') && !peek(')')) {
            const auto initial_pos = m_pos;
            const std::size_t error_length = consume_error_until_one_of(u8",)");
            error(Source_Span { initial_pos, error_length }, u8"Invalid group member."sv);
            return false;
        }

        m_out[argument_instruction_index].type = push_type;
        m_out.push_back({ pop_type });

        return true;
    }

    [[nodiscard]]
    bool try_match_ellipsis()
    {
        if (const std::size_t ellipsis = ulight::cowel::match_ellipsis(peek_all())) {
            advance_by(ellipsis);
            m_out.push_back({ AST_Instruction_Type::ellipsis, ellipsis });
            return true;
        }
        return false;
    }

    /// @brief Matches the name of an argument, including any surrounding whitespace and the `=`
    /// character following it.
    /// If the argument couldn't be matched, returns `false` and keeps the parser state unchanged.
    [[nodiscard]]
    bool try_match_member_name()
    {
        Scoped_Attempt a = attempt();

        if (eof()) {
            return false;
        }

        const std::size_t name_length = ulight::cowel::match_argument_name(peek_all());
        m_out.push_back({ AST_Instruction_Type::member_name, name_length });
        if (name_length == 0) {
            return false;
        }
        advance_by(name_length);

        const std::size_t trailing_whitespace = ulight::cowel::match_whitespace(peek_all());
        if (trailing_whitespace != 0) {
            m_out.push_back({ AST_Instruction_Type::skip, trailing_whitespace });
            advance_by(trailing_whitespace);
        }
        if (eof()) {
            return false;
        }

        if (!expect(u8'=')) {
            return false;
        }

        m_out.push_back({ AST_Instruction_Type::member_equal });
        a.commit();
        return true;
    }

    bool consume_member_value()
    {
        COWEL_ASSERT(!eof());

        const std::u8string_view remainder = peek_all();

        switch (remainder[0]) {
        case u8'"': {
            return consume_quoted_string();
        }
        case u8'(': {
            return consume_group();
        }
        case u8'{': {
            return consume_block();
        }
        default: {
            return try_match_directive_call() //
                || try_match_numeric_literal() //
                || consume_unquoted_value();
        }
        }
    }

    bool try_match_directive_call()
    {
        const std::u8string_view remainder = peek_all();
        const std::size_t name_length = ulight::cowel::match_directive_name(remainder);
        if (name_length == 0) {
            return false;
        }

        Scoped_Attempt a = attempt();
        advance_by(name_length);
        m_out.push_back({ AST_Instruction_Type::push_directive, name_length });

        skip_blank();
        const bool has_group = peek(u8'(') && consume_group();

        skip_blank();
        const bool has_block = peek(u8'{') && consume_block();

        if (!has_group && !has_block) {
            return false;
        }

        m_out.push_back({ AST_Instruction_Type::pop_directive });

        a.commit();
        return true;
    }

    bool try_match_numeric_literal()
    {
        const std::u8string_view remainder = peek_all();
        if (remainder.empty()) {
            return false;
        }

        const ulight::Common_Number_Result result = ulight::cowel::match_number(remainder);
        if (!result || result.erroneous
            || (result.length < remainder.length()
                && ulight::is_cowel_unquoted_string(remainder[result.length]))) {
            return false;
        }

        const auto type = [&] {
            if (result.is_non_integer()) {
                return AST_Instruction_Type::float_literal;
            }
            if (result.prefix == 0) {
                return AST_Instruction_Type::decimal_int_literal;
            }
            COWEL_ASSERT(result.prefix == 2);
            const char8_t prefix_char = remainder[result.sign + 1];
            switch (prefix_char) {
            case u8'b': return AST_Instruction_Type::binary_int_literal;
            case u8'o': return AST_Instruction_Type::octal_int_literal;
            case u8'x': return AST_Instruction_Type::hexadecimal_int_literal;
            default: break;
            }
            COWEL_ASSERT_UNREACHABLE(u8"Invalid prefix.");
        }();
        advance_by(result.length);
        m_out.push_back({ type, result.length });
        return true;
    }

    bool consume_unquoted_value()
    {
        const std::u8string_view remainder = peek_all();
        const std::size_t length = ulight::ascii::length_if(remainder, [](char8_t c) {
            return ulight::is_cowel_unquoted_string(c);
        });

        if (length == 0) {
            const auto initial_pos = m_pos;
            const std::size_t error_length = consume_error_until_one_of(u8",)"sv);
            error(Source_Span { initial_pos, error_length }, u8"Invalid member value."sv);
            return false;
        }
        const std::u8string_view match = m_source.substr(m_pos.begin, length);

        if (match == u8"unit"sv) {
            advance_by(length);
            m_out.push_back({ AST_Instruction_Type::keyword_unit, length });
            return true;
        }
        if (match == u8"null"sv) {
            advance_by(length);
            m_out.push_back({ AST_Instruction_Type::keyword_null, length });
            return true;
        }
        if (match == u8"true"sv) {
            advance_by(length);
            m_out.push_back({ AST_Instruction_Type::keyword_true, length });
            return true;
        }
        if (match == u8"false"sv) {
            advance_by(length);
            m_out.push_back({ AST_Instruction_Type::keyword_false, length });
            return true;
        }
        if (match == u8"infinity"sv) {
            advance_by(length);
            m_out.push_back({ AST_Instruction_Type::keyword_infinity, length });
            return true;
        }
        if (match == u8"-infinity"sv) {
            advance_by(length);
            m_out.push_back({ AST_Instruction_Type::keyword_neg_infinity, length });
            return true;
        }

        advance_by(length);
        m_out.push_back({ AST_Instruction_Type::unquoted_string, length });
        return true;
    }

    bool consume_quoted_string()
    {
        const auto initial_pos = m_pos;
        COWEL_ASSERT(expect(u8'"'));

        const std::size_t instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type::push_quoted_string });

        const std::size_t elements = match_markup_sequence(Content_Context::string);

        if (!expect(u8'"')) {
            error(
                Source_Span { initial_pos, 1 }, u8"No matching '\"'. This string is unterminated."sv
            );
            return false;
        }

        m_out.push_back({ AST_Instruction_Type::pop_quoted_string });
        m_out[instruction_index].n = elements;
        return true;
    }

    std::size_t skip_blank()
    {
        const std::size_t start = m_pos.begin;

        while (true) {
            const std::size_t white_length = ulight::cowel::match_whitespace(peek_all());
            advance_by(white_length);

            const std::size_t comment_length = ulight::cowel::match_line_comment(peek_all());
            if (comment_length) {
                advance_by(comment_length);
                continue;
            }

            const ulight::cowel::Comment_Result c = ulight::cowel::match_block_comment(peek_all());
            if (c) {
                if (!c.is_terminated) {
                    COWEL_ASSERT(m_pos.begin + c.length == m_source.length());
                    error(Source_Span { m_pos, 2 }, u8"Unterminated block comment."sv);
                    advance_by(c.length);
                }
                else {
                    advance_by(c.length);
                    continue;
                }
            }
            break;
        }

        const std::size_t skip_length = m_pos.begin - start;
        if (skip_length) {
            m_out.push_back({ AST_Instruction_Type::skip, skip_length });
        }

        return skip_length;
    }

    bool consume_block()
    {
        const auto initial_pos = m_pos;
        COWEL_ASSERT(expect(u8'{'));

        const std::size_t instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type::push_block });

        const std::size_t elements = match_markup_sequence(Content_Context::block);

        if (!expect(u8'}')) {
            error(Source_Span { initial_pos, 1 }, u8"No matching '}'. This block is unclosed."sv);
            return false;
        }
        m_out[instruction_index].n = elements;
        m_out.push_back({ AST_Instruction_Type::pop_block });
        return true;
    }

    [[nodiscard]]
    std::size_t consume_error_until_one_of(std::u8string_view set)
    {
        COWEL_ASSERT(!set.contains(u8'\\'));

        const std::size_t initial_begin = m_pos.begin;
        while (!eof()) {
            const std::size_t skip_length
                = ulight::ascii::length_if_not(peek_all(), [&](char8_t c) {
                      return c == u8'\\' || set.contains(c);
                  });
            advance_by(skip_length);

            if (!peek(u8'\\')) {
                break;
            }

            const std::size_t line_comment_length = ulight::cowel::match_line_comment(peek_all());
            if (line_comment_length) {
                advance_by(line_comment_length);
                continue;
            }

            const ulight::cowel::Comment_Result block_comment
                = ulight::cowel::match_block_comment(peek_all());
            if (block_comment) {
                if (!block_comment.is_terminated) {
                    error(Source_Span { m_pos, 2 }, u8"Unterminated block comment."sv);
                }
                advance_by(block_comment.length);
                continue;
            }

            // It is possible that we have matched a backslash but did not encounter a
            // comment, in which case we simply skip the backslash.
            advance_by(1);
        }

        return m_pos.begin - initial_begin;
    }
};

} // namespace

std::u8string_view ast_instruction_type_name(AST_Instruction_Type type)
{
    using enum AST_Instruction_Type;
    switch (type) {
        COWEL_ENUM_STRING_CASE8(skip);
        COWEL_ENUM_STRING_CASE8(escape);
        COWEL_ENUM_STRING_CASE8(text);
        COWEL_ENUM_STRING_CASE8(unquoted_string);
        COWEL_ENUM_STRING_CASE8(binary_int_literal);
        COWEL_ENUM_STRING_CASE8(octal_int_literal);
        COWEL_ENUM_STRING_CASE8(decimal_int_literal);
        COWEL_ENUM_STRING_CASE8(hexadecimal_int_literal);
        COWEL_ENUM_STRING_CASE8(float_literal);
        COWEL_ENUM_STRING_CASE8(keyword_true);
        COWEL_ENUM_STRING_CASE8(keyword_false);
        COWEL_ENUM_STRING_CASE8(keyword_null);
        COWEL_ENUM_STRING_CASE8(keyword_unit);
        COWEL_ENUM_STRING_CASE8(keyword_infinity);
        COWEL_ENUM_STRING_CASE8(keyword_neg_infinity);
        COWEL_ENUM_STRING_CASE8(line_comment);
        COWEL_ENUM_STRING_CASE8(block_comment);
        COWEL_ENUM_STRING_CASE8(member_name);
        COWEL_ENUM_STRING_CASE8(ellipsis);
        COWEL_ENUM_STRING_CASE8(member_equal);
        COWEL_ENUM_STRING_CASE8(member_comma);
        COWEL_ENUM_STRING_CASE8(push_document);
        COWEL_ENUM_STRING_CASE8(pop_document);
        COWEL_ENUM_STRING_CASE8(push_directive);
        COWEL_ENUM_STRING_CASE8(pop_directive);
        COWEL_ENUM_STRING_CASE8(push_group);
        COWEL_ENUM_STRING_CASE8(pop_group);
        COWEL_ENUM_STRING_CASE8(push_named_member);
        COWEL_ENUM_STRING_CASE8(pop_named_member);
        COWEL_ENUM_STRING_CASE8(push_positional_member);
        COWEL_ENUM_STRING_CASE8(pop_positional_member);
        COWEL_ENUM_STRING_CASE8(push_ellipsis_argument);
        COWEL_ENUM_STRING_CASE8(pop_ellipsis_argument);
        COWEL_ENUM_STRING_CASE8(push_block);
        COWEL_ENUM_STRING_CASE8(pop_block);
        COWEL_ENUM_STRING_CASE8(push_quoted_string);
        COWEL_ENUM_STRING_CASE8(pop_quoted_string);
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid type.");
}

bool parse(
    std::pmr::vector<AST_Instruction>& out,
    std::u8string_view source,
    Parse_Error_Consumer on_error
)
{
    return Parser { out, source, on_error }();
}

} // namespace cowel
