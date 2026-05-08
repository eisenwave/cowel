#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/code_point_names.hpp"
#include "cowel/util/from_chars.hpp"

#include "cowel/fwd.hpp"

#include "cowel/syntax/parse_utils.hpp"

namespace cowel {

Blank_Line
// NOLINTNEXTLINE(bugprone-exception-escape)
find_blank_line_sequence(std::u8string_view str, Blank_Line_Initial_State initial_state) noexcept
{
    enum struct State : Default_Underlying {
        /// @brief We are at the start of a line.
        maybe_blank,
        /// @brief There are non-whitespace characters on this line.
        not_blank,
        /// @brief At least one line is blank.
        blank,
    };
    static_assert(static_cast<State>(Blank_Line_Initial_State::normal) == State::maybe_blank);
    static_assert(static_cast<State>(Blank_Line_Initial_State::middle) == State::not_blank);
    auto state = static_cast<State>(initial_state);

    std::size_t blank_begin = 0;
    // initialized to suppress false positive GCC warning
    std::size_t blank_end = -1uz;

    for (std::size_t i = 0; i < str.size(); ++i) {
        switch (state) {
        case State::maybe_blank: {
            if (str[i] == u8'\n') {
                state = State::blank;
                blank_end = i + 1;
            }
            else if (!is_html_whitespace(str[i])) {
                state = State::not_blank;
            }
            continue;
        }
        case State::not_blank: {
            if (str[i] == u8'\n') {
                state = State::maybe_blank;
                blank_begin = i + 1;
            }
            continue;
        }
        case State::blank: {
            if (str[i] == u8'\n') {
                blank_end = i + 1;
            }
            else if (!is_html_whitespace(str[i])) {
                COWEL_DEBUG_ASSERT(blank_end != -1uz);
                return { .begin = blank_begin, .length = blank_end - blank_begin };
            }
            continue;
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid state");
    }

    static_assert(!Blank_Line {}, "A value-initialized Blank_Line should be falsy");
    if (state == State::blank) {
        return { .begin = blank_begin, .length = str.size() - blank_begin };
    }
    return {};
}

std::size_t match_digits(std::u8string_view str, int base)
{
    COWEL_ASSERT((base >= 2 && base <= 10) || base == 16);
    static constexpr std::u8string_view hexadecimal_digits = u8"0123456789abcdefABCDEF";

    const std::u8string_view digits
        = base == 16 ? hexadecimal_digits : hexadecimal_digits.substr(0, std::size_t(base));

    // std::min covers the case of std::u8string_view::npos
    return std::min(str.find_first_not_of(digits), str.size());
}

Result<char32_t, Expand_Escape_Error_Code> unsafe_expand_escape(const std::u8string_view escape)
{
    COWEL_ASSERT(!escape.empty());
    COWEL_DEBUG_ASSERT(is_cowel_escapeable(escape[0]));
    COWEL_DEBUG_ASSERT(escape[0] != u8'\\' || escape == u8"\\");

    switch (escape[0]) {
    case u8'\r':
    case u8'\n': {
        return Expand_Escape_Error_Code::empty;
    }
    case u8'+': {
        const std::u8string_view digits = escape.substr(1);
        std::uint32_t parsed = 0;
        const auto [ptr, ec] = from_characters(digits, parsed, 16);
        COWEL_ASSERT(ec == std::errc {});
        COWEL_ASSERT(ptr == as_string_view(digits).data() + digits.size());
        const char32_t code_point = parsed;
        if (!is_scalar_value(char32_t(code_point))) {
            return Expand_Escape_Error_Code::nonscalar;
        }
        return parsed;
    }
    case u8'\'': {
        const std::u8string_view name = escape.substr(1, escape.length() - 2);
        const char32_t code_point = code_point_by_name(name);
        if (code_point == char32_t(-1)) {
            return Expand_Escape_Error_Code::bad_name;
        }
        // Nonscalars don't have code point names.
        COWEL_DEBUG_ASSERT(is_scalar_value(code_point));
        return code_point;
    }
    default: break;
    }
    COWEL_DEBUG_ASSERT(is_ascii(escape[0]));
    return char32_t(escape[0]);
}

} // namespace cowel
