#ifndef COWEL_PARSE_HPP
#define COWEL_PARSE_HPP

#include <compare>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/function_ref.hpp"

#include "cowel/fwd.hpp"

#include "cowel/syntax/ast.hpp"
#include "cowel/syntax/lex.hpp"

namespace cowel {

#define COWEL_CST_INSTRUCTION_KIND_ENUM_DATA(F)                                                    \
    F(skip, "skip")                                                                                \
    F(escape, "escape")                                                                            \
    F(text, "text")                                                                                \
    F(unquoted_member_name, "unquoted_member_name")                                                \
    F(id_expression, "id_expression")                                                              \
    F(binary_int, "binary_int")                                                                    \
    F(octal_int, "octal_int")                                                                      \
    F(decimal_int, "decimal_int")                                                                  \
    F(hexadecimal_int, "hexadecimal_int")                                                          \
    F(decimal_float, "decimal_float")                                                              \
    F(keyword_true, "keyword_true")                                                                \
    F(keyword_false, "keyword_false")                                                              \
    F(keyword_null, "keyword_null")                                                                \
    F(keyword_unit, "keyword_unit")                                                                \
    F(keyword_infinity, "keyword_infinity")                                                        \
    F(line_comment, "line_comment")                                                                \
    F(block_comment, "block_comment")                                                              \
    F(ellipsis, "ellipsis")                                                                        \
    F(equals, "equals")                                                                            \
    F(comma, "comma")                                                                              \
    /* `n` is the amount of markup elements in the document. */                                    \
    F(push_document, "push_document")                                                              \
    F(pop_document, "pop_document")                                                                \
    F(push_directive_splice, "push_directive_splice")                                              \
    F(pop_directive_splice, "pop_directive_splice")                                                \
    F(push_expression_splice, "push_expression_splice")                                            \
    F(pop_expression_splice, "pop_expression_splice")                                              \
    F(push_expression_line_splice, "push_expression_line_splice")                                  \
    F(pop_expression_line_splice, "pop_expression_line_splice")                                    \
    F(push_expr_bitwise_not, "push_expr_bitwise_not")                                              \
    F(pop_expr_bitwise_not, "pop_expr_bitwise_not")                                                \
    F(push_expr_logical_not, "push_expr_logical_not")                                              \
    F(pop_expr_logical_not, "pop_expr_logical_not")                                                \
    F(push_expr_unary_plus, "push_expr_unary_plus")                                                \
    F(pop_expr_unary_plus, "pop_expr_unary_plus")                                                  \
    F(push_expr_unary_minus, "push_expr_unary_minus")                                              \
    F(pop_expr_unary_minus, "pop_expr_unary_minus")                                                \
    F(push_expr_parenthesized, "push_expr_parenthesized")                                          \
    F(pop_expr_parenthesized, "pop_expr_parenthesized")                                            \
    F(push_expr_directive_call, "push_expr_directive_call")                                        \
    F(pop_expr_directive_call, "pop_expr_directive_call")                                          \
    F(push_expr_let, "push_expr_let")                                                              \
    F(pop_expr_let, "pop_expr_let")                                                                \
    F(push_expr_assign, "push_expr_assign")                                                        \
    F(pop_expr_assign, "pop_expr_assign")                                                          \
    F(push_expr_logical_or, "push_expr_logical_or")                                                \
    F(pop_expr_logical_or, "pop_expr_logical_or")                                                  \
    F(push_expr_logical_and, "push_expr_logical_and")                                              \
    F(pop_expr_logical_and, "pop_expr_logical_and")                                                \
    F(push_expr_equals, "push_expr_equals")                                                        \
    F(pop_expr_equals, "pop_expr_equals")                                                          \
    F(push_expr_not_equals, "push_expr_not_equals")                                                \
    F(pop_expr_not_equals, "pop_expr_not_equals")                                                  \
    F(push_expr_less_than, "push_expr_less_than")                                                  \
    F(pop_expr_less_than, "pop_expr_less_than")                                                    \
    F(push_expr_greater_than, "push_expr_greater_than")                                            \
    F(pop_expr_greater_than, "pop_expr_greater_than")                                              \
    F(push_expr_less_equal, "push_expr_less_equal")                                                \
    F(pop_expr_less_equal, "pop_expr_less_equal")                                                  \
    F(push_expr_greater_equal, "push_expr_greater_equal")                                          \
    F(pop_expr_greater_equal, "pop_expr_greater_equal")                                            \
    F(push_expr_add, "push_expr_add")                                                              \
    F(pop_expr_add, "pop_expr_add")                                                                \
    F(push_expr_subtract, "push_expr_subtract")                                                    \
    F(pop_expr_subtract, "pop_expr_subtract")                                                      \
    F(push_expr_multiply, "push_expr_multiply")                                                    \
    F(pop_expr_multiply, "pop_expr_multiply")                                                      \
    F(push_expr_divide, "push_expr_divide")                                                        \
    F(pop_expr_divide, "pop_expr_divide")                                                          \
    F(push_expr_modulo, "push_expr_modulo")                                                        \
    F(pop_expr_modulo, "pop_expr_modulo")                                                          \
    /* `n` is the amount of group members. */                                                      \
    F(push_group, "push_group")                                                                    \
    F(pop_group, "pop_group")                                                                      \
    F(push_named_member, "push_named_member")                                                      \
    F(pop_named_member, "pop_named_member")                                                        \
    F(push_positional_member, "push_positional_member")                                            \
    F(pop_positional_member, "pop_positional_member")                                              \
    F(push_ellipsis_argument, "push_ellipsis_argument")                                            \
    F(pop_ellipsis_argument, "pop_ellipsis_argument")                                              \
    /* `n` is the amount of markup elements in the block. */                                       \
    F(push_block, "push_block")                                                                    \
    F(pop_block, "pop_block")                                                                      \
    /* `n` is the amount of markup elements in the member name. */                                 \
    F(push_quoted_member_name, "push_quoted_member_name")                                          \
    F(pop_quoted_member_name, "pop_quoted_member_name")                                            \
    /* `n` is the amount of markup elements in the string. */                                      \
    F(push_quoted_string, "push_quoted_string")                                                    \
    F(pop_quoted_string, "pop_quoted_string")

#define COWEL_CST_INSTRUCTION_KIND_ENUMERATOR(id, name) id,

enum struct CST_Instruction_Kind : Default_Underlying {
    COWEL_CST_INSTRUCTION_KIND_ENUM_DATA(COWEL_CST_INSTRUCTION_KIND_ENUMERATOR)
};

[[nodiscard]]
constexpr bool cst_instruction_kind_has_operand(CST_Instruction_Kind type)
{
    using enum CST_Instruction_Kind;
    switch (type) {
    case push_document:
    case push_group:
    case push_quoted_string:
    case push_block: return true;

    default: return false;
    }
}

[[nodiscard]]
std::u8string_view cst_instruction_kind_name(CST_Instruction_Kind type);

struct CST_Instruction {
    CST_Instruction_Kind kind;
    std::size_t n = 0;

    friend std::strong_ordering operator<=>(const CST_Instruction&, const CST_Instruction&)
        = default;
};

/// @brief Returns the fixed kind of token corresponding to this instruction kind.
/// For example, given `ellipsis`, returns `Token_Kind::ellipsis`.
/// If there is no such fixed token, returns `Token_Kind::error`.
[[nodiscard]]
Token_Kind cst_instruction_kind_fixed_token(CST_Instruction_Kind);

/// @brief Returns `true` iff the given kind results in advancing by a token.
[[nodiscard]]
bool cst_instruction_kind_advances(CST_Instruction_Kind);

using Parse_Error_Consumer = Function_Ref<
    void(std::u8string_view id, const Source_Span& location, Char_Sequence8 message)>;

/// @brief Parses the COWEL document.
/// This process does not result in an AST,
/// but a vector of instructions that can be used to construct an CST.
/// In essence, this is a serialized and/or linearized CST.
/// @param out A vector where instructions for constructing a syntax tree are emitted.
/// @param tokens The input tokens.
/// These shall be obtained from a successful call to `lex`.
/// @param on_error If not empty, invoked whenever a parse error is encountered.
/// @returns `true` iff parsing succeeded without any errors.
[[nodiscard]]
bool parse(
    std::pmr::vector<CST_Instruction>& out,
    std::span<const Token> tokens,
    Parse_Error_Consumer on_error = {}
);

/// @brief Builds an AST from a span of instructions,
/// usually obtained from `parse`.
void build_ast(
    ast::Pmr_Vector<ast::Markup_Element>& out,
    std::u8string_view source,
    File_Id file,
    std::span<const Token> tokens,
    std::span<const CST_Instruction> instructions,
    std::pmr::memory_resource* memory
);

/// @brief Builds an AST from a span of instructions,
/// usually obtained from `parse`.
[[nodiscard]]
ast::Pmr_Vector<ast::Markup_Element> build_ast(
    std::u8string_view source,
    File_Id file,
    std::span<const Token> tokens,
    std::span<const CST_Instruction> instructions,
    std::pmr::memory_resource* memory
);

/// @brief Parses a document via `parse(out, tokens, on_error)`.
/// If `parse` returns `true`, runs `build_ast` on the resulting parse instructions.
/// Otherwise, returns `false`.
[[nodiscard]]
bool parse_and_build(
    ast::Pmr_Vector<ast::Markup_Element>& out,
    std::u8string_view source,
    std::span<const Token> tokens,
    File_Id file,
    std::pmr::memory_resource* memory,
    Parse_Error_Consumer on_error = {}
);

[[nodiscard]]
bool lex_and_parse_and_build(
    ast::Pmr_Vector<ast::Markup_Element>& out,
    std::u8string_view source,
    File_Id file,
    std::pmr::memory_resource* memory,
    Parse_Error_Consumer on_error = {}
);

} // namespace cowel

#endif
