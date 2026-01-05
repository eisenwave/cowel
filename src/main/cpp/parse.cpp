#include <cstddef>
#include <string_view>
#include <vector>

#include "cowel/util/assert.hpp"

#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/lex.hpp"
#include "cowel/parse.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

enum struct Content_Context : Default_Underlying {
    document,
    block,
    quoted_string,
};

struct [[nodiscard]] Parser {
private:
    struct [[nodiscard]] Scoped_Attempt {
    private:
        Parser* m_self;
        std::size_t m_initial_pos;
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

    std::pmr::vector<CST_Instruction>& m_out;
    std::span<const Token> m_tokens;
    const Parse_Error_Consumer m_on_error;

    std::size_t m_pos = 0;
    bool m_success = true;

public:
    [[nodiscard]]
    Parser(
        std::pmr::vector<CST_Instruction>& out,
        std::span<const Token> tokens,
        Parse_Error_Consumer on_error
    )
        : m_out { out }
        , m_tokens { tokens }
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
        COWEL_DEBUG_ASSERT(m_pos + n <= m_tokens.size());
        m_pos += n;
    }

    Scoped_Attempt attempt()
    {
        return Scoped_Attempt { *this };
    }

    /// @return `true` if the parser is at the end of the file, `false` otherwise.
    [[nodiscard]]
    bool eof() const
    {
        return m_pos >= m_tokens.size();
    }

    [[nodiscard]]
    const Token* peek() const
    {
        if (eof()) {
            return {};
        }
        return &m_tokens[m_pos];
    }

    /// @brief Checks whether the next character matches an expected value without advancing
    /// the parser.
    /// @param c the character to test
    /// @return `true` if the next character equals `c`, `false` otherwise.
    [[nodiscard]]
    const Token* peek(Token_Kind kind) const
    {
        if (const Token* const next = peek()) {
            if (next->kind == kind) {
                return next;
            }
        }
        return nullptr;
    }

    [[nodiscard]]
    const Token* expect(Token_Kind kind)
    {
        if (const Token* const next = peek(kind)) {
            advance_by(1);
            return next;
        }
        return nullptr;
    }

    void emit_and_advance_by_one(const CST_Instruction_Kind kind)
    {
        if constexpr (is_debug_build) {
            const auto expected_token = cst_instruction_kind_fixed_token(kind);
            COWEL_ASSERT(
                expected_token == Token_Kind::error || expected_token == m_tokens[m_pos].kind
            );
        }
        m_out.push_back({ kind });
        advance_by(1);
    }

    void consume_document()
    {
        const std::size_t document_instruction_index = m_out.size();
        m_out.push_back({ CST_Instruction_Kind::push_document, 0 });
        const std::size_t content_amount = consume_markup_sequence(Content_Context::document);
        m_out[document_instruction_index].n = content_amount;
        m_out.push_back({ CST_Instruction_Kind::pop_document });
    }

    [[nodiscard]]
    std::size_t consume_markup_sequence(Content_Context context)
    {
        std::size_t elements = 0;
        while (expect_markup_element(context)) {
            ++elements;
        }
        return elements;
    }

    [[nodiscard]]
    bool expect_markup_element(const Content_Context context)
    {
        const Token* const next = peek();
        if (!next) {
            return false;
        }
        switch (next->kind) {
        case Token_Kind::document_text: {
            COWEL_DEBUG_ASSERT(context == Content_Context::document);
            emit_and_advance_by_one(CST_Instruction_Kind::text);
            return true;
        }
        case Token_Kind::block_text: {
            COWEL_DEBUG_ASSERT(context == Content_Context::block);
            emit_and_advance_by_one(CST_Instruction_Kind::text);
            return true;
        }
        case Token_Kind::quoted_string_text: {
            COWEL_DEBUG_ASSERT(context == Content_Context::quoted_string);
            emit_and_advance_by_one(CST_Instruction_Kind::text);
            return true;
        }
        case Token_Kind::escape: {
            emit_and_advance_by_one(CST_Instruction_Kind::escape);
            return true;
        }
        case Token_Kind::line_comment: {
            emit_and_advance_by_one(CST_Instruction_Kind::line_comment);
            return true;
        }
        case Token_Kind::block_comment: {
            emit_and_advance_by_one(CST_Instruction_Kind::block_comment);
            return true;
        }
        case Token_Kind::directive_splice_name: {
            consume_directive_splice();
            return true;
        }
        case Token_Kind::brace_right: {
            COWEL_DEBUG_ASSERT(context == Content_Context::block);
            return false;
        }
        case Token_Kind::string_quote: {
            COWEL_DEBUG_ASSERT(context == Content_Context::quoted_string);
            return false;
        }
        default: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Unexpected token in markup sequence.");
    }

    void consume_directive_splice()
    {
        const Token* const splice_name = expect(Token_Kind::directive_splice_name);
        COWEL_ASSERT(splice_name);

        m_out.push_back({ CST_Instruction_Kind::push_directive_splice });

        if (peek(Token_Kind::parenthesis_left)) {
            consume_group();
        }
        if (peek(Token_Kind::brace_left)) {
            consume_block();
        }
        m_out.push_back({ CST_Instruction_Kind::pop_directive_splice });
    }

    void consume_group()
    {
        COWEL_ASSERT(expect(Token_Kind::parenthesis_left));

        const std::size_t instruction_index = m_out.size();
        m_out.push_back({ CST_Instruction_Kind::push_group, 0 });

        std::size_t member_count = 0;
        while (!eof()) {
            consume_blank_sequence();
            if (expect(Token_Kind::parenthesis_right)) {
                m_out.push_back({ CST_Instruction_Kind::pop_group });
                m_out[instruction_index].n = member_count;
                return;
            }
            if (expect(Token_Kind::comma)) {
                m_out.push_back({ CST_Instruction_Kind::comma });
                continue;
            }
            consume_group_member();
            ++member_count;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Unterminated group should have been dealt with by lexer.");
    }

    void consume_group_member()
    {
        COWEL_ASSERT(!eof());

        const std::size_t argument_instruction_index = m_out.size();
        m_out.push_back({ CST_Instruction_Kind {}, 0 });

        consume_blank_sequence();

        CST_Instruction_Kind push_type;
        CST_Instruction_Kind pop_type;

        const bool is_named = expect_member_name();
        if (is_named) {
            consume_blank_sequence();
            push_type = CST_Instruction_Kind::push_named_member;
            pop_type = CST_Instruction_Kind::pop_named_member;
        }
        else if (expect(Token_Kind::ellipsis)) {
            m_out.push_back({ CST_Instruction_Kind::ellipsis });
            push_type = CST_Instruction_Kind::push_ellipsis_argument;
            pop_type = CST_Instruction_Kind::pop_ellipsis_argument;
        }
        else {
            push_type = CST_Instruction_Kind::push_positional_member;
            pop_type = CST_Instruction_Kind::pop_positional_member;
        }

        consume_blank_sequence();

        if (push_type != CST_Instruction_Kind::push_ellipsis_argument) {
            if (!expect_member_value()) {
                error(m_tokens[m_pos].location, u8"Invalid group member value."sv);
                skip_to_next_group_member();
                return;
            }
            consume_blank_sequence();
        }

        if (!peek(Token_Kind::comma) && !peek(Token_Kind::parenthesis_right)) {
            error(m_tokens[m_pos].location, u8"Invalid group member."sv);
            skip_to_next_group_member();
            return;
        }

        m_out[argument_instruction_index].kind = push_type;
        m_out.push_back({ pop_type });
    }

    void skip_to_next_group_member()
    {
        const std::size_t initial_pos = m_pos;
        while (true) {
            const Token* const next = peek();
            if (!next) {
                break;
            }
            switch (next->kind) {
            case Token_Kind::comma:
            case Token_Kind::parenthesis_right: {
                goto done;
            }
            case Token_Kind::parenthesis_left: {
                consume_group();
                continue;
            }
            case Token_Kind::brace_left: {
                consume_block();
                continue;
            }
            default: {
                continue;
            }
            }
        }
    done:
        // If we haven't made any progress,
        // this gets us stuck in an infinite loop of failing to parse.
        COWEL_ASSERT(m_pos != initial_pos);
    }

    /// @brief Matches the name of an argument, including any surrounding whitespace and the `=`
    /// character following it.
    /// If the argument couldn't be matched, returns `false` and keeps the parser state unchanged.
    [[nodiscard]]
    bool expect_member_name()
    {
        const Token* const next = peek();
        if (!next) {
            return false;
        }
        if (next->kind != Token_Kind::unquoted_identifier
            && next->kind != Token_Kind::quoted_identifier) {
            return false;
        }
        Scoped_Attempt a = attempt();
        emit_and_advance_by_one(CST_Instruction_Kind::member_name);
        consume_blank_sequence();

        if (expect(Token_Kind::equals)) {
            m_out.push_back({ CST_Instruction_Kind::equals });
            consume_blank_sequence();
            a.commit();
            return true;
        }
        return false;
    }

    bool expect_member_value()
    {
        const Token* const next = peek();
        if (!next) {
            return false;
        }

        switch (next->kind) {
        case Token_Kind::string_quote: {
            consume_quoted_string();
            return true;
        }
        case Token_Kind::parenthesis_left: {
            consume_group();
            return true;
        }
        case Token_Kind::brace_left: {
            consume_block();
            return true;
        }
        case Token_Kind::unit: {
            emit_and_advance_by_one(CST_Instruction_Kind::keyword_unit);
            return true;
        }
        case Token_Kind::null: {
            emit_and_advance_by_one(CST_Instruction_Kind::keyword_null);
            return true;
        }
        case Token_Kind::true_: {
            emit_and_advance_by_one(CST_Instruction_Kind::keyword_true);
            return true;
        }
        case Token_Kind::false_: {
            emit_and_advance_by_one(CST_Instruction_Kind::keyword_false);
            return true;
        }
        case Token_Kind::infinity: {
            emit_and_advance_by_one(CST_Instruction_Kind::keyword_infinity);
            return true;
        }
        case Token_Kind::negative_infinity: {
            emit_and_advance_by_one(CST_Instruction_Kind::keyword_neg_infinity);
            return true;
        }
        case Token_Kind::binary_int: {
            emit_and_advance_by_one(CST_Instruction_Kind::binary_int);
            return true;
        }
        case Token_Kind::octal_int: {
            emit_and_advance_by_one(CST_Instruction_Kind::octal_int);
            return true;
        }
        case Token_Kind::decimal_int: {
            emit_and_advance_by_one(CST_Instruction_Kind::decimal_int);
            return true;
        }
        case Token_Kind::hexadecimal_int_literal: {
            emit_and_advance_by_one(CST_Instruction_Kind::hexadecimal_int);
            return true;
        }
        case Token_Kind::decimal_float: {
            emit_and_advance_by_one(CST_Instruction_Kind::decimal_float);
            return true;
        }
        case Token_Kind::unquoted_identifier: {
            if (expect_directive_call()) {
                return true;
            }
            emit_and_advance_by_one(CST_Instruction_Kind::unquoted_string);
            return true;
        }
        case Token_Kind::comma:
        case Token_Kind::ellipsis:
        case Token_Kind::equals:
        case Token_Kind::parenthesis_right:
        case Token_Kind::brace_right: return false;

        case Token_Kind::directive_splice_name:
        case Token_Kind::document_text:
        case Token_Kind::quoted_identifier:
        case Token_Kind::quoted_string_text:
        case Token_Kind::block_text:
        case Token_Kind::error:
        case Token_Kind::escape:
        case Token_Kind::reserved_escape:
        case Token_Kind::reserved_number:
        case Token_Kind::whitespace:
        case Token_Kind::block_comment:
        case Token_Kind::line_comment: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Unexpected token in group.");
    }

    bool expect_directive_call()
    {
        Scoped_Attempt a = attempt();

        const Token* const next = expect(Token_Kind::unquoted_identifier);
        if (!next) {
            return false;
        }

        m_out.push_back({ CST_Instruction_Kind::push_directive_call });

        consume_blank_sequence();
        bool has_group = false;
        if (peek(Token_Kind::parenthesis_left)) {
            has_group = true;
            consume_group();
        }

        consume_blank_sequence();
        bool has_block = false;
        if (peek(Token_Kind::brace_left)) {
            has_block = true;
            consume_block();
        }

        if (!has_group && !has_block) {
            return false;
        }

        m_out.push_back({ CST_Instruction_Kind::pop_directive_call });

        a.commit();
        return true;
    }

    void consume_quoted_string()
    {
        COWEL_ASSERT(expect(Token_Kind::string_quote));

        const std::size_t instruction_index = m_out.size();
        m_out.push_back({ CST_Instruction_Kind::push_quoted_string });

        const std::size_t elements = consume_markup_sequence(Content_Context::quoted_string);
        const bool is_closed = expect(Token_Kind::string_quote);
        COWEL_ASSERT(is_closed);

        m_out.push_back({ CST_Instruction_Kind::pop_quoted_string });
        m_out[instruction_index].n = elements;
    }

    void consume_block()
    {
        COWEL_ASSERT(expect(Token_Kind::brace_left));

        const std::size_t instruction_index = m_out.size();
        m_out.push_back({ CST_Instruction_Kind::push_block });

        const std::size_t elements = consume_markup_sequence(Content_Context::block);

        const bool is_closed = expect(Token_Kind::brace_right);
        COWEL_ASSERT(is_closed);

        m_out[instruction_index].n = elements;
        m_out.push_back({ CST_Instruction_Kind::pop_block });
    }

    void consume_blank_sequence()
    {
        constexpr auto is_blank = [](Token_Kind kind) {
            return kind == Token_Kind::whitespace //
                || kind == Token_Kind::line_comment //
                || kind == Token_Kind::block_comment;
        };

        while (true) {
            const Token* const next = peek();
            if (!next || !is_blank(next->kind)) {
                break;
            }
            emit_and_advance_by_one(CST_Instruction_Kind::skip);
        }
    }
};

} // namespace

std::u8string_view cst_instruction_kind_name(CST_Instruction_Kind type)
{
    using enum CST_Instruction_Kind;
    switch (type) {
        COWEL_ENUM_STRING_CASE8(skip);
        COWEL_ENUM_STRING_CASE8(escape);
        COWEL_ENUM_STRING_CASE8(text);
        COWEL_ENUM_STRING_CASE8(unquoted_string);
        COWEL_ENUM_STRING_CASE8(binary_int);
        COWEL_ENUM_STRING_CASE8(octal_int);
        COWEL_ENUM_STRING_CASE8(decimal_int);
        COWEL_ENUM_STRING_CASE8(hexadecimal_int);
        COWEL_ENUM_STRING_CASE8(decimal_float);
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
        COWEL_ENUM_STRING_CASE8(equals);
        COWEL_ENUM_STRING_CASE8(comma);
        COWEL_ENUM_STRING_CASE8(push_document);
        COWEL_ENUM_STRING_CASE8(pop_document);
        COWEL_ENUM_STRING_CASE8(push_directive_splice);
        COWEL_ENUM_STRING_CASE8(pop_directive_splice);
        COWEL_ENUM_STRING_CASE8(push_directive_call);
        COWEL_ENUM_STRING_CASE8(pop_directive_call);
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

Token_Kind cst_instruction_kind_fixed_token(CST_Instruction_Kind type)
{
    using enum CST_Instruction_Kind;
    switch (type) {
    case escape: return Token_Kind::escape;
    case unquoted_string: return Token_Kind::unquoted_identifier;
    case binary_int: return Token_Kind::binary_int;
    case octal_int: return Token_Kind::octal_int;
    case decimal_int: return Token_Kind::decimal_int;
    case hexadecimal_int: return Token_Kind::hexadecimal_int_literal;
    case decimal_float: return Token_Kind::decimal_float;
    case keyword_true: return Token_Kind::true_;
    case keyword_false: return Token_Kind::false_;
    case keyword_null: return Token_Kind::null;
    case keyword_unit: return Token_Kind::unit;
    case keyword_infinity: return Token_Kind::infinity;
    case keyword_neg_infinity: return Token_Kind::negative_infinity;
    case line_comment: return Token_Kind::line_comment;
    case block_comment: return Token_Kind::block_comment;
    case member_name: return Token_Kind::unquoted_identifier;
    case ellipsis: return Token_Kind::ellipsis;
    case equals: return Token_Kind::equals;
    case comma: return Token_Kind::comma;
    case push_directive_splice: return Token_Kind::directive_splice_name;
    case push_directive_call: return Token_Kind::unquoted_identifier;
    case push_group: return Token_Kind::parenthesis_left;
    case pop_group: return Token_Kind::parenthesis_right;
    case push_block: return Token_Kind::brace_left;
    case pop_block: return Token_Kind::brace_right;
    case push_quoted_string:
    case pop_quoted_string: return Token_Kind::string_quote;

    case skip:
    case text:
    case push_document:
    case pop_document:
    case push_named_member:
    case pop_named_member:
    case push_positional_member:
    case pop_positional_member:
    case push_ellipsis_argument:
    case pop_ellipsis_argument:
    case pop_directive_splice:
    case pop_directive_call:
        //
        return Token_Kind::error;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid instruction.");
}

bool cst_instruction_kind_advances(CST_Instruction_Kind kind)
{
    using enum CST_Instruction_Kind;
    switch (kind) {
    case skip:
    case escape:
    case unquoted_string:
    case binary_int:
    case octal_int:
    case decimal_int:
    case hexadecimal_int:
    case decimal_float:
    case keyword_true:
    case keyword_false:
    case keyword_null:
    case keyword_unit:
    case keyword_infinity:
    case keyword_neg_infinity:
    case line_comment:
    case block_comment:
    case member_name:
    case ellipsis:
    case equals:
    case comma:
    case push_directive_splice:
    case push_directive_call:
    case push_group:
    case pop_group:
    case push_block:
    case pop_block:
    case push_quoted_string:
    case pop_quoted_string:
    case text: return true;

    case push_document:
    case pop_document:
    case push_named_member:
    case pop_named_member:
    case push_positional_member:
    case pop_positional_member:
    case push_ellipsis_argument:
    case pop_ellipsis_argument:
    case pop_directive_splice:
    case pop_directive_call: return false;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid instruction.");
}

bool parse(
    std::pmr::vector<CST_Instruction>& out,
    std::span<const Token> tokens,
    Parse_Error_Consumer on_error
)
{
    return Parser { out, tokens, on_error }();
}

} // namespace cowel
