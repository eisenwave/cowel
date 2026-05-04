#ifndef COWEL_FROM_CHARS_HPP
#define COWEL_FROM_CHARS_HPP

#include <charconv>
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
std::from_chars_result
from_chars128(const char* first, const char* last, Uint128& out, int base = 10);

[[nodiscard]]
std::from_chars_result
from_chars128(const char* first, const char* last, Int128& out, int base = 10);

template <signed_or_unsigned T>
[[nodiscard]]
std::from_chars_result from_characters(std::string_view sv, T& out, int base = 10)
{
    if constexpr (std::is_same_v<T, Int128> || std::is_same_v<T, Uint128>) {
        return from_chars128(sv.data(), sv.data() + sv.size(), out, base);
    }
    else {
        return std::from_chars(sv.data(), sv.data() + sv.size(), out, base);
    }
}

template <signed_or_unsigned T>
[[nodiscard]]
std::optional<T> from_characters(std::string_view sv, int base = 10)
{
    std::optional<T> result;
    if (auto r = from_characters(sv, result.emplace(), base); r.ec != std::errc {}) {
        result.reset();
    }
    return result;
}

template <signed_or_unsigned T>
[[nodiscard]]
std::from_chars_result from_characters(std::u8string_view sv, T& out, int base = 10)
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

template <no_cv_floating T>
[[nodiscard]]
std::from_chars_result
from_characters(std::string_view sv, T& out, std::chars_format fmt = std::chars_format::general)
{
    return std::from_chars(sv.data(), sv.data() + sv.size(), out, fmt);
}

extern template std::from_chars_result from_characters(std::string_view, float&, std::chars_format);
extern template std::from_chars_result
from_characters(std::string_view, double&, std::chars_format);

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
