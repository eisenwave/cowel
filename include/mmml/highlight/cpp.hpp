#include <cstddef>
#include <optional>
#include <string_view>

#include "mmml/fwd.hpp"

namespace mmml::cpp {

/// @brief Matches zero or more characters for which `is_cpp_whitespace` is `true`.
[[nodiscard]]
std::size_t match_whitespace(std::u8string_view str);

/// @brief Matches zero or more characters for which `is_cpp_whitespace` is `false`.
[[nodiscard]]
std::size_t match_non_whitespace(std::u8string_view str);

struct Comment_Result {
    std::size_t length;
    bool is_terminated;

    [[nodiscard]]
    constexpr explicit operator bool() const noexcept
    {
        return length != 0;
    }
};

/// @brief Returns a match for a C99-style line comment at the start of `str`, if any.
/// If matched, the `length` of the result includes the terminating newline character.
[[nodiscard]]
Comment_Result match_line_comment(std::u8string_view str) noexcept;

/// @brief Returns a match for a C89-style block comment at the start of `str`, if any.
[[nodiscard]]
Comment_Result match_block_comment(std::u8string_view str) noexcept;

[[nodiscard]]
Comment_Result match_preprocessing_line(std::u8string_view str) noexcept;

/** @brief Type of https://eel.is/c++draft/lex.icon#nt:integer-literal */
enum struct Integer_Literal_Type : Default_Underlying {
    /** @brief *binary-literal* */
    binary,
    /** @brief *octal-literal* */
    octal,
    /** @brief *decimal-literal* */
    decimal,
    /** @brief *hexadecimal-literal* */
    hexadecimal,
};

enum struct Literal_Match_Status : Default_Underlying {
    /// @brief Successful match.
    ok,
    /// @brief The literal has no digits.
    no_digits,
    /// @brief The literal starts with an integer prefix `0x` or `0b`, but does not have any digits
    /// following it.
    no_digits_following_prefix
};

struct Literal_Match_Result {
    /// @brief The status of a literal match.
    /// If `status == ok`, matching succeeded.
    Literal_Match_Status status;
    /// @brief The length of the matched literal.
    /// If `status == ok`, `length` is the length of the matched literal.
    /// If `status == no_digits_following_prefix`, it is the length of the prefix.
    /// Otherwise, it is zero.
    std::size_t length;
    /// @brief The type of the matched literal.
    /// If `status == no_digits`, the value is value-initialized.
    Integer_Literal_Type type;

    [[nodiscard]] operator bool() const
    {
        return status == Literal_Match_Status::ok;
    }
};

/// @brief Matches a literal at the beginning of the given string.
/// This includes any prefix such as `0x`, `0b`, or `0` and all the following digits.
/// @param str the string which may contain a literal at the start
/// @return The match or an error.
[[nodiscard]]
Literal_Match_Result match_integer_literal(std::u8string_view str) noexcept;

/// @brief Matches the regex `\\.?[0-9]('?[0-9a-zA-Z_]|[eEpP][+-]|\\.)*`
/// at the start of `str`.
/// Returns `0` if it couldn't be matched.
///
/// A https://eel.is/c++draft/lex.ppnumber#nt:pp-number in C++ is a superset of
/// *integer-literal* and *floating-point-literal*,
/// and also includes malformed numbers like `1e+3p-55` that match neither of those,
/// but are still considered a single token.
///
/// pp-numbers are converted into a single token in phase
/// https://eel.is/c++draft/lex.phases#1.7
[[nodiscard]]
std::size_t match_pp_number(std::u8string_view str);

/// @brief Matches a C++
/// *[identifier](https://eel.is/c++draft/lex.name#nt:identifier)*
/// at the start of `str`
/// and returns its length.
/// If none could be matched, returns zero.
[[nodiscard]]
std::size_t match_identifier(std::u8string_view str);

struct Character_Literal_Result {
    std::size_t length;
    std::size_t encoding_prefix_length;
    bool terminated;

    [[nodiscard]]
    constexpr explicit operator bool() const noexcept
    {
        return length != 0;
    }
};

/// @brief Matches a C++
/// *[character-literal](https://eel.is/c++draft/lex#nt:character-literal)*
/// at the start of `str`.
/// Returns zero if noe could be matched.
[[nodiscard]]
Character_Literal_Result match_character_literal(std::u8string_view str);

struct String_Literal_Result {
    std::size_t length;
    std::size_t encoding_prefix_length;
    bool raw;
    bool terminated;

    [[nodiscard]]
    constexpr explicit operator bool() const noexcept
    {
        return length != 0;
    }
};

/// @brief Matches a C++
/// *[string-literal](https://eel.is/c++draft/lex#nt:string-literal)*
/// at the start of `str`.
/// Returns zero if noe could be matched.
[[nodiscard]]
String_Literal_Result match_string_literal(std::u8string_view str);

#define MMML_CPP_TOKEN_ENUM_DATA(F)                                                                \
    F(pound, "#")                                                                                  \
    F(pound_pound, "##")                                                                           \
    F(pound_alt, "%:")                                                                             \
    F(pound_pound_alt, "%:%:")                                                                     \
    F(left_brace, "{")                                                                             \
    F(right_brace, "}")                                                                            \
    F(left_square, "[")                                                                            \
    F(right_square, "]")                                                                           \
    F(left_parens, "(")                                                                            \
    F(right_parens, ")")                                                                           \
    F(left_brace_alt, "<%")                                                                        \
    F(right_brace_alt, "%>")                                                                       \
    F(left_square_alt, "<:")                                                                       \
    F(right_square_alt, ":>")                                                                      \
    F(semicolon, ";")                                                                              \
    F(colon, ":")                                                                                  \
    F(ellipsis, "...")                                                                             \
    F(question, "?")                                                                               \
    F(scope, "::")                                                                                 \
    F(dot, ".")                                                                                    \
    F(member_pointer_access, ".*")                                                                 \
    F(arrow, "->")                                                                                 \
    F(member_arrow_access, "->*")                                                                  \
    F(tilde, "~")                                                                                  \
    F(exclamation, "!")                                                                            \
    F(plus, "+")                                                                                   \
    F(minus, "-")                                                                                  \
    F(minus_eq, "-=")                                                                              \
    F(asterisk, "*")                                                                               \
    F(slash, "/")                                                                                  \
    F(percent, "%")                                                                                \
    F(caret, "^")                                                                                  \
    F(caret_caret, "^^")                                                                           \
    F(pipe, "|")                                                                                   \
    F(eq, "=")                                                                                     \
    F(plus_eq, "+=")                                                                               \
    F(asterisk_eq, "*=")                                                                           \
    F(slash_eq, "/=")                                                                              \
    F(percent_eq, "%=")                                                                            \
    F(amp, "&")                                                                                    \
    F(caret_eq, "^=")                                                                              \
    F(amp_eq, "&=")                                                                                \
    F(pipe_eq, "|=")                                                                               \
    F(eq_eq, "==")                                                                                 \
    F(exclamation_eq, "!=")                                                                        \
    F(less, "<")                                                                                   \
    F(greater, ">")                                                                                \
    F(less_eq, "<=")                                                                               \
    F(greater_eq, ">=")                                                                            \
    F(three_way, "<=>")                                                                            \
    F(amp_amp, "&&")                                                                               \
    F(pipe_pipe, "||")                                                                             \
    F(less_less, "<<")                                                                             \
    F(greater_greater, ">>")                                                                       \
    F(less_less_eq, "<<=")                                                                         \
    F(greater_greater_eq, ">>=")                                                                   \
    F(plus_plus, "++")                                                                             \
    F(minus_minus, "--")                                                                           \
    F(comma, ",")                                                                                  \
    F(keyword_and, "and")                                                                          \
    F(keyword_or, "or")                                                                            \
    F(keyword_xor, "xor")                                                                          \
    F(keyword_not, "not")                                                                          \
    F(keyword_bitand, "bitand")                                                                    \
    F(keyword_bitor, "bitor")                                                                      \
    F(keyword_compl, "compl")                                                                      \
    F(keyword_and_eq, "and_eq")                                                                    \
    F(keyword_or_eq, "or_eq")                                                                      \
    F(keyword_xor_eq, "xor_eq")                                                                    \
    F(keyword_not_eq, "not_eq")

#define MMML_CPP_TOKEN_ENUM_ENUMERATOR(id, code) id,

enum struct Cpp_Token_Type : Default_Underlying { //
    MMML_CPP_TOKEN_ENUM_DATA(MMML_CPP_TOKEN_ENUM_ENUMERATOR)
};

#define MMML_CPP_TOKEN_ENUM_CODE_CASE(id, code)                                                    \
    case Cpp_Token_Type::id: return u8##code;

/// @brief Returns the in-code representation of `type`.
/// For example, if `type` is `plus`, returns `"+"`.
/// If `type` is invalid, returns an empty string.
[[nodiscard]]
constexpr std::u8string_view cpp_token_type_code(Cpp_Token_Type type) noexcept
{
    switch (type) {
        MMML_CPP_TOKEN_ENUM_DATA(MMML_CPP_TOKEN_ENUM_CODE_CASE);
    default: return {};
    }
}

#define MMML_CPP_TOKEN_ENUM_LENGTH_CASE(id, code)                                                  \
    case Cpp_Token_Type::id: return u8##code##sv.size();

/// @brief Equivalent to `cpp_token_type_code(type).length()`.
[[nodiscard]]
constexpr std::size_t cpp_token_type_length(Cpp_Token_Type type) noexcept
{
    using namespace std::literals;
    switch (type) {
        MMML_CPP_TOKEN_ENUM_DATA(MMML_CPP_TOKEN_ENUM_LENGTH_CASE);
    default: return 0;
    }
}

/// @brief Matches a C++
/// *[preprocessing-op-or-punc](https://eel.is/c++draft/lex#nt:preprocessing-op-or-punc)*
/// at the start of `str`.
[[nodiscard]]
std::optional<Cpp_Token_Type> match_preprocessing_op_or_punc(std::u8string_view str);

} // namespace mmml::cpp
