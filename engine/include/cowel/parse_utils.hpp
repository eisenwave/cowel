#ifndef BIT_MANIPULATION_PARSE_HPP
#define BIT_MANIPULATION_PARSE_HPP

#include <cstddef>
#include <string_view>

namespace cowel {

struct Blank_Line {
    std::size_t begin;
    std::size_t length;

    [[nodiscard]]
    constexpr explicit operator bool() const
    {
        return length != 0;
    }

    [[nodiscard]]
    friend constexpr bool operator==(Blank_Line, Blank_Line)
        = default;
};

enum struct Blank_Line_Initial_State : bool {
    /// @brief The given `str` is on a new line,
    /// possibly at the start of the file.
    normal,
    /// @brief The given `str` is prefixed by other characters on the same line.
    /// This means that the next newline is not considered to begin a blank line sequence,
    /// but ends the current line.
    middle,
};

/// @brief Returns a `Blank_Line` where `begin` is the index of the first whitespace character
/// that is part of the blank line sequence,
/// and where `length` is the length of the blank line sequence, in code units.
/// The last character in the sequence is always `\\n`.
///
/// Note that the terminating whitespace of the previous line
/// is not considered to be part of the blank line.
/// For example, in `"first\\n\\t\\t\\n\\n second"`,
/// the blank line sequence consists of `"\\t\\t\\n\\n"`.
[[nodiscard]]
Blank_Line find_blank_line_sequence(
    std::u8string_view str,
    Blank_Line_Initial_State initial_state = Blank_Line_Initial_State::normal
) noexcept;

/// @brief Matches as many digits as possible, in a base of choice.
/// For bases above 10, lower and upper case characters are permitted.
/// @param str the string with digits at the beginning
/// @param base in range [2, 16]
/// @return The number of digits that belong to a numeric literal of the given base.
[[nodiscard]]
std::size_t match_digits(std::u8string_view str, int base);

} // namespace cowel

#endif
