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

/// @brief Returns the precedence of a binary operator token kind.
/// Higher numbers indicate higher precedence (bind more tightly).
/// Returns 0 if the token is not a binary operator.
[[nodiscard]]
constexpr int token_kind_op_precedence(const Token_Kind kind)
{
    using enum Token_Kind;
    switch (kind) {
    case logical_or: return 1;
    case logical_and: return 2;
    case equals_equals:
    case not_equals: return 3;
    case less_than:
    case greater_than:
    case less_equal:
    case greater_equal: return 4;
    case plus:
    case minus: return 5;
    case asterisk:
    case slash:
    case percent: return 6;
    default: return 0;
    }
}

struct Binary_Op_Instructions {
    CST_Instruction_Kind push;
    CST_Instruction_Kind pop;
};

[[nodiscard]]
constexpr Binary_Op_Instructions token_kind_binary_op_instructions(const Token_Kind kind)
{
    switch (kind) {
    case Token_Kind::logical_or:
        return {
            CST_Instruction_Kind::push_expr_logical_or,
            CST_Instruction_Kind::pop_expr_logical_or,
        };
    case Token_Kind::logical_and:
        return {
            CST_Instruction_Kind::push_expr_logical_and,
            CST_Instruction_Kind::pop_expr_logical_and,
        };
    case Token_Kind::equals_equals:
        return {
            CST_Instruction_Kind::push_expr_equals,
            CST_Instruction_Kind::pop_expr_equals,
        };
    case Token_Kind::not_equals:
        return {
            CST_Instruction_Kind::push_expr_not_equals,
            CST_Instruction_Kind::pop_expr_not_equals,
        };
    case Token_Kind::less_than:
        return {
            CST_Instruction_Kind::push_expr_less_than,
            CST_Instruction_Kind::pop_expr_less_than,
        };
    case Token_Kind::greater_than:
        return {
            CST_Instruction_Kind::push_expr_greater_than,
            CST_Instruction_Kind::pop_expr_greater_than,
        };
    case Token_Kind::less_equal:
        return {
            CST_Instruction_Kind::push_expr_less_equal,
            CST_Instruction_Kind::pop_expr_less_equal,
        };
    case Token_Kind::greater_equal:
        return {
            CST_Instruction_Kind::push_expr_greater_equal,
            CST_Instruction_Kind::pop_expr_greater_equal,
        };
    case Token_Kind::plus:
        return {
            CST_Instruction_Kind::push_expr_add,
            CST_Instruction_Kind::pop_expr_add,
        };
    case Token_Kind::minus:
        return {
            CST_Instruction_Kind::push_expr_subtract,
            CST_Instruction_Kind::pop_expr_subtract,
        };
    case Token_Kind::asterisk:
        return {
            CST_Instruction_Kind::push_expr_multiply,
            CST_Instruction_Kind::pop_expr_multiply,
        };
    case Token_Kind::slash:
        return {
            CST_Instruction_Kind::push_expr_divide,
            CST_Instruction_Kind::pop_expr_divide,
        };
    case Token_Kind::percent:
        return {
            CST_Instruction_Kind::push_expr_modulo,
            CST_Instruction_Kind::pop_expr_modulo,
        };
    default: break;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid binary operator token kind.");
}

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

    [[nodiscard]]
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

    // Parses a construct starting with `(` in a context where expressions can appear.
    //
    // This is difficult because e.g. `(0)` is a parenthesized-expression,
    // whereas `(0,)` is a group with a single positional member.
    // In general, we only know the meaning of the construct after reading the first member
    // (ellipses and named members imply parsing as a group)
    // or something immediately following it
    // (a comma following a positional member implies parsing as a group).
    void consume_group_or_parenthesized_expression()
    {
        COWEL_ASSERT(expect(Token_Kind::parenthesis_left));

        const std::size_t wrapper_instruction_index = m_out.size();
        m_out.push_back({ CST_Instruction_Kind::push_expr_parenthesized });

        consume_blank_sequence();
        if (expect(Token_Kind::parenthesis_right)) {
            m_out[wrapper_instruction_index] = { CST_Instruction_Kind::push_group, 0 };
            m_out.push_back({ CST_Instruction_Kind::pop_group });
            return;
        }

        const std::size_t first_member_instruction_index = m_out.size();
        enum struct First_Member_Kind : Default_Underlying {
            named,
            positional,
            ellipsis,
        };

        const auto consume_group_tail = [&](std::size_t member_count) -> void {
            while (!eof()) {
                if (expect(Token_Kind::parenthesis_right)) {
                    m_out[wrapper_instruction_index].n = member_count;
                    m_out.push_back({ CST_Instruction_Kind::pop_group });
                    return;
                }
                if (expect(Token_Kind::comma)) {
                    m_out.push_back({ CST_Instruction_Kind::comma });
                    consume_blank_sequence();
                    if (expect(Token_Kind::parenthesis_right)) {
                        m_out[wrapper_instruction_index].n = member_count;
                        m_out.push_back({ CST_Instruction_Kind::pop_group });
                        return;
                    }
                    consume_group_member();
                    ++member_count;
                    continue;
                }
                error(m_tokens[m_pos].location, u8"Invalid group member."sv);
                skip_to_end_of_group_member();
            }

            error(
                m_tokens.back().location,
                u8"Unterminated group should have been dealt with by lexer."sv
            );
        };

        const auto promote_first_member_to_group
            = [&](const CST_Instruction_Kind push, const CST_Instruction_Kind pop) -> void {
            m_out[wrapper_instruction_index] = { CST_Instruction_Kind::push_group, 1 };
            // We have parsed an expression without the push/pop for the group member,
            // so this wrapping needs to be retroactively inserted.
            m_out.insert(
                m_out.begin() + static_cast<std::ptrdiff_t>(first_member_instruction_index),
                { push }
            );
            m_out.push_back({ pop });
        };

        const auto first_member_kind = [&] -> std::optional<First_Member_Kind> {
            if (expect_member_name()) {
                consume_blank_sequence();
                if (!expect_expression()) {
                    // Even though we couldn't parse the argument expression,
                    // we still return `named` because we recognize the group member kind.
                    error(m_tokens[m_pos].location, u8"Invalid group member value."sv);
                    skip_to_end_of_group_member();
                }
                return First_Member_Kind::named;
            }
            if (expect(Token_Kind::ellipsis)) {
                m_out.push_back({ CST_Instruction_Kind::ellipsis });
                return First_Member_Kind::ellipsis;
            }
            if (expect_expression()) {
                return First_Member_Kind::positional;
            }
            return {};
        }();
        if (!first_member_kind) {
            error(m_tokens[m_pos].location, u8"Invalid group member value."sv);
            m_out[wrapper_instruction_index] = { CST_Instruction_Kind::push_group, 0 };
            consume_group_tail(0);
            return;
        }

        consume_blank_sequence();

        if (*first_member_kind == First_Member_Kind::positional
            && expect(Token_Kind::parenthesis_right)) {
            m_out.push_back({ CST_Instruction_Kind::pop_expr_parenthesized });
            return;
        }

        switch (*first_member_kind) {
        case First_Member_Kind::named: {
            promote_first_member_to_group(
                CST_Instruction_Kind::push_named_member, CST_Instruction_Kind::pop_named_member
            );
            consume_group_tail(1);
            return;
        }
        case First_Member_Kind::positional: {
            if (!peek(Token_Kind::comma)) {
                error(
                    m_tokens[m_pos].location,
                    u8"Expected ')' or ',' after parenthesized expression."sv
                );
                skip_to_end_of_group_member();
            }
            promote_first_member_to_group(
                CST_Instruction_Kind::push_positional_member,
                CST_Instruction_Kind::pop_positional_member
            );
            consume_group_tail(1);
            return;
        }
        case First_Member_Kind::ellipsis: {
            promote_first_member_to_group(
                CST_Instruction_Kind::push_ellipsis_argument,
                CST_Instruction_Kind::pop_ellipsis_argument
            );
            consume_group_tail(1);
            return;
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid first member kind.");
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
            if (!expect_expression()) {
                error(m_tokens[m_pos].location, u8"Invalid group member value."sv);
                skip_to_end_of_group_member();
                return;
            }
            consume_blank_sequence();
        }

        if (!peek(Token_Kind::comma) && !peek(Token_Kind::parenthesis_right)) {
            error(m_tokens[m_pos].location, u8"Invalid group member."sv);
            skip_to_end_of_group_member();
            return;
        }

        m_out[argument_instruction_index].kind = push_type;
        m_out.push_back({ pop_type });
    }

    /// @brief Advances the parser past this group member,
    /// to a comma or closing parenthesis.
    ///
    /// This function should only be used for error recovery.
    /// It also takes nested constructs like parentheses and braces into account,
    /// and parses these as groups or blocks.
    /// Doing so is usually sufficient to avoid an explosion in error count on recovery.
    void skip_to_end_of_group_member()
    {
        if (peek(Token_Kind::comma) || peek(Token_Kind::parenthesis_right)) {
            return;
        }

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
                advance_by(1);
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
        if (next->kind != Token_Kind::identifier && next->kind != Token_Kind::string_quote) {
            return false;
        }
        Scoped_Attempt a = attempt();
        if (next->kind == Token_Kind::identifier) {
            emit_and_advance_by_one(CST_Instruction_Kind::unquoted_member_name);
        }
        else {
            consume_quoted(
                CST_Instruction_Kind::push_quoted_member_name,
                CST_Instruction_Kind::pop_quoted_member_name
            );
        }
        consume_blank_sequence();
        if (expect(Token_Kind::equals)) {
            m_out.push_back({ CST_Instruction_Kind::equals });
            consume_blank_sequence();
            a.commit();
            return true;
        }
        return false;
    }

    [[nodiscard]]
    bool expect_expression()
    {
        constexpr int min_precedence = 0;
        return expect_expression_with_min_precedence(min_precedence);
    }

    /// @brief Pratt parser implementation for binary expressions.
    /// This parses an expression with at least the given minimum precedence,
    /// handling left-associative binary operators correctly.
    [[nodiscard]]
    bool expect_expression_with_min_precedence(const int min_precedence)
    {
        COWEL_DEBUG_ASSERT(min_precedence >= 0);

        // Record where the left operand begins in the output.
        // When a binary operator is found, we insert the push instruction here,
        // so that push_op wraps the entire left subtree (not just the right operand).
        const std::size_t left_start = m_out.size();

        if (!expect_unary_expression()) {
            return false;
        }

        // Now try to parse binary operators and their right operands.
        // We loop while the next token is a binary operator with sufficient precedence.
        while (true) {
            consume_blank_sequence();

            const Token* const next = peek();
            if (!next) {
                break;
            }

            const int op_precedence = token_kind_op_precedence(next->kind);
            if (op_precedence == 0 || op_precedence < min_precedence) {
                // Next token is not a binary operator, or has lower precedence than required.
                // Stop parsing; the caller (higher-precedence parser) will handle this.
                break;
            }

            // We have a binary operator with sufficient precedence.
            // For left-associativity, the right operand should have precedence > op_precedence.
            // This ensures that x+y+z is parsed as (x+y)+z, not x+(y+z).
            const Token* const op_token = next;
            const auto [push_op, pop_op] = token_kind_binary_op_instructions(op_token->kind);

            // Consume the operator token.
            advance_by(1);

            // Insert the push instruction before the left subtree so that the binary
            // expression node wraps both operands: push_op left right pop_op.
            m_out.insert(
                m_out.begin() + static_cast<std::ptrdiff_t>(left_start), CST_Instruction { push_op }
            );
            consume_blank_sequence();

            // Parse the right operand with higher precedence (for left-associativity).
            if (!expect_expression_with_min_precedence(op_precedence + 1)) {
                error(m_tokens[m_pos].location, u8"Expected expression after binary operator."sv);
                skip_to_end_of_group_member();
            }

            m_out.push_back({ pop_op });
        }

        return true;
    }

    /// @brief Parses a primary expression or a prefix operator followed by an expression.
    ///
    /// This is the base case for the Pratt parser; it parses the left-most operand
    /// and any prefix operators that precede it.
    [[nodiscard]]
    bool expect_unary_expression()
    {
        const Token* const next = peek();
        if (!next) {
            return false;
        }

        switch (next->kind) {
            // Prefix expressions:
        case Token_Kind::bitwise_not: {
            return do_expect_prefix_expression(
                Token_Kind::bitwise_not, //
                CST_Instruction_Kind::push_expr_bitwise_not,
                CST_Instruction_Kind::pop_expr_bitwise_not
            );
        }
        case Token_Kind::logical_not: {
            return do_expect_prefix_expression(
                Token_Kind::logical_not, //
                CST_Instruction_Kind::push_expr_logical_not,
                CST_Instruction_Kind::pop_expr_logical_not
            );
        }
        case Token_Kind::plus: {
            return do_expect_prefix_expression(
                Token_Kind::plus, //
                CST_Instruction_Kind::push_expr_unary_plus,
                CST_Instruction_Kind::pop_expr_unary_plus
            );
        }
        case Token_Kind::minus: {
            return do_expect_prefix_expression(
                Token_Kind::minus, //
                CST_Instruction_Kind::push_expr_unary_minus,
                CST_Instruction_Kind::pop_expr_unary_minus
            );
        }

        // Postfix expressions:
        case Token_Kind::identifier: {
            if (expect_directive_call_expression()) {
                return true;
            }
            emit_and_advance_by_one(CST_Instruction_Kind::unquoted_string);
            return true;
        }

        // Primary expressions:
        case Token_Kind::string_quote: {
            consume_quoted(
                CST_Instruction_Kind::push_quoted_string, CST_Instruction_Kind::pop_quoted_string
            );
            return true;
        }
        case Token_Kind::parenthesis_left: {
            consume_group_or_parenthesized_expression();
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

        // Tokens that cannot start an expression in general.
        case Token_Kind::comma:
        case Token_Kind::ellipsis:
        case Token_Kind::equals:
        case Token_Kind::parenthesis_right:
        case Token_Kind::brace_right:
        case Token_Kind::directive_splice_name:
        case Token_Kind::escape:
        // Binary operators cannot start an expression (they're infix).
        case Token_Kind::asterisk:
        case Token_Kind::equals_equals:
        case Token_Kind::greater_equal:
        case Token_Kind::greater_than:
        case Token_Kind::less_equal:
        case Token_Kind::less_than:
        case Token_Kind::logical_and:
        case Token_Kind::logical_or:
        case Token_Kind::not_equals:
        case Token_Kind::percent:
        case Token_Kind::slash: return false;

        case Token_Kind::document_text:
        case Token_Kind::quoted_string_text:
        case Token_Kind::block_text:
        case Token_Kind::error:
        case Token_Kind::reserved_escape:
        case Token_Kind::reserved_number:
        case Token_Kind::whitespace:
        case Token_Kind::block_comment:
        case Token_Kind::line_comment: break;
        }

        error(m_tokens[m_pos].location, u8"Unexpected token in expression."sv);
        return false;
    }

    [[nodiscard]]
    bool do_expect_prefix_expression(
        const Token_Kind op,
        const CST_Instruction_Kind push,
        const CST_Instruction_Kind pop
    )
    {
        const Token* const next = expect(op);
        COWEL_ASSERT(next && next->kind == op);

        Scoped_Attempt a = attempt();

        m_out.push_back({ push });
        consume_blank_sequence();
        if (!expect_unary_expression()) {
            return false;
        }
        m_out.push_back({ pop });
        a.commit();
        return true;
    }

    [[nodiscard]]
    bool expect_directive_call_expression()
    {
        Scoped_Attempt a = attempt();

        const Token* const next = expect(Token_Kind::identifier);
        if (!next) {
            return false;
        }

        m_out.push_back({ CST_Instruction_Kind::push_expr_directive_call });

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

        m_out.push_back({ CST_Instruction_Kind::pop_expr_directive_call });

        a.commit();
        return true;
    }

    void consume_quoted(CST_Instruction_Kind push, CST_Instruction_Kind pop)
    {
        COWEL_DEBUG_ASSERT(
            push == CST_Instruction_Kind::push_quoted_member_name
            || push == CST_Instruction_Kind::push_quoted_string
        );
        COWEL_DEBUG_ASSERT(
            pop == CST_Instruction_Kind::pop_quoted_member_name
            || pop == CST_Instruction_Kind::pop_quoted_string
        );
        COWEL_ASSERT(expect(Token_Kind::string_quote));

        const std::size_t instruction_index = m_out.size();
        m_out.push_back({ push });

        const std::size_t elements = consume_markup_sequence(Content_Context::quoted_string);
        const bool is_closed = expect(Token_Kind::string_quote);
        COWEL_ASSERT(is_closed);

        m_out.push_back({ pop });
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
        COWEL_ENUM_STRING_CASE8(unquoted_member_name);
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
        COWEL_ENUM_STRING_CASE8(line_comment);
        COWEL_ENUM_STRING_CASE8(block_comment);
        COWEL_ENUM_STRING_CASE8(ellipsis);
        COWEL_ENUM_STRING_CASE8(equals);
        COWEL_ENUM_STRING_CASE8(comma);
        COWEL_ENUM_STRING_CASE8(push_document);
        COWEL_ENUM_STRING_CASE8(pop_document);
        COWEL_ENUM_STRING_CASE8(push_directive_splice);
        COWEL_ENUM_STRING_CASE8(pop_directive_splice);
        COWEL_ENUM_STRING_CASE8(push_expr_bitwise_not);
        COWEL_ENUM_STRING_CASE8(pop_expr_bitwise_not);
        COWEL_ENUM_STRING_CASE8(push_expr_logical_not);
        COWEL_ENUM_STRING_CASE8(pop_expr_logical_not);
        COWEL_ENUM_STRING_CASE8(push_expr_unary_plus);
        COWEL_ENUM_STRING_CASE8(pop_expr_unary_plus);
        COWEL_ENUM_STRING_CASE8(push_expr_unary_minus);
        COWEL_ENUM_STRING_CASE8(pop_expr_unary_minus);
        COWEL_ENUM_STRING_CASE8(push_expr_parenthesized);
        COWEL_ENUM_STRING_CASE8(pop_expr_parenthesized);
        COWEL_ENUM_STRING_CASE8(push_expr_directive_call);
        COWEL_ENUM_STRING_CASE8(pop_expr_directive_call);
        COWEL_ENUM_STRING_CASE8(push_expr_logical_or);
        COWEL_ENUM_STRING_CASE8(pop_expr_logical_or);
        COWEL_ENUM_STRING_CASE8(push_expr_logical_and);
        COWEL_ENUM_STRING_CASE8(pop_expr_logical_and);
        COWEL_ENUM_STRING_CASE8(push_expr_equals);
        COWEL_ENUM_STRING_CASE8(pop_expr_equals);
        COWEL_ENUM_STRING_CASE8(push_expr_not_equals);
        COWEL_ENUM_STRING_CASE8(pop_expr_not_equals);
        COWEL_ENUM_STRING_CASE8(push_expr_less_than);
        COWEL_ENUM_STRING_CASE8(pop_expr_less_than);
        COWEL_ENUM_STRING_CASE8(push_expr_greater_than);
        COWEL_ENUM_STRING_CASE8(pop_expr_greater_than);
        COWEL_ENUM_STRING_CASE8(push_expr_less_equal);
        COWEL_ENUM_STRING_CASE8(pop_expr_less_equal);
        COWEL_ENUM_STRING_CASE8(push_expr_greater_equal);
        COWEL_ENUM_STRING_CASE8(pop_expr_greater_equal);
        COWEL_ENUM_STRING_CASE8(push_expr_add);
        COWEL_ENUM_STRING_CASE8(pop_expr_add);
        COWEL_ENUM_STRING_CASE8(push_expr_subtract);
        COWEL_ENUM_STRING_CASE8(pop_expr_subtract);
        COWEL_ENUM_STRING_CASE8(push_expr_multiply);
        COWEL_ENUM_STRING_CASE8(pop_expr_multiply);
        COWEL_ENUM_STRING_CASE8(push_expr_divide);
        COWEL_ENUM_STRING_CASE8(pop_expr_divide);
        COWEL_ENUM_STRING_CASE8(push_expr_modulo);
        COWEL_ENUM_STRING_CASE8(pop_expr_modulo);
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
        COWEL_ENUM_STRING_CASE8(push_quoted_member_name);
        COWEL_ENUM_STRING_CASE8(pop_quoted_member_name);
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
    case unquoted_member_name:
    case unquoted_string: return Token_Kind::identifier;
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
    case line_comment: return Token_Kind::line_comment;
    case block_comment: return Token_Kind::block_comment;
    case ellipsis: return Token_Kind::ellipsis;
    case equals: return Token_Kind::equals;
    case comma: return Token_Kind::comma;
    case push_directive_splice: return Token_Kind::directive_splice_name;
    case push_expr_bitwise_not: return Token_Kind::bitwise_not;
    case push_expr_logical_not: return Token_Kind::logical_not;
    case push_expr_unary_minus: return Token_Kind::minus;
    case push_expr_unary_plus: return Token_Kind::plus;
    case push_expr_parenthesized: return Token_Kind::parenthesis_left;
    case pop_expr_parenthesized: return Token_Kind::parenthesis_right;
    case push_expr_directive_call: return Token_Kind::identifier;
    case push_group: return Token_Kind::parenthesis_left;
    case pop_group: return Token_Kind::parenthesis_right;
    case push_block: return Token_Kind::brace_left;
    case pop_block: return Token_Kind::brace_right;
    case push_quoted_member_name:
    case pop_quoted_member_name:
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
    case pop_expr_bitwise_not:
    case pop_expr_logical_not:
    case pop_expr_unary_minus:
    case pop_expr_unary_plus:
    case pop_expr_directive_call:
    case push_expr_logical_or:
    case pop_expr_logical_or:
    case push_expr_logical_and:
    case pop_expr_logical_and:
    case push_expr_equals:
    case pop_expr_equals:
    case push_expr_not_equals:
    case pop_expr_not_equals:
    case push_expr_less_than:
    case pop_expr_less_than:
    case push_expr_greater_than:
    case pop_expr_greater_than:
    case push_expr_less_equal:
    case pop_expr_less_equal:
    case push_expr_greater_equal:
    case pop_expr_greater_equal:
    case push_expr_add:
    case pop_expr_add:
    case push_expr_subtract:
    case pop_expr_subtract:
    case push_expr_multiply:
    case pop_expr_multiply:
    case push_expr_divide:
    case pop_expr_divide:
    case push_expr_modulo:
    case pop_expr_modulo:
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
    case unquoted_member_name:
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
    case line_comment:
    case block_comment:
    case ellipsis:
    case equals:
    case comma:
    case push_directive_splice:
    case push_expr_bitwise_not:
    case push_expr_logical_not:
    case push_expr_unary_minus:
    case push_expr_unary_plus:
    case push_expr_parenthesized:
    case push_expr_directive_call:
    case push_group:
    case pop_group:
    case pop_expr_parenthesized:
    case push_block:
    case pop_block:
    case push_quoted_member_name:
    case pop_quoted_member_name:
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
    case pop_expr_bitwise_not:
    case pop_expr_logical_not:
    case pop_expr_unary_minus:
    case pop_expr_unary_plus:
    case pop_expr_directive_call:
    case push_expr_logical_or:
    case pop_expr_logical_or:
    case push_expr_logical_and:
    case pop_expr_logical_and:
    case push_expr_equals:
    case pop_expr_equals:
    case push_expr_not_equals:
    case pop_expr_not_equals:
    case push_expr_less_than:
    case pop_expr_less_than:
    case push_expr_greater_than:
    case pop_expr_greater_than:
    case push_expr_less_equal:
    case pop_expr_less_equal:
    case push_expr_greater_equal:
    case pop_expr_greater_equal:
    case push_expr_add:
    case pop_expr_add:
    case push_expr_subtract:
    case pop_expr_subtract:
    case push_expr_multiply:
    case pop_expr_multiply:
    case push_expr_divide:
    case pop_expr_divide:
    case push_expr_modulo:
    case pop_expr_modulo: return false;
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
