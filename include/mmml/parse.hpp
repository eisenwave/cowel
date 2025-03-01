#ifndef MMML_BMD_TOKENS_HPP
#define MMML_BMD_TOKENS_HPP

#include <span>
#include <string_view>
#include <vector>

#include "mmml/ast.hpp"
#include "mmml/fwd.hpp"
#include "mmml/source_position.hpp"

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

struct AST_Instruction {
    AST_Instruction_Type type;
    std::size_t n;
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
