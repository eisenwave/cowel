#ifndef COWEL_FROM_CHARS_HPP
#define COWEL_FROM_CHARS_HPP

#include <charconv>
#include <cmath>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>

#include "cowel/util/meta.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/settings.hpp"

namespace cowel {

// INTEGRAL ====================================================================

/// @brief Implements the interface of `to_chars` for decimal input of 128-bit integers.
/// In the "happy case" of having at most 19 digits,
/// this simply calls `std::from_chars` for 64-bit integers.
/// In the worst case, three such 64-bit calls are needed,
/// handling 19 digits at a time, with 39 decimal digits being the maximum for 128-bit.
[[nodiscard]]
constexpr std::from_chars_result
from_chars128(const char* const first, const char* const last, Uint128& out)
{
    COWEL_DEBUG_ASSERT(first);
    COWEL_DEBUG_ASSERT(last);

    if (first == last) {
        return { last, std::errc::invalid_argument };
    }

    constexpr std::uint64_t exp10_19 = 10000000000000000000ull;
    constexpr std::ptrdiff_t max_lower_length = 19;
    const auto lower_length = std::min(last - first, max_lower_length);
    const char* const lower_begin = last - lower_length;

    std::uint64_t lower {};
    const std::from_chars_result lower_result = std::from_chars(lower_begin, last, lower);

    if (last - first <= max_lower_length || lower_result.ec != std::errc {}) {
        out = lower;
        return lower_result;
    }

    Uint128 upper {};
    const std::from_chars_result upper_result = from_chars128(first, lower_begin, upper);
    out = upper * exp10_19 + lower;

    return upper_result;
}

[[nodiscard]]
constexpr std::from_chars_result
from_chars128(const char* const first, const char* const last, Int128& out)
{
    COWEL_DEBUG_ASSERT(first);
    COWEL_DEBUG_ASSERT(last);

    if (first == last) {
        return { last, std::errc::invalid_argument };
    }
    if (last - first <= 19) {
        std::int64_t x {};
        const std::from_chars_result result = std::from_chars(first, last, x);
        out = x;
        return result;
    }
    if (*first != '-') {
        Uint128 x {};
        const std::from_chars_result result = from_chars128(first, last, x);
        if (x >> 127) {
            return { result.ptr, std::errc::value_too_large };
        }
        out = Int128(x);
        return result;
    }
    constexpr auto max_u128 = Uint128 { 1 } << 127;
    Uint128 x {};
    const std::from_chars_result result = from_chars128(first + 1, last, x);
    if (x > max_u128) {
        return { result.ptr, std::errc::value_too_large };
    }
    out = -Int128(x);
    return result;
}

template <signed_or_unsigned T>
[[nodiscard]]
constexpr std::from_chars_result from_characters(std::string_view sv, T& out, int base = 10)
{
    if constexpr (std::is_same_v<T, Int128> || std::is_same_v<T, Uint128>) {
        COWEL_ASSERT(base == 10); // Sorry, other bases not yet implemented :(
        return from_chars128(sv.data(), sv.data() + sv.size(), out);
    }
    else {
        return std::from_chars(sv.data(), sv.data() + sv.size(), out, base);
    }
}

template <signed_or_unsigned T>
[[nodiscard]]
constexpr std::optional<T> from_characters(std::string_view sv, int base = 10)
{
    std::optional<T> result;
    if (auto r = from_characters(sv, result.emplace(), base); r.ec != std::errc {}) {
        result.reset();
    }
    return result;
}

template <signed_or_unsigned T>
[[nodiscard]]
inline std::from_chars_result from_characters(std::u8string_view sv, T& out, int base = 10)
{
    return from_characters(as_string_view(sv), out, base);
}

template <signed_or_unsigned T>
[[nodiscard]]
std::optional<T> from_characters(std::u8string_view sv, int base = 10)
{
    return from_characters<T>(as_string_view(sv), base);
}

// FLOATING POINT ==============================================================

namespace detail {

inline void str_to_float(const char* str, char** str_end, float& result)
{
    result = std::strtof(str, str_end);
}
inline void str_to_float(const char* str, char** str_end, double& result)
{
    result = std::strtod(str, str_end);
}
inline void str_to_float(const char* str, char** str_end, long double& result)
{
    result = std::strtold(str, str_end);
}

} // namespace detail

template <no_cv_floating T>
[[nodiscard]]
std::from_chars_result
from_characters(std::string_view sv, T& out, std::chars_format fmt = std::chars_format::general)
{
#ifdef __GLIBCXX__
    out = T { 0 };
#endif
    const std::from_chars_result result
        = std::from_chars(sv.data(), sv.data() + sv.size(), out, fmt);
#ifdef __GLIBCXX__
    // This is a workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=123078
    // std::from_chars doesn't handle underflow properly.
    if (result.ec == std::errc {} && sv.starts_with('-')) {
        out = std::copysign(out, T { -1 });
    }
#endif
    return result;
}

template <no_cv_floating T>
[[nodiscard]]
Result<T, std::errc>
from_characters(std::string_view sv, std::chars_format fmt = std::chars_format::general)
{
    T result;
    if (auto r = from_characters(sv, result, fmt); r.ec != std::errc {}) {
        return r.ec;
    }
    return result;
}

template <no_cv_floating T>
[[nodiscard]]
inline std::from_chars_result
from_characters(std::u8string_view sv, T& out, std::chars_format fmt = std::chars_format::general)
{
    return from_characters(as_string_view(sv), out, fmt);
}

template <no_cv_floating T>
[[nodiscard]]
Result<T, std::errc>
from_characters(std::u8string_view sv, std::chars_format fmt = std::chars_format::general)
{
    return from_characters<T>(as_string_view(sv), fmt);
}

/// @brief Like `from_chars`,
/// but silently accepts values which are out of range and treats them as infinity.
template <no_cv_floating T>
[[nodiscard]]
std::optional<T>
from_characters_or_inf(std::string_view sv, std::chars_format fmt = std::chars_format::general)
{
    Result<T, std::errc> result = from_characters<T>(sv, fmt);
    if (!result && result.error() == std::errc::result_out_of_range) {
        const T inf = std::numeric_limits<T>::infinity();
        return sv.starts_with(u8'-') ? -inf : inf;
    }
    return *result;
}

/// @brief Like `from_chars`,
/// but silently accepts values which are out of range and treats them as infinity.
template <no_cv_floating T>
[[nodiscard]]
std::optional<T>
from_characters_or_inf(std::u8string_view sv, std::chars_format fmt = std::chars_format::general)
{
    return from_characters_or_inf<T>(as_string_view(sv), fmt);
}

} // namespace cowel

#endif
