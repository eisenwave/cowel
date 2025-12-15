#ifndef COWEL_MATH_HPP
#define COWEL_MATH_HPP

#include <cmath>

#include "cowel/fwd.hpp"

namespace cowel {

// https://github.com/eisenwave/integer-division

[[nodiscard]]
constexpr Integer div_to_pos_inf(Integer x, Integer y)
{
    const bool quotient_positive = (x ^ y) >= 0;
    const bool adjust = (x % y != 0) & quotient_positive;
    return (x / y) + Integer(adjust);
}

[[nodiscard]]
constexpr Integer rem_to_pos_inf(Integer x, Integer y)
{
    const bool quotient_positive = (x ^ y) >= 0;
    const bool adjust = (x % y != 0) & quotient_positive;
    return (x % y) - (Integer(adjust) * y);
}

[[nodiscard]]
constexpr Integer div_to_neg_inf(Integer x, Integer y) noexcept
{
    const bool quotient_negative = (x ^ y) < 0;
    const bool adjust = (x % y != 0) & quotient_negative;
    return (x / y) - Integer(adjust);
}

[[nodiscard]]
constexpr Integer rem_to_neg_inf(Integer x, Integer y) noexcept
{
    const bool quotient_negative = (x ^ y) < 0;
    const bool adjust = (x % y != 0) & quotient_negative;
    return (x % y) + (Integer(adjust) * y);
}

/// @brief Computes `out = x + y` and returns `true`
/// if the result could not be exactly represented .
[[nodiscard]]
constexpr bool add_overflow(Uint128& out, Uint128 x, Uint128 y) noexcept
{
    return __builtin_add_overflow(x, y, &out);
}

/// @brief Computes `out = x * y` and returns `true`
/// if the result could not be exactly represented .
[[nodiscard]]
constexpr bool mul_overflow(Uint128& out, Uint128 x, unsigned long long y) noexcept
{
    return __builtin_mul_overflow(x, y, &out);
}

inline float roundeven(float x) noexcept
{
#if __has_builtin(__builtin_roundevenf)
    return __builtin_roundevenf(x);
#else
    return ::roundevenf(x);
#endif
}

inline double roundeven(double x) noexcept
{
#if __has_builtin(__builtin_roundeven)
    return __builtin_roundeven(x);
#else
    return ::roundeven(x);
#endif
}

inline float fminimum(float x, float y) noexcept
{
#if __has_builtin(__builtin_wasm_min_f32)
    return __builtin_wasm_min_f32(x, y);
#elif __has_builtin(__builtin_fminimumf)
    return __builtin_fminimumf(x, y);
#else
    return ::fminimumf(x, y);
#endif
}

inline double fminimum(double x, double y) noexcept
{
#if __has_builtin(__builtin_wasm_min_f64)
    return __builtin_wasm_min_f64(x, y);
#elif __has_builtin(__builtin_fminimum)
    return __builtin_fminimum(x, y);
#else
    return ::fminimum(x, y);
#endif
}

inline float fmaximum(float x, float y) noexcept
{
#if __has_builtin(__builtin_wasm_max_f32)
    return __builtin_wasm_max_f32(x, y);
#elif __has_builtin(__builtin_fmaximumf)
    return __builtin_fmaximumf(x, y);
#else
    return ::fmaximumf(x, y);
#endif
}

inline double fmaximum(double x, double y) noexcept
{
#if __has_builtin(__builtin_wasm_max_f64)
    return __builtin_wasm_max_f64(x, y);
#elif __has_builtin(__builtin_fmaximum)
    return __builtin_fmaximum(x, y);
#else
    return ::fmaximum(x, y);
#endif
}

} // namespace cowel

#endif
