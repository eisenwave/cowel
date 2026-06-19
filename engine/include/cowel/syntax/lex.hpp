#ifndef COWEL_LEX_HPP
#define COWEL_LEX_HPP

#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/source_position.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

#define COWEL_TOKEN_KIND_ENUM_DATA(F)                                                              \
    F(equals, "ASSIGN", '=')                                                                       \
    F(asterisk, "ASTERISK", '*')                                                                   \
    F(binary_int, "BINARY-INT", '\0')                                                              \
    F(bitwise_not, "BITWISE-NOT", '~')                                                             \
    F(block_comment, "BLOCK-COMMENT", '\\')                                                        \
    F(block_text, "BLOCK-TEXT", '\0')                                                              \
    F(brace_left, "BRACE-LEFT", '{')                                                               \
    F(brace_right, "BRACE-RIGHT", '}')                                                             \
    F(comma, "COMMA", ',')                                                                         \
    F(decimal_float, "DECIMAL-FLOAT", '\0')                                                        \
    F(decimal_int, "DECIMAL-INT", '\0')                                                            \
    F(directive_splice_name, "DIRECTIVE-SPLICE-NAME", '\\')                                        \
    F(document_text, "DOCUMENT-TEXT", '\0')                                                        \
    F(ellipsis, "ELLIPSIS", '.')                                                                   \
    F(equals_equals, "EQUALS", '=')                                                                \
    F(error, "ERROR", '\0')                                                                        \
    F(escape, "ESCAPE", '\\')                                                                      \
    F(expression_line_splice, "EXPRESSION-LINE-SPLICE", '\\')                                      \
    F(expression_splice, "EXPRESSION-SPLICE", '\\')                                                \
    F(false_, "FALSE", 'f')                                                                        \
    F(greater_equal, "GREATER-EQUAL", '>')                                                         \
    F(greater_than, "GREATER-THAN", '>')                                                           \
    F(hexadecimal_int_literal, "HEXADECIMAL-INT", '\0')                                            \
    F(identifier, "IDENTIFIER", '\0')                                                              \
    F(infinity, "INFINITY", 'i')                                                                   \
    F(less_equal, "LESS-EQUAL", '<')                                                               \
    F(less_than, "LESS-THAN", '<')                                                                 \
    F(line_comment, "LINE-COMMENT", '\\')                                                          \
    F(line_terminator, "LINE-TERMINATOR", '\n')                                                    \
    F(logical_and, "LOGICAL-AND", '&')                                                             \
    F(logical_not, "LOGICAL-NOT", '!')                                                             \
    F(logical_or, "LOGICAL-OR", '|')                                                               \
    F(minus, "MINUS", '-')                                                                         \
    F(not_equals, "NOT-EQUALS", '!')                                                               \
    F(null, "NULL", 'n')                                                                           \
    F(octal_int, "OCTAL-INT", '\0')                                                                \
    F(parenthesis_left, "PARENTHESIS-LEFT", '(')                                                   \
    F(parenthesis_right, "PARENTHESIS-RIGHT", ')')                                                 \
    F(percent, "PERCENT", '%')                                                                     \
    F(plus, "PLUS", '+')                                                                           \
    F(quoted_string_text, "QUOTED-STRING-TEXT", '\0')                                              \
    F(reserved_escape, "RESERVED-ESCAPE", '\\')                                                    \
    F(reserved_number, "RESERVED-NUMBER", '\0')                                                    \
    F(slash, "SLASH", '/')                                                                         \
    F(string_quote, "STRING-QUOTE", '"')                                                           \
    F(true_, "TRUE", 't')                                                                          \
    F(unit, "UNIT", 'u')                                                                           \
    F(whitespace, "WHITESPACE", '\0')

#define COWEL_TOKEN_KIND_ENUMERATOR(id, name, first) id,

enum struct Token_Kind : Default_Underlying {
    COWEL_TOKEN_KIND_ENUM_DATA(COWEL_TOKEN_KIND_ENUMERATOR)
};

struct Token {
    static constexpr auto no_code_point = char32_t(-1);
    /// @brief The kind of token.
    Token_Kind kind;
    /// @brief For `ESCAPE-TOKEN`, the encoded code point,
    /// or `no_code_point` to indicate that there is no code point
    /// (in the case of whitespace escapes).
    char32_t code_point;
    /// @brief The location of the token in the lexed source.
    Source_Span location;
};

using Lex_Error_Consumer = Function_Ref<
    void(std::u8string_view id, const Source_Span& location, Char_Sequence8 message)>;

bool lex(
    std::pmr::vector<Token>& out, //
    std::u8string_view source,
    Lex_Error_Consumer on_error
);

} // namespace cowel

#endif
