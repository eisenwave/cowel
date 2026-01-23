#ifndef COWEL_MATH_HPP
#define COWEL_MATH_HPP

#include <bit>
#include <cmath>

#include "cowel/fwd.hpp"
#include "cowel/settings.hpp"

namespace cowel {

/// @brief Specifies the rounding mode for integer division.
enum struct Div_Rounding : Default_Underlying {
    /// @brief Rounding toward zero.
    to_zero,
    /// @brief Rounding toward positive infinity (i.e. "ceil").
    to_pos_inf,
    /// @brief Rounding toward negative infinity (i.e. "floor").
    to_neg_inf,
};

// https://github.com/eisenwave/integer-division

[[nodiscard]]
constexpr Int128 div_to_pos_inf(Int128 x, Int128 y)
{
    const bool quotient_positive = (x ^ y) >= 0;
    const bool adjust = (x % y != 0) & quotient_positive;
    return (x / y) + Int128(adjust);
}

[[nodiscard]]
constexpr Int128 rem_to_pos_inf(Int128 x, Int128 y)
{
    const bool quotient_positive = (x ^ y) >= 0;
    const bool adjust = (x % y != 0) & quotient_positive;
    return (x % y) - (Int128(adjust) * y);
}

[[nodiscard]]
constexpr Int128 div_to_neg_inf(Int128 x, Int128 y)
{
    const bool quotient_negative = (x ^ y) < 0;
    const bool adjust = (x % y != 0) & quotient_negative;
    return (x / y) - Int128(adjust);
}

[[nodiscard]]
constexpr Int128 rem_to_neg_inf(Int128 x, Int128 y)
{
    const bool quotient_negative = (x ^ y) < 0;
    const bool adjust = (x % y != 0) & quotient_negative;
    return (x % y) + (Int128(adjust) * y);
}

[[nodiscard]]
constexpr int countl_zero(const Uint128 x) noexcept
{
    const int hi = std::countl_zero(Uint64(x >> 64));
    if (hi != 64) {
        return hi;
    }
    return 64 + std::countl_zero(Uint64(x));
}

[[nodiscard]]
constexpr int countl_one(const Uint128 x) noexcept
{
    const int hi = std::countl_one(Uint64(x >> 64));
    if (hi != 64) {
        return hi;
    }
    return 64 + std::countl_one(Uint64(x));
}

/// @brief Returns the width of the smallest hypothetical integer in two's complement
/// representation that can fit the value.
/// In other words, the smallest `N` for which `_BitInt(N)` can fit this value.
/// Mathematically, this is `floor(log2(x)) + 1` for positive numbers
/// and `floor(log2(-x - 1)) + 1` for negative numbers,
/// where `log2` is the binary logarithm with `log2(0) == 0`.
[[nodiscard]]
constexpr int twos_width(const Int128 x) noexcept
{
    return x >= 0 ? 129 - countl_zero(Uint128(x)) //
                  : 129 - countl_one(Uint128(x));
}

/// @brief Returns the width of the smallest hypothetical integer in one's complement
/// representation that can fit the value.
/// Mathematically, this is `floor(log2(abs(x))) + 1`,
/// where `log2` is the binary logarithm with `log2(0) == 0`.
[[nodiscard]]
constexpr int ones_width(const Int128 x) noexcept
{
    return 129 - countl_zero(x >= 0 ? Uint128(x) : -Uint128(x));
}

/// @brief Computes `out = x + y` and returns `true`
/// if the result could not be exactly represented .
[[nodiscard]]
constexpr bool add_overflow(Uint128& out, const Uint128 x, const Uint128 y) noexcept
{
    return __builtin_add_overflow(x, y, &out);
}

/// @brief Computes `out = x + y` and returns `true`
/// if the result could not be exactly represented .
[[nodiscard]]
constexpr bool add_overflow(Int128& out, const Int128 x, const Int128 y) noexcept
{
    return __builtin_add_overflow(x, y, &out);
}

/// @brief Computes `out = x - y` and returns `true`
/// if the result could not be exactly represented .
[[nodiscard]]
constexpr bool sub_overflow(Uint128& out, const Uint128 x, const Uint128 y) noexcept
{
    return __builtin_sub_overflow(x, y, &out);
}

/// @brief Computes `out = x - y` and returns `true`
/// if the result could not be exactly represented .
[[nodiscard]]
constexpr bool sub_overflow(Int128& out, const Int128 x, const Int128 y) noexcept
{
    return __builtin_sub_overflow(x, y, &out);
}

/// @brief Computes `out = x * y` and returns `true`
/// if the result could not be exactly represented .
[[nodiscard]]
constexpr bool mul_overflow(Uint128& out, Uint128 x, unsigned long long y) noexcept
{
    return __builtin_mul_overflow(x, y, &out);
}

/// @brief Computes `out = x * y` and returns `true`
/// if the result could not be exactly represented .
[[nodiscard]]
constexpr bool mul_overflow(Int128& out, const Int128 x, const Int128 y) noexcept
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
