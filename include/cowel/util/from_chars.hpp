#ifndef COWEL_FROM_CHARS_HPP
#define COWEL_FROM_CHARS_HPP

#include <charconv>
#include <cmath>
#include <concepts>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>

#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"

namespace cowel {

// INTEGRAL ====================================================================

template <std::integral T>
[[nodiscard]]
constexpr std::from_chars_result from_characters(std::string_view sv, T& out, int base = 10)
{
    return std::from_chars(sv.data(), sv.data() + sv.size(), out, base);
}

template <std::integral T>
[[nodiscard]]
constexpr std::optional<T> from_characters(std::string_view sv, int base = 10)
{
    std::optional<T> result;
    if (auto r = std::from_chars(sv.data(), sv.data() + sv.size(), result.emplace(), base);
        r.ec != std::errc {}) {
        result.reset();
    }
    return result;
}

template <std::integral T>
[[nodiscard]]
inline std::from_chars_result from_characters(std::u8string_view sv, T& out, int base = 10)
{
    return from_characters(as_string_view(sv), out, base);
}

template <std::integral T>
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

template <std::floating_point T>
[[nodiscard]]
std::from_chars_result
from_characters(std::string_view sv, T& out, std::chars_format fmt = std::chars_format::general)
{
    // For a very long time, only libstdc++, not libc++ had floating-point support.
    // If need be, we emulate std::from_chars using std::strtod et al.
#ifdef _LIBCPP_VERSION
    if constexpr (!requires { std::from_chars(sv.data(), sv.data() + sv.size(), out, fmt); }) {
        std::string buffer(sv);
        COWEL_ASSERT(fmt == std::chars_format::general);
        char* end = nullptr;
        char** str_end = &end;
        detail::str_to_float(buffer.c_str(), str_end, out);
        if (*str_end == buffer.c_str()) {
            return { sv.data(), std::errc::invalid_argument };
        }
        if (std::isinf(out) && !buffer.starts_with("-I") && !buffer.starts_with("-i")
            && !buffer.starts_with("+I") && !buffer.starts_with("+i") && !buffer.starts_with('I')
            && !buffer.starts_with('i')) {
            return { sv.data(), std::errc::result_out_of_range };
        }
        const std::ptrdiff_t matched_length = end - buffer.data();
        return { sv.data() + matched_length, std::errc {} };
    }
    else
#endif
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
}

template <std::floating_point T>
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

template <std::floating_point T>
[[nodiscard]]
inline std::from_chars_result
from_characters(std::u8string_view sv, T& out, std::chars_format fmt = std::chars_format::general)
{
    return from_characters(as_string_view(sv), out, fmt);
}

template <std::floating_point T>
[[nodiscard]]
Result<T, std::errc>
from_characters(std::u8string_view sv, std::chars_format fmt = std::chars_format::general)
{
    return from_characters<T>(as_string_view(sv), fmt);
}

/// @brief Like `from_chars`,
/// but silently accepts values which are out of range and treats them as infinity.
template <std::floating_point T>
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
template <std::floating_point T>
[[nodiscard]]
std::optional<T>
from_characters_or_inf(std::u8string_view sv, std::chars_format fmt = std::chars_format::general)
{
    return from_characters_or_inf<T>(as_string_view(sv), fmt);
}

} // namespace cowel

#endif
