// From: https://github.com/Eisenwave/cxx26-bit-permutations
#ifndef MMML_BIT_PERMUTATIONS_HPP
#define MMML_BIT_PERMUTATIONS_HPP

#include <bit>
#include <concepts>
#include <limits>
#include <version>

// DETECT GNU COMPILERS AND BUILTINS
// =================================

#ifdef __GNUC__
#define MMML_GNU __GNUC__

#ifndef MMML_DISABLE_BUILTINS
#if MMML_GNU >= 14
// GCC 14 in particular is quite important because it brings generic __builtins
#define MMML_BUILTIN_CLZG
#define MMML_BUILTIN_CTZG
#define MMML_BUILTIN_POPCOUNTG
#endif
#define MMML_BUILTIN_CLZ
#define MMML_BUILTIN_CTZ
#define MMML_BUILTIN_POPCOUNT
#define MMML_BUILTIN_BSWAP
#endif // MMML_DISABLE_BUILTINS

#endif // __GNUC__

// DETECT CLANG COMPILER AND BUILTINS
// ==================================

#ifdef __clang__
#define MMML_CLANG __clang_major__
#define MMML_BITINT

#if MMML_CLANG >= 16
#define CXX_BIT_PERMUTATIONS_BITINT_BEYOND_128
#endif

#ifndef MMML_DISABLE_BUILTINS
#define MMML_BUILTIN_BITREVERSE
#endif // MMML_DISABLE_BUILTINS

#endif // __clang__

// DETECT MICROSOFT COMPILER AND BUILTINS
// ======================================

#ifdef _MSC_VER
#define MMML_MSVC _MSC_VER

#ifndef MMML_DISABLE_BUILTINS
#define MMML_BUILTIN_LZCNT
#define MMML_BUILTIN_BSF
#define MMML_BUILTIN_BYTESWAP
#define MMML_BUILTIN_POPCNT
#endif // MMML_DISABLE_BUILTINS

#endif // _MSC_VER

// DETECT ARCHITECTURE
// ===================

#ifndef MMML_DISABLE_ARCH_INTRINSICS
#if defined(__x86_64__) || defined(_M_X64)
#define MMML_X86_64
#define MMML_X86
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#define MMML_X86
#endif

#if defined(_M_ARM) || defined(__arm__)
#define MMML_ARM
#endif

// DETECT INSTRUCTION SET FEATURES
// ===============================

#ifdef __BMI__
#define MMML_X86_BMI
#endif
#ifdef __BMI2__
#define MMML_X86_BMI2
#endif
#ifdef __PCLMUL__
#define MMML_X86_PCLMUL
#endif
#ifdef __POPCNT__
#define MMML_X86_POPCNT
#endif
#ifdef __ARM_FEATURE_SVE2
#define MMML_ARM_SVE2
#endif
#ifdef __ARM_FEATURE_SVE
#define MMML_ARM_SVE
#endif
#ifdef __ARM_FEATURE_SME
#define MMML_ARM_SME
#endif

// DEFINE INSTRUCTION SUPPORT BASED ON INSTRUCTION SET FEATURES
// ============================================================

#if defined(MMML_X86_BMI2)
#define MMML_X86_PDEP
#define MMML_X86_PEXT
#endif

#if defined(__ARM_FEATURE_SVE2)
#define MMML_ARM_BDEP
#define MMML_ARM_BEXT
#define MMML_ARM_BGRP
#endif

#if defined(MMML_ARM_SME) || defined(__ARM_FEATURE_SVE)
#define MMML_ARM_RBIT
#endif

#if defined(MMML_ARM_SME) || defined(__ARM_FEATURE_SVE)
// support for 64-bit PMUL is optional, so we will
// only use up to the 32-bit variant of this
#define MMML_ARM_PMUL
#endif

// DEFINE WHICH FUNCTIONS HAVE "FAST" SUPPORT

#if defined(MMML_ARM_RBIT)
#define MMML_FAST_REVERSE
#endif

#if defined(MMML_X86_PEXT) || defined(MMML_ARM_BEXT)
#define MMML_FAST_COMPRESS
#endif

#if defined(MMML_X86_PDEP) || defined(MMML_ARM_BDEP)
#define MMML_FAST_EXPAND
#endif

#if defined(MMML_X86_POPCNT) || defined(MMML_X86_BMI2) || defined(MMML_ARM_SVE)
#define MMML_FAST_POPCOUNT
#endif

// ARCHITECTURE INTRINSIC INCLUDES
// ===============================

#ifdef MMML_X86
#include <immintrin.h>
#endif
#ifdef MMML_ARM_RBIT
#include <arm_acle.h>
#endif
#ifdef MMML_ARM_PMUL
#include <arm_neon.h>
#endif
#ifdef MMML_ARM_SVE2
#include <arm_sve.h>
#endif
#endif

// COMPILER-SPECIFIC FEATURES
// ==========================

#ifdef MMML_GNU
#define MMML_ALWAYS_INLINE [[gnu::always_inline]]
#define MMML_AGGRESSIVE_UNROLL _Pragma("GCC unroll 16")

#elif defined(MMML_MSVC)
#define MMML_ALWAYS_INLINE __forceinline

#else
#define MMML_ALWAYS_INLINE inline
#define MMML_AGGRESSIVE_UNROLL
#endif

// =================================================================================================
// =================================================================================================
// =================================================================================================
// END OF CONFIG === END OF CONFIG === END OF CONFIG === END OF CONFIG === END OF CONFIG === END OF
// =================================================================================================
// =================================================================================================
// =================================================================================================

namespace mmml {

namespace detail {

template <typename T>
inline constexpr int digits_v = std::numeric_limits<T>::digits;

/// Computes `floor(log2(max(1, x)))` of an  integer `x`.
/// If x is zero or negative, returns zero.
[[nodiscard]]
constexpr int log2_floor(int x) noexcept
{
    return x < 1 ? 0 : digits_v<unsigned> - std::countl_zero(static_cast<unsigned>(x)) - 1;
}

} // namespace detail

namespace detail {

/// Each bit in `x` is converted to the parity a bit and all bits to its right.
/// This can also be expressed as `CLMUL(x, -1)` where `CLMUL` is a carry-less
/// multiplication.
template <std::unsigned_integral T>
[[nodiscard]]
constexpr T bitwise_inclusive_right_parity(T x) noexcept
{
    constexpr int N = digits_v<T>;

#ifdef MMML_X86_PCLMUL
    if MMML_NOT_CONSTANT_EVALUATED {
        if constexpr (N <= 64) {
            const __m128i x_128 = _mm_set_epi64x(0, x);
            const __m128i neg1_128 = _mm_set_epi64x(0, -1);
            const __m128i result_128 = _mm_clmulepi64_si128(x_128, neg1_128, 0);
            return static_cast<T>(_mm_extract_epi64(result_128, 0));
        }
    }
#endif
    // TODO: Technically, ARM does have some support for polynomial multiplication prior to
    // SVE2.
    //       However, this support is fairly limited and it's not even clear whether it
    //       beats this implementation.
    for (int i = 1; i < N; i <<= 1) {
        x ^= x << i;
    }
    return x;
}

} // namespace detail

template <std::unsigned_integral T>
[[nodiscard]]
constexpr T bit_compress(T x, T m) noexcept
{
    constexpr int N = detail::digits_v<T>;

#ifdef MMML_X86_PEXT
    if MMML_NOT_CONSTANT_EVALUATED {
        if constexpr (N <= 32) {
            return static_cast<T>(_pext_u32(x, m));
        }
        else if constexpr (N <= 64) {
            return static_cast<T>(_pext_u64(x, m));
        }
    }
#endif

#ifdef MMML_ARM_BEXT
    if MMML_NOT_CONSTANT_EVALUATED {
        if constexpr (N <= 8) {
            auto sv_result = svbext_u8(svdup_u8(x), svdup_u8(m));
            return static_cast<T>(svorv_u8(svptrue_b8(), sv_result));
        }
        else if constexpr (N <= 16) {
            auto sv_result = svbext_u16(svdup_u16(x), svdup_u16(m));
            return static_cast<T>(svorv_u16(svptrue_b16(), sv_result));
        }
        else if constexpr (N <= 32) {
            auto sv_result = svbext_u32(svdup_u32(x), svdup_u32(m));
            return static_cast<T>(svorv_u32(svptrue_b32(), sv_result));
        }
        else if constexpr (N <= 64) {
            auto sv_result = svbext_u64(svdup_u64(x), svdup_u64(m));
            return static_cast<T>(svorv_u64(svptrue_b64(), sv_result));
        }
    }
#endif
    x &= m;
    T mk = ~m << 1;

    MMML_AGGRESSIVE_UNROLL
    for (int i = 1; i < N; i <<= 1) {
        const T mk_parity = detail::bitwise_inclusive_right_parity(mk);

        const T move = mk_parity & m;
        m = (m ^ move) | (move >> i);

        const T t = x & move;
        x = (x ^ t) | (t >> i);

        mk &= ~mk_parity;
    }
    return x;
}

template <std::unsigned_integral T>
[[nodiscard]]
constexpr T bit_expand(T x, T m) noexcept
{
    constexpr int N = detail::digits_v<T>;
    constexpr int log_N = detail::log2_floor(std::bit_ceil<unsigned>(N));

#ifdef MMML_X86_PDEP
    if MMML_NOT_CONSTANT_EVALUATED {
        if constexpr (N <= 32) {
            return _pdep_u32(x, m);
        }
        else if constexpr (N <= 64) {
            return _pdep_u64(x, m);
        }
    }
#endif

#ifdef MMML_ARM_BDEP
    if MMML_NOT_CONSTANT_EVALUATED {
        if constexpr (N <= 8) {
            auto sv_result = svbdep_u8(svdup_u8(x), svdup_u8(m));
            return static_cast<T>(svorv_u8(svptrue_b8(), sv_result));
        }
        else if constexpr (N <= 16) {
            auto sv_result = svbdep_u16(svdup_u16(x), svdup_u16(m));
            return static_cast<T>(svorv_u16(svptrue_b16(), sv_result));
        }
        else if constexpr (N <= 32) {
            auto sv_result = svbdep_u32(svdup_u32(x), svdup_u32(m));
            return static_cast<T>(svorv_u32(svptrue_b32(), sv_result));
        }
        else if constexpr (N <= 64) {
            auto sv_result = svbdep_u64(svdup_u64(x), svdup_u64(m));
            return static_cast<T>(svorv_u64(svptrue_b64(), sv_result));
        }
    }
#endif
    const T initial_m = m;

    T array[log_N];
    T mk = ~m << 1;

    MMML_AGGRESSIVE_UNROLL
    for (int i = 0; i < log_N; ++i) {
        const T mk_parity = detail::bitwise_inclusive_right_parity(mk);
        const T move = mk_parity & m;
        m = (m ^ move) | (move >> (1 << i));
        array[i] = move;
        mk &= ~mk_parity;
    }

    MMML_AGGRESSIVE_UNROLL
    for (int i = log_N; i > 0;) {
        --i; // Normally, I would write (i-- > 0), but this triggers
             // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=113581
        const T move = array[i];
        const T t = x << (1 << i);
        x = (x & ~move) | (t & move);
    }

    return x & initial_m;
}

} // namespace mmml

#endif
