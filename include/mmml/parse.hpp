#ifndef MMML_TOKENS_HPP
#define MMML_TOKENS_HPP

#include <span>
#include <string_view>
#include <vector>

#include "mmml/util/source_position.hpp"

#include "mmml/ast.hpp"
#include "mmml/fwd.hpp"

namespace mmml {

enum struct AST_Instruction_Type : Default_Underlying {
    /// @brief Ignore the next `n` characters.
    skip,
    /// @brief The next `n` characters are an escape sequence (e.g. `\\{`).
    escape,
    /// @brief The next `n` characters are literal text.
    text,
    /// @brief The next `n` characters are an argument name.
    argument_name,
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
    /// One leading `{` and one trailing `}` are implicitly skipped.
    push_arguments,
    pop_arguments,
    /// @brief Begin argument.
    /// The operand is the amount of pieces that comprise the argument content,
    /// where a piece is an escape sequence, text, or a directive.
    push_argument,
    pop_argument,
    /// @brief Begin directive content.
    /// The operand is the amount of pieces that comprise the argument content,
    /// where a piece is an escape sequence, text, or a directive.
    ///
    /// One leading `{` and one trailing `}` are implicitly skipped.
    push_block,
    pop_block
};

[[nodiscard]]
constexpr bool ast_instruction_type_has_operand(AST_Instruction_Type type)
{
    using enum AST_Instruction_Type;
    switch (type) {
    case pop_document:
    case pop_directive:
    case pop_arguments:
    case pop_argument:
    case pop_block: return false;
    default: return true;
    }
}

[[nodiscard]]
constexpr std::string_view ast_instruction_type_name(AST_Instruction_Type type)
{
    using enum AST_Instruction_Type;
    switch (type) {
        MMML_ENUM_STRING_CASE(skip);
        MMML_ENUM_STRING_CASE(escape);
        MMML_ENUM_STRING_CASE(text);
        MMML_ENUM_STRING_CASE(argument_name);
        MMML_ENUM_STRING_CASE(push_document);
        MMML_ENUM_STRING_CASE(pop_document);
        MMML_ENUM_STRING_CASE(push_directive);
        MMML_ENUM_STRING_CASE(pop_directive);
        MMML_ENUM_STRING_CASE(push_arguments);
        MMML_ENUM_STRING_CASE(pop_arguments);
        MMML_ENUM_STRING_CASE(push_argument);
        MMML_ENUM_STRING_CASE(pop_argument);
        MMML_ENUM_STRING_CASE(push_block);
        MMML_ENUM_STRING_CASE(pop_block);
    }
    MMML_ASSERT_UNREACHABLE("Invalid type.");
}

struct AST_Instruction {
    AST_Instruction_Type type;
    std::size_t n = 0;

    friend std::strong_ordering operator<=>(const AST_Instruction&, const AST_Instruction&)
        = default;
};

/// @brief Parses the MMML document.
/// This process does not result in an AST, but a vector of instructions that can be used to
/// construct an AST.
///
/// Note that parsing is infallible.
/// In the grammar, any syntax violation can fall back onto literal text,
/// so the parsed result may be undesirable, but always valid.
void parse(std::pmr::vector<AST_Instruction>& out, std::string_view source);

/// @brief Builds an AST from a span of instructions,
/// usually obtained from `parse`.
std::pmr::vector<ast::Content> build_ast(
    std::string_view source,
    std::span<const AST_Instruction> instructions,
    std::pmr::memory_resource* memory
);

/// @brief Parses a document and runs `build_ast` on the results.
std::pmr::vector<ast::Content>
parse_and_build(std::string_view source, std::pmr::memory_resource* memory);

} // namespace mmml

#endif
