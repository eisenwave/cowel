#ifndef BIT_MANIPULATION_PARSE_HPP
#define BIT_MANIPULATION_PARSE_HPP

#include <concepts>
#include <optional>
#include <string_view>

#include "mmml/util/chars.hpp"
#include "mmml/util/result.hpp"

#include "mmml/fwd.hpp"

namespace mmml {

enum struct Literal_Type : Default_Underlying {
    binary,
    octal,
    decimal,
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
    Literal_Type type;

    [[nodiscard]] operator bool() const
    {
        return status == Literal_Match_Status::ok;
    }
};

struct Text_Match {
    std::size_t length;
    bool is_terminated;
};

/// @brief Matches a C99-style line-comment.
/// @param str the string
/// @return The match or `std::nullopt`.
[[nodiscard]]
std::optional<Text_Match> match_line_comment(std::string_view str) noexcept;

/// @brief Matches a C89-style block comment.
/// @param str the string
/// @return The match or `std::nullopt`.
[[nodiscard]]
std::optional<Text_Match> match_block_comment(std::string_view str) noexcept;

/// @brief Matches a simple string literal, which is a sequence of characters beginning and ending
/// with `"`.
/// However, "escaped" `\"`, i.e. quotes preceded by a backslash do not end the string,
/// and `\\` correspond to a literal backwards slash within the string.
/// @param str the string
/// @return The match or `std::nullopt`.
[[nodiscard]]
std::optional<Text_Match> match_string_literal(std::string_view str) noexcept;

/// @brief Matches as many digits as possible, in a base of choice.
/// For bases above 10, lower and upper case characters are permitted.
/// @param str the string with digits at the beginning
/// @param base in range [2, 16]
/// @return The number of digits that belong to a numeric literal of the given base.
[[nodiscard]]
std::size_t match_digits(std::string_view str, int base);

/// @brief Matches leading whitespace.
/// @param str the string
/// @return The number of leading whitespace characters.
[[nodiscard]]
inline std::size_t match_whitespace(std::string_view str) noexcept
{
    return std::min(str.find_first_not_of(" \t\r\n"), str.length());
}

/// @brief Matches a literal at the beginning of the given string.
/// This includes any prefix such as `0x`, `0b`, or `0` and all the following digits.
/// @param str the string which may contain a literal at the start
/// @return The match or an error.
[[nodiscard]]
Literal_Match_Result match_integer_literal(std::string_view str) noexcept;

/// @brief Like `parse_integer_literal`, but does not permit negative numbers and results
/// in an unsigned integer.
/// @param str the string containing the prefix and literal digits
/// @return The parsed number.
[[nodiscard]]
std::optional<unsigned long long> parse_uinteger_literal(std::string_view str) noexcept;

/// @brief Converts a literal string to an signed integer.
/// The sign of the integer is based on a leading `-` character.
/// The base of the literal is automatically detected based on prefix:
/// - `0b` for binary
/// - `0` for octal
/// - `0x` for hexadecimal
/// - otherwise decimal
/// @param str the string containing the prefix and literal digits
/// @return The parsed number.
[[nodiscard]]
std::optional<long long> parse_integer_literal(std::string_view str) noexcept;

} // namespace mmml

#endif
