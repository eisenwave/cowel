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
    /// @brief The next `n` characters are a comment,
    /// including the `\:` prefix and the terminating newline, if any.
    comment,
    /// @brief The next `n` characters are an argument name.
    argument_name,
    /// @brief The next `n` characters are an ellipsis (currently always `3`).
    argument_ellipsis,
    /// @brief Advance past `=` following an argument name.
    argument_equal,
    /// @brief Advance past `,` between arguments.
    argument_comma,
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
    push_arguments,
    /// @brief Advance past `)`.
    pop_arguments,
    /// @brief Begin argument.
    /// The operand is the amount of elements in the content sequence,
    /// or zero if the argument is a group.
    push_named_argument,
    pop_named_argument,
    /// @brief Begin argument.
    /// The operand is the amount of elements in the content sequence,
    /// or zero if the argument is a group.
    push_positional_argument,
    pop_positional_argument,
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
    /// @brief Advance past `{`.
    error_unclosed_block,
};

[[nodiscard]]
constexpr bool ast_instruction_type_has_operand(AST_Instruction_Type type)
{
    using enum AST_Instruction_Type;
    switch (type) {
    case pop_document:
    case pop_directive:
    case pop_arguments:
    case pop_named_argument:
    case pop_positional_argument:
    case pop_ellipsis_argument:
    case pop_block:
    case argument_comma:
    case argument_equal: return false;
    default: return true;
    }
}

[[nodiscard]]
constexpr bool ast_instruction_type_is_push_argument(AST_Instruction_Type type)
{
    return type == AST_Instruction_Type::push_named_argument
        || type == AST_Instruction_Type::push_positional_argument
        || type == AST_Instruction_Type::push_ellipsis_argument;
}

[[nodiscard]]
constexpr bool ast_instruction_type_is_pop_argument(AST_Instruction_Type type)
{
    return type == AST_Instruction_Type::pop_named_argument
        || type == AST_Instruction_Type::pop_positional_argument
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

/// @brief Parses the COWEL document.
/// This process does not result in an AST, but a vector of instructions that can be used to
/// construct an AST.
///
/// Note that parsing is infallible.
/// In the grammar, any syntax violation can fall back onto literal text,
/// so the parsed result may be undesirable, but always valid.
void parse(std::pmr::vector<AST_Instruction>& out, std::u8string_view source);

using Parse_Error_Consumer = Function_Ref<
    void(std::u8string_view id, const File_Source_Span& location, std::u8string_view message)>;

/// @brief Builds an AST from a span of instructions,
/// usually obtained from `parse`.
void build_ast(
    ast::Pmr_Vector<ast::Content>& out,
    std::u8string_view source,
    File_Id file,
    std::span<const AST_Instruction> instructions,
    std::pmr::memory_resource* memory,
    Parse_Error_Consumer on_error = {}
);

/// @brief Builds an AST from a span of instructions,
/// usually obtained from `parse`.
[[nodiscard]]
ast::Pmr_Vector<ast::Content> build_ast(
    std::u8string_view source,
    File_Id file,
    std::span<const AST_Instruction> instructions,
    std::pmr::memory_resource* memory,
    Parse_Error_Consumer on_error = {}
);

/// @brief Parses a document and runs `build_ast` on the results.
void parse_and_build(
    ast::Pmr_Vector<ast::Content>& out,
    std::u8string_view source,
    File_Id file,
    std::pmr::memory_resource* memory,
    Parse_Error_Consumer on_error = {}
);

/// @brief Parses a document and runs `build_ast` on the results.
[[nodiscard]]
ast::Pmr_Vector<ast::Content> parse_and_build(
    std::u8string_view source,
    File_Id file,
    std::pmr::memory_resource* memory,
    Parse_Error_Consumer on_error = {}
);

} // namespace cowel

#endif
