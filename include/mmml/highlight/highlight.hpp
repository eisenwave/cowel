#ifndef MMML_HIGHLIGHT_TOKEN_HPP
#define MMML_HIGHLIGHT_TOKEN_HPP

#include <string_view>
#include <vector>

#include "mmml/fwd.hpp"

namespace mmml {

enum struct Highlight_Type : Default_Underlying {
    /// @brief Attribute name in markup.
    attribute,
    /// @brief Comment.
    comment,
    /// @brief Delimiters of comments (like `//`).
    comment_delimiter,
    /// @brief Deletion (in `diff`).
    deletion,
    /// @brief Identifier.
    identifier,
    /// @brief Insertion (in `diff`).
    insertion,
    /// @brief Keyword.
    keyword,
    /// @brief Keywords for control flow, like `if`.
    keyword_control,
    /// @brief Keyword for types, like `int`.
    keyword_type,
    /// @brief `true`, `false`, and other such keywords.
    keyword_boolean,
    /// @brief Keyword for non-boolean constants, like `this`, `nullptr`.
    keyword_constant,
    /// @brief Meta-instructions, like C++ preprocessor directives.
    meta,
    /// @brief Numeric literals.
    number,
    /// @brief Unimportant special characters, like punctuation.
    symbol_other,
    /// @brief Normal special characters, like operators.
    symbol_normal,
    /// @brief Important special characters, like braces.
    symbol_important,
    /// @brief String literal.
    string,
    /// @brief Escape sequence in strings, like `\\n`.
    string_escape,
    /// @brief Tag name in markup.
    tag,
};

bool highlight_mmml(
    std::pmr::vector<Highlight_Span>& out,
    std::u8string_view source,
    std::pmr::memory_resource* memory
);
bool highlight_cpp(
    std::pmr::vector<Highlight_Span>& out,
    std::u8string_view source,
    std::pmr::memory_resource* memory
);

} // namespace mmml

#endif
