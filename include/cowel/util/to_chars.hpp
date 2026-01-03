#ifndef COWEL_TO_CHARS_HPP
#define COWEL_TO_CHARS_HPP

#include <bit>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <limits>
#include <system_error>
#include <type_traits>

#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/fixed_string.hpp"
#include "cowel/util/meta.hpp"

namespace cowel {

[[nodiscard]]
std::to_chars_result to_chars128(char* first, char* last, Uint128 x, int base);

[[nodiscard]]
std::to_chars_result to_chars128(char* first, char* last, Int128 x, int base);

template <typename Char, std::size_t capacity>
using Basic_Characters = Basic_Fixed_String<Char, capacity>;
template <std::size_t capacity>
using Characters = Fixed_String<capacity>;
template <std::size_t capacity>
using Characters8 = Fixed_String8<capacity>;

template <signed_or_unsigned T>
inline constexpr std::size_t buffer_size_for_int
    = std::numeric_limits<std::make_unsigned_t<T>>::digits + 1;
template <>
inline constexpr std::size_t buffer_size_for_int<Int128> = 129;
template <>
inline constexpr std::size_t buffer_size_for_int<Uint128> = 129;

template <char_like Char = char, signed_or_unsigned T>
[[nodiscard]]
constexpr Basic_Characters<Char, buffer_size_for_int<T>>
to_characters(const T& x, int base = 10, bool to_upper = false)
{
    COWEL_ASSERT(base >= 2 && base <= 36);

    if constexpr (std::is_same_v<Char, char>) {
        using array_type = typename decltype(to_characters<char>(x, base, to_upper))::array_type;

        array_type chars {};
        auto* const buffer_start = chars.data();
        std::to_chars_result result;
        if constexpr (std::is_same_v<T, Int128> || std::is_same_v<T, Uint128>) {
            result = to_chars128(buffer_start, buffer_start + chars.size(), x, base);
        }
        else {
            result = std::to_chars(buffer_start, buffer_start + chars.size(), x, base);
        }
        COWEL_ASSERT(result.ec == std::errc {});
        const auto result_length = std::size_t(result.ptr - buffer_start);
        if (to_upper) {
            for (char& c : chars) {
                c = char(to_ascii_upper(char8_t(c)));
            }
        }

        return { chars, result_length };
    }
    else {
        using result_type = decltype(to_characters<Char>(x, base, to_upper));
        const auto char_result = to_characters<char>(x, base, to_upper);
        return std::bit_cast<result_type>(char_result);
    }
}

// Source: these numbers were revealed to me in a dream.

/// @brief A pessimistic buffer size necessary to hold a double-precision
/// floating-point number in scientific notation.
/// This includes 17 significant digits as well as some fluff,
// such as the radix point and exponent.
constexpr std::size_t buffer_size_for_double_scientific = 32;

/// @brief A pessimistic buffer size necessary to hold a double-precision
/// floating-point number in fixed notation.
/// There can be extreme cases like one ulp,
/// where there are hundreds of digits.
constexpr std::size_t buffer_size_for_double_fixed = 512;

template <char_like Char = char, std::floating_point T>
[[nodiscard]]
Basic_Characters<Char, buffer_size_for_double_scientific> to_characters(T x)
{
    if constexpr (std::is_same_v<Char, char>) {
        using array_type = typename decltype(to_characters<char>(x))::array_type;

        array_type chars {};
        auto* const buffer_start = chars.data();
        const auto result = std::to_chars(buffer_start, buffer_start + chars.size(), x);
        COWEL_ASSERT(result.ec == std::errc {});
        const auto result_length = std::size_t(result.ptr - buffer_start);

        return { chars, result_length };
    }
    else {
        using result_type = decltype(to_characters<Char>(x));
        const auto char_result = to_characters<char>(x);
        return std::bit_cast<result_type>(char_result);
    }
}

template <char_like Char = char, std::floating_point T>
[[nodiscard]]
Basic_Characters<Char, buffer_size_for_double_fixed> to_characters(T x, std::chars_format format)
{
    if constexpr (std::is_same_v<Char, char>) {
        using array_type = typename decltype(to_characters<char>(x, format))::array_type;

        array_type chars {};
        auto* const buffer_start = chars.data();
        const auto result = std::to_chars(buffer_start, buffer_start + chars.size(), x, format);
        COWEL_ASSERT(result.ec == std::errc {});
        const auto result_length = std::size_t(result.ptr - buffer_start);

        return { chars, result_length };
    }
    else {
        using result_type = decltype(to_characters<Char>(x, format));
        const auto char_result = to_characters<char>(x, format);
        return std::bit_cast<result_type>(char_result);
    }
}

template <char_like Char = char, std::floating_point T>
[[nodiscard]]
Basic_Characters<Char, buffer_size_for_double_fixed>
to_characters(T x, std::chars_format format, int precision)
{
    COWEL_ASSERT(precision <= 400);
    if constexpr (std::is_same_v<Char, char>) {
        using array_type = typename decltype(to_characters<char>(x, format, precision))::array_type;

        array_type chars {};
        auto* const buffer_start = chars.data();
        const auto result = std::to_chars(buffer_start, buffer_start + chars.size(), x);
        COWEL_ASSERT(result.ec == std::errc {});
        const auto result_length = std::size_t(result.ptr - buffer_start);

        return { chars, result_length };
    }
    else {
        using result_type = decltype(to_characters<Char>(x, format, precision));
        const auto char_result = to_characters<char>(x, format, precision);
        return std::bit_cast<result_type>(char_result);
    }
}

template <signed_or_unsigned T>
[[nodiscard]]
constexpr auto to_characters8(const T& x, int base = 10, bool to_upper = false)
    -> decltype(to_characters<char8_t, T>(x, base, to_upper))
{
    return to_characters<char8_t>(x, base, to_upper);
}

template <no_cv_floating T>
[[nodiscard]]
constexpr auto to_characters8(T x) -> decltype(to_characters<char8_t, T>(x))
{
    return to_characters<char8_t, T>(x);
}

template <no_cv_floating T>
[[nodiscard]]
constexpr auto to_characters8(T x, std::chars_format format)
    -> decltype(to_characters<char8_t, T>(x, format))
{
    return to_characters<char8_t, T>(x, format);
}

template <no_cv_floating T>
[[nodiscard]]
constexpr auto to_characters8(T x, std::chars_format format, int precision)
    -> decltype(to_characters<char8_t, T>(x, format, precision))
{
    return to_characters<char8_t, T>(x, format, precision);
}

} // namespace cowel

#endif
