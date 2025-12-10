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

enum struct AST_Instruction_Type : Default_Underlying {
    /// @brief Ignore the next `n` characters.
    /// This is used only within directive arguments,
    /// where leading and trailing whitespace generally doesn't matter.
    skip,
    /// @brief The next `n` characters are an escape sequence (e.g. `\{`).
    escape,
    /// @brief The next `n` characters are literal text.
    text,
    /// @brief The next `n` characters are an unquoted string.
    unquoted_string,
    /// @brief The next `n` characters are decimal integer literal.
    decimal_int_literal,
    /// @brief The next `n` characters are decimal floating-point literal.
    float_literal,
    /// @brief The next `n` characters are `true`.
    keyword_true,
    /// @brief The next `n` characters are `false`.
    keyword_false,
    /// @brief The next `n` characters are `null`.
    keyword_null,
    /// @brief The next `n` characters are `unit`.
    keyword_unit,
    /// @brief The next `n` characters are `infinity`.
    keyword_infinity,
    /// @brief The next `n` characters are `-infinity`.
    keyword_neg_infinity,
    /// @brief The next `n` characters are a comment,
    /// including the `\:` prefix and the terminating newline, if any.
    comment,
    /// @brief The next `n` characters are an argument name.
    member_name,
    /// @brief The next `n` characters are an ellipsis (currently always `3`).
    ellipsis,
    /// @brief Advance past `=` following an argument name.
    member_equal,
    /// @brief Advance past `,` between arguments.
    member_comma,
    /// @brief Begins the document.
    /// Always the first instruction.
    /// The operand is the amount of pieces that comprise the argument content,
    /// where a piece is an escape sequence, text, or a directive.
    push_document,
    pop_document,
    /// @brief Begin directive.
    /// The operand is the amount of characters to advance until the end the directive name.
    /// Note that this includes the leading `\\`.
    push_directive,
    pop_directive,
    /// @brief Begin directive arguments.
    /// The operand is the amount of arguments.
    ///
    /// Advance past `(`.
    push_group,
    /// @brief Advance past `)`.
    pop_group,
    /// @brief Begin argument.
    /// The operand is the amount of elements in the content sequence,
    /// or zero if the argument is a group.
    push_named_member,
    pop_named_member,
    /// @brief Begin argument.
    /// The operand is the amount of elements in the content sequence,
    /// or zero if the argument is a group.
    push_positional_member,
    pop_positional_member,
    /// @brief Begin argument.
    /// The operand is the amount of elements in the content sequence.
    push_ellipsis_argument,
    pop_ellipsis_argument,
    /// @brief Begin directive content.
    /// The operand is the amount of pieces that comprise the argument content,
    /// where a piece is an escape sequence, text, or a directive.
    ///
    /// Advance past `{`.
    push_block,
    /// @brief Advance past `}`.
    pop_block,
    /// @brief Begin quoted string.
    /// The operand is the amount of pieces that comprise the string.
    ///
    /// Advance past `"`.
    push_quoted_string,
    /// Advance past `"`.
    pop_quoted_string,
};

[[nodiscard]]
constexpr bool ast_instruction_type_has_operand(AST_Instruction_Type type)
{
    using enum AST_Instruction_Type;
    switch (type) {
    case pop_document:
    case pop_directive:
    case pop_group:
    case pop_quoted_string:
    case pop_block:

    case push_named_member:
    case pop_named_member:

    case push_positional_member:
    case pop_positional_member:

    case push_ellipsis_argument:
    case pop_ellipsis_argument:

    case member_comma:
    case member_equal: return false;
    default: return true;
    }
}

[[nodiscard]]
constexpr bool ast_instruction_type_is_push_argument(AST_Instruction_Type type)
{
    return type == AST_Instruction_Type::push_named_member
        || type == AST_Instruction_Type::push_positional_member
        || type == AST_Instruction_Type::push_ellipsis_argument;
}

[[nodiscard]]
constexpr bool ast_instruction_type_is_pop_argument(AST_Instruction_Type type)
{
    return type == AST_Instruction_Type::pop_named_member
        || type == AST_Instruction_Type::pop_positional_member
        || type == AST_Instruction_Type::pop_ellipsis_argument;
}

[[nodiscard]]
std::u8string_view ast_instruction_type_name(AST_Instruction_Type type);

struct AST_Instruction {
    AST_Instruction_Type type;
    std::size_t n = 0;

    friend std::strong_ordering operator<=>(const AST_Instruction&, const AST_Instruction&)
        = default;
};

using Parse_Error_Consumer = Function_Ref<
    void(std::u8string_view id, const Source_Span& location, Char_Sequence8 message)>;

/// @brief Parses the COWEL document.
/// This process does not result in an AST, but a vector of instructions that can be used to
/// construct an AST.
/// @param out A vector where instructions for constructing a syntax tree are emitted.
/// @param source The source code to parse.
/// @param on_error If not empty, invoked whenever a parse error is encountered.
/// @returns `true` iff parsing succeeded without any errors.
[[nodiscard]]
bool parse(
    std::pmr::vector<AST_Instruction>& out,
    std::u8string_view source,
    Parse_Error_Consumer on_error = {}
);

/// @brief Builds an AST from a span of instructions,
/// usually obtained from `parse`.
void build_ast(
    ast::Pmr_Vector<ast::Markup_Element>& out,
    std::u8string_view source,
    File_Id file,
    std::span<const AST_Instruction> instructions,
    std::pmr::memory_resource* memory
);

/// @brief Builds an AST from a span of instructions,
/// usually obtained from `parse`.
[[nodiscard]]
ast::Pmr_Vector<ast::Markup_Element> build_ast(
    std::u8string_view source,
    File_Id file,
    std::span<const AST_Instruction> instructions,
    std::pmr::memory_resource* memory
);

/// @brief Parses a document via `parse(out, source, on_error)`.
/// If `parse` returns `true`, runs `build_ast` on the resulting parse instructions.
/// Otherwise, returns `false`.
[[nodiscard]]
bool parse_and_build(
    ast::Pmr_Vector<ast::Markup_Element>& out,
    std::u8string_view source,
    File_Id file,
    std::pmr::memory_resource* memory,
    Parse_Error_Consumer on_error = {}
);

} // namespace cowel

#endif
