#ifndef COWEL_PARSE_HPP
#define COWEL_PARSE_HPP

#include <compare>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/ast.hpp"
#include "cowel/fwd.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/function_ref.hpp"

namespace cowel {

enum struct CST_Instruction_Kind : Default_Underlying {
    skip,
    escape,
    text,
    unquoted_string,
    binary_int,
    octal_int,
    decimal_int,
    hexadecimal_int,
    decimal_float,
    keyword_true,
    keyword_false,
    keyword_null,
    keyword_unit,
    keyword_infinity,
    keyword_neg_infinity,
    line_comment,
    block_comment,
    member_name,
    ellipsis,
    equals,
    comma,
    /// @brief `n` is the amount of markup elements in the document.
    push_document,
    pop_document,
    push_directive_splice,
    pop_directive_splice,
    push_directive_call,
    pop_directive_call,
    /// @brief `n` is the amount of group members.
    push_group,
    pop_group,
    push_named_member,
    pop_named_member,
    push_positional_member,
    pop_positional_member,
    push_ellipsis_argument,
    pop_ellipsis_argument,
    /// @brief `n` is the amount of markup elements in the block.
    push_block,
    pop_block,
    /// @brief `n` is the amount of markup elements in the string.
    push_quoted_string,
    pop_quoted_string,
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
constexpr bool cst_instruction_kind_is_push_argument(CST_Instruction_Kind type)
{
    return type == CST_Instruction_Kind::push_named_member
        || type == CST_Instruction_Kind::push_positional_member
        || type == CST_Instruction_Kind::push_ellipsis_argument;
}

[[nodiscard]]
constexpr bool cst_instruction_kind_is_pop_argument(CST_Instruction_Kind type)
{
    return type == CST_Instruction_Kind::pop_named_member
        || type == CST_Instruction_Kind::pop_positional_member
        || type == CST_Instruction_Kind::pop_ellipsis_argument;
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
