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
#include "cowel/util/meta.hpp"
#include "cowel/util/static_string.hpp"

namespace cowel {

[[nodiscard]]
constexpr std::to_chars_result to_chars128(char* const first, char* const last, const Uint128 x)
{
    if (x <= std::uint64_t(-1)) {
        return std::to_chars(first, last, std::uint64_t(x));
    }

    /// The greatest power of 10 that fits into a 64-bit integer is 10^19.
    constexpr std::uint64_t exp10_19 = 10000000000000000000ull;
    constexpr int lower_max_digits = 19;

    const std::to_chars_result upper_result = to_chars128(first, last, x / exp10_19);
    if (upper_result.ec != std::errc {}) {
        return upper_result;
    }

    const std::to_chars_result lower_result
        = std::to_chars(upper_result.ptr, last, std::uint64_t(x % exp10_19));
    if (lower_result.ec != std::errc {}) {
        return lower_result;
    }
    const auto lower_length = lower_result.ptr - upper_result.ptr;

    // The remainder (lower part) is mathematically exactly 19 digits long,
    // and we have to zero-pad to the left if it is shorter
    // (because std::to_chars wouldn't give us the leading zeros we need).
    char* const result_end = upper_result.ptr + lower_max_digits;
    std::ranges::copy(upper_result.ptr, upper_result.ptr + lower_length, result_end - lower_length);
    std::ranges::fill_n(upper_result.ptr, lower_max_digits - lower_length, '0');

    return { result_end, std::errc {} };
}

[[nodiscard]]
constexpr std::to_chars_result to_chars128(char* const first, char* const last, const Int128 x)
{
    if (x >= 0) {
        return to_chars128(first, last, Uint128(x));
    }
    if (x >= std::numeric_limits<int64_t>::min()) {
        return std::to_chars(first, last, int64_t(x));
    }
    if (last - first < 2) {
        return { last, std::errc::value_too_large };
    }
    *first = '-';
    return to_chars128(first + 1, last, Uint128(-x));
}

template <typename Char, std::size_t capacity>
using Basic_Characters = Basic_Static_String<Char, capacity>;
template <std::size_t capacity>
using Characters = Static_String<capacity>;
template <std::size_t capacity>
using Characters8 = Static_String8<capacity>;

template <char_like Char = char, signed_or_unsigned T>
[[nodiscard]]
constexpr Basic_Characters<Char, std::numeric_limits<T>::digits + 1>
to_characters(const T& x, int base = 10, bool to_upper = false)
{
    COWEL_ASSERT(base >= 2 && base <= 36);

    if constexpr (std::is_same_v<Char, char>) {
        using array_type = typename decltype(to_characters<char>(x, base))::array_type;

        array_type chars {};
        auto* const buffer_start = chars.data();
        std::to_chars_result result;
        if constexpr (std::is_same_v<T, Int128> || std::is_same_v<T, Uint128>) {
            COWEL_ASSERT(base == 10); // Sorry, other bases not yet implemented :(
            result = to_chars128(buffer_start, buffer_start + chars.size(), x);
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

template <char_like Char = char, std::floating_point T>
[[nodiscard]]
Basic_Characters<Char, 128> to_characters(T x)
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
Basic_Characters<Char, 128> to_characters(T x, std::chars_format format)
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
Basic_Characters<Char, 128> to_characters(T x, std::chars_format format, int precision)
{
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
