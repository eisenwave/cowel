#ifndef MMML_HIGHLIGHT_TYPE_HPP
#define MMML_HIGHLIGHT_TYPE_HPP

#include <string_view>

#include "mmml/util/assert.hpp"

#include "mmml/fwd.hpp"

namespace mmml {

#define MMML_HIGHLIGHT_TYPE_ENUM_DATA(F)                                                           \
    /* Attribute name in markup. */                                                                \
    F(attribute, "attr")                                                                           \
    /* Comment. */                                                                                 \
    F(comment, "cmt")                                                                              \
    /* Delimiters of comments (like `//`). */                                                      \
    F(comment_delimiter, "cmt.delim")                                                              \
    /* Deletion (in `diff`). */                                                                    \
    F(deletion, "del")                                                                             \
    /* Identifier. */                                                                              \
    F(identifier, "id")                                                                            \
    /* Insertion (in `diff`). */                                                                   \
    F(insertion, "ins")                                                                            \
    /* Keyword. */                                                                                 \
    F(keyword, "key")                                                                              \
    /* Keywords for control flow, like `if`. */                                                    \
    F(keyword_control, "key.ctrl")                                                                 \
    /* Keyword for types, like `int`. */                                                           \
    F(keyword_type, "key.type")                                                                    \
    /* `true`, `false`, and other such keywords. */                                                \
    F(keyword_boolean, "key.bool")                                                                 \
    /* Keyword for non-boolean constants, like `this`, `nullptr`. */                               \
    F(keyword_constant, "key.const")                                                               \
    /* Meta-instructions, like C++ preprocessor directives. */                                     \
    F(meta, "meta")                                                                                \
    /* Numeric literals. */                                                                        \
    F(number, "num")                                                                               \
    /* Special characters, like operators. */                                                      \
    F(symbol, "sym")                                                                               \
    /* Unimportant special characters, like punctuation. */                                        \
    F(symbol_other, "sym.etc")                                                                     \
    /* Important special characters, like braces. */                                               \
    F(symbol_important, "sym.imp")                                                                 \
    /* String literal. */                                                                          \
    F(string, "str")                                                                               \
    /* Escape sequence in strings, like `\\n`. */                                                  \
    F(string_escape, "str.esc")                                                                    \
    /* Tag name in markup. */                                                                      \
    F(tag, "tag")

#define MMML_HIGHLIGHT_TYPE_ENUMERATOR(id, css) id,

enum struct Highlight_Type : Default_Underlying {
    MMML_HIGHLIGHT_TYPE_ENUM_DATA(MMML_HIGHLIGHT_TYPE_ENUMERATOR)
};

namespace detail {

#define MMML_HIGHLIGHT_TYPE_U8_CSS(id, css) u8##css,

inline constexpr std::u8string_view highlight_type_css[] {
    MMML_HIGHLIGHT_TYPE_ENUM_DATA(MMML_HIGHLIGHT_TYPE_U8_CSS)
};

} // namespace detail

/// @brief Returns the value of the CSS `data-h` attribute
/// that spans highlighted with the given `type` should have.
[[nodiscard]]
constexpr std::u8string_view highlight_type_css(Highlight_Type type)
{
    const auto index = std::size_t(type);
    MMML_ASSERT(index < std::size(detail::highlight_type_css));
    return detail::highlight_type_css[index];
}

} // namespace mmml

#endif
