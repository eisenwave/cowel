#ifndef COWEL_LEX_HPP
#define COWEL_LEX_HPP

#include <cstdint>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/function_ref.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

#define COWEL_TOKEN_KIND_ENUM_DATA(F)                                                              \
    F(binary_int, "BINARY-INT")                                                                    \
    F(block_comment, "BLOCK-COMMENT")                                                              \
    F(block_text, "BLOCK-TEXT")                                                                    \
    F(brace_left, "BRACE-LEFT")                                                                    \
    F(brace_right, "BRACE-RIGHT")                                                                  \
    F(comma, "COMMA")                                                                              \
    F(decimal_float, "DECIMAL-FLOAT")                                                              \
    F(decimal_int, "DECIMAL-INT")                                                                  \
    F(directive_splice_name, "DIRECTIVE-SPLICE-NAME")                                              \
    F(document_text, "DOCUMENT-TEXT")                                                              \
    F(ellipsis, "ELLIPSIS")                                                                        \
    F(equals, "EQUALS")                                                                            \
    F(error, "ERROR")                                                                              \
    F(escape, "ESCAPE")                                                                            \
    F(false_, "FALSE")                                                                             \
    F(hexadecimal_int_literal, "HEXADECIMAL-INT")                                                  \
    F(infinity, "INFINITY")                                                                        \
    F(line_comment, "LINE-COMMENT")                                                                \
    F(negative_infinity, "NEGATIVE-INFINITY")                                                      \
    F(null, "NULL")                                                                                \
    F(octal_int, "OCTAL-INT")                                                                      \
    F(parenthesis_left, "PARENTHESIS-LEFT")                                                        \
    F(parenthesis_right, "PARENTHESIS-RIGHT")                                                      \
    F(quoted_identifier, "QUOTED-IDENTIFIER")                                                      \
    F(quoted_string_text, "QUOTED-STRING-TEXT")                                                    \
    F(string_quote, "STRING-QUOTE")                                                                \
    F(true_, "TRUE")                                                                               \
    F(unit, "UNIT")                                                                                \
    F(unquoted_identifier, "UNQUOTED-IDENTIFIER")                                                  \
    F(whitespace, "WHITESPACE")

#define COWEL_TOKEN_KIND_ENUMERATOR(id, _) id,

enum struct Token_Kind : Default_Underlying {
    COWEL_TOKEN_KIND_ENUM_DATA(COWEL_TOKEN_KIND_ENUMERATOR)
};

struct Token {
    Token_Kind kind;
    std::uint32_t length;
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
