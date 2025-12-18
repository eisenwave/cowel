#include <charconv>

#include "cowel/util/assert.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/math.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/settings.hpp"

namespace cowel {
namespace {

consteval int u64_max_output_digits_naive(int base)
{
    COWEL_ASSERT(base >= 2);
    std::uint64_t x = std::numeric_limits<std::uint64_t>::max();
    int result = 0;
    while (x != 0) {
        x /= std::uint64_t(base);
        ++result;
    }
    return result;
}

/// @brief Returns the amount of digits necessary to represent
/// `numeric_limits<std::uint64_t>::max()` in the given base.
/// Mathematically, this is `floor(log(pow(2, 64)) / log(base))`.
[[maybe_unused]] [[nodiscard]]
constexpr int u64_max_output_digits(int base)
{
    COWEL_DEBUG_ASSERT(base >= 2);
    COWEL_DEBUG_ASSERT(base <= 36);

    static constexpr auto table = [] consteval {
        std::array<signed char, 36> result {};
        for (std::size_t i = 2; i < result.size(); ++i) {
            result[i] = static_cast<signed char>(u64_max_output_digits_naive(int(i)));
        }
        return result;
    }();
    return table[std::size_t(base)];
}

consteval int u64_max_input_digits_naive(int base)
{
    COWEL_ASSERT(base >= 2);

    const auto max = Int128(1) << 64;
    Int128 x = 1;
    int result = 0;
    while (x <= max) {
        x *= unsigned(base);
        ++result;
        if (x == 0) {
            break;
        }
    }
    return result - 1;
}

/// @brief Returns the amount of digits that `std::uint64_t` can represent
/// in the given base.
/// Mathematically, this is `floor(log(pow(2, 64)) / log(base))`.
constexpr int u64_max_input_digits(int base)
{
    COWEL_DEBUG_ASSERT(base >= 2);
    COWEL_DEBUG_ASSERT(base <= 36);

    static constexpr auto table = [] consteval {
        std::array<signed char, 36> result {};
        for (std::size_t i = 2; i < result.size(); ++i) {
            result[i] = static_cast<signed char>(u64_max_input_digits_naive(int(i)));
        }
        return result;
    }();
    return table[std::size_t(base)];
}

[[nodiscard]]
consteval std::uint64_t u64_pow_naive(std::uint64_t x, int y)
{
    std::uint64_t result = 1;
    for (int i = 0; i < y; ++i) {
        result *= x;
    }
    return result;
}

/// @brief Returns the greatest power of `base` representable in `std::uint64_t`,
/// or zero if the next greater power is exactly `pow(2, 64)`.
///
/// A result of zero essentially communicates that no bit of `std::uint64_t` is wasted,
/// such as in the base-2 or base-16 case.
[[nodiscard]]
constexpr std::uint64_t u64_max_power(int base)
{
    COWEL_DEBUG_ASSERT(base >= 2);
    COWEL_DEBUG_ASSERT(base <= 36);

    static constexpr auto table = [] consteval {
        std::array<std::uint64_t, 36> result {};
        for (std::size_t i = 2; i < result.size(); ++i) {
            const int max_exponent = u64_max_input_digits(int(i));
            result[i] = u64_pow_naive(i, max_exponent);
        }
        return result;
    }();
    return table[std::size_t(base)];
}

static_assert(u64_max_output_digits_naive(2) == 64);
static_assert(u64_max_output_digits(2) == 64);
static_assert(u64_max_output_digits_naive(16) == 16);
static_assert(u64_max_output_digits(16) == 16);
static_assert(u64_max_output_digits_naive(10) == 20);
static_assert(u64_max_output_digits(10) == 20);

static_assert(u64_max_input_digits_naive(2) == 64);
static_assert(u64_max_input_digits(2) == 64);
static_assert(u64_max_input_digits_naive(8) == 21);
static_assert(u64_max_input_digits(8) == 21);
static_assert(u64_max_input_digits_naive(10) == 19);
static_assert(u64_max_input_digits(10) == 19);
static_assert(u64_max_input_digits_naive(16) == 16);
static_assert(u64_max_input_digits(16) == 16);

static_assert(u64_max_power(2) == 0);
static_assert(u64_max_power(8) == 0x8000000000000000ull);
static_assert(u64_max_power(10) == 10000000000000000000ull);
static_assert(u64_max_power(16) == 0);

} // namespace

/// @brief Implements the interface of `to_chars` for decimal input of 128-bit integers.
/// In the "happy case" of having at most 19 digits,
/// this simply calls `std::from_chars` for 64-bit integers.
/// In the worst case, three such 64-bit calls are needed,
/// handling 19 digits at a time, with 39 decimal digits being the maximum for 128-bit.
[[nodiscard]]
std::from_chars_result
from_chars128(const char* const first, const char* const last, Uint128& out, int base)
{
    COWEL_ASSERT(base >= 2);
    COWEL_ASSERT(base <= 36);
    COWEL_DEBUG_ASSERT(first);
    COWEL_DEBUG_ASSERT(last);

    if (first == last) {
        return { last, std::errc::invalid_argument };
    }

    Uint128 result = 0;
    const char* current_last = last;

    const std::uint64_t max_pow = u64_max_power(base);
    const std::ptrdiff_t max_lower_length = u64_max_input_digits(base);
    const bool is_pow_2 = (base & (base - 1)) == 0;
    COWEL_DEBUG_ASSERT(max_pow != 0 || is_pow_2);

    if (is_pow_2) {
        const int bits_per_iteration = std::countr_zero(max_pow);
        int shift = 0;

        while (true) {
            const auto lower_length = std::min(current_last - first, max_lower_length);
            const char* const current_first = current_last - lower_length;

            std::uint64_t digits {};
            const std::from_chars_result partial_result
                = std::from_chars(current_first, current_last, digits, base);
            if (partial_result.ec != std::errc {}) {
                // Since we only handle as many digits as can fit into a 64-bit integer,
                // the only possible failure should be an invalid string.
                COWEL_ASSERT(partial_result.ec == std::errc::invalid_argument);
                return partial_result;
            }

            const int added_digits = 64 - std::countl_zero(digits);
            if (shift + added_digits > 128) {
                return { last, std::errc::result_out_of_range };
            }

            result |= Uint128(digits) << shift;
            shift += bits_per_iteration;

            if (current_last - first <= max_lower_length || partial_result.ec != std::errc {}) {
                out = result;
                return partial_result;
            }
            current_last -= lower_length;
            COWEL_DEBUG_ASSERT(current_last >= first);
        }
    }
    else {
        Uint128 factor = 1;

        while (true) {
            const auto lower_length = std::min(current_last - first, max_lower_length);
            const char* const current_first = current_last - lower_length;

            std::uint64_t digits {};
            const std::from_chars_result partial_result
                = std::from_chars(current_first, current_last, digits, base);

            Uint128 summand;
            if (mul_overflow(summand, factor, digits)) {
                return { last, std::errc::result_out_of_range };
            }
            if (add_overflow(result, result, summand)) {
                return { last, std::errc::result_out_of_range };
            }

            if (current_last - first <= max_lower_length || partial_result.ec != std::errc {}) {
                out = result;
                return partial_result;
            }
            factor *= max_pow;
            current_last -= lower_length;
            COWEL_DEBUG_ASSERT(current_last >= first);
        }
    }
}

[[nodiscard]]
std::from_chars_result
from_chars128(const char* const first, const char* const last, Int128& out, int base)
{
    COWEL_DEBUG_ASSERT(first);
    COWEL_DEBUG_ASSERT(last);

    if (first == last) {
        return { last, std::errc::invalid_argument };
    }

    if (*first != '-') {
        Uint128 x {};
        const std::from_chars_result result = from_chars128(first, last, x, base);
        if (x >> 127) {
            return { result.ptr, std::errc::result_out_of_range };
        }
        out = Int128(x);
        return result;
    }
    const std::ptrdiff_t max_lower_length = u64_max_input_digits(base);
    if (last - first + 1 <= max_lower_length) {
        std::int64_t x {};
        const std::from_chars_result result = std::from_chars(first, last, x, base);
        out = x;
        return result;
    }
    constexpr auto max_u128 = Uint128 { 1 } << 127;
    Uint128 x {};
    const std::from_chars_result result = from_chars128(first + 1, last, x, base);
    if (x > max_u128) {
        return { result.ptr, std::errc::result_out_of_range };
    }
    out = Int128(-x);
    return result;
}

[[nodiscard]]
std::to_chars_result to_chars128(char* const first, char* const last, const Uint128 x, int base)
{
    COWEL_DEBUG_ASSERT(first);
    COWEL_DEBUG_ASSERT(last);
    COWEL_DEBUG_ASSERT(base >= 2 && base <= 36);

    if (x <= std::uint64_t(-1)) {
        return std::to_chars(first, last, std::uint64_t(x), base);
    }
    if (first == last) {
        return { last, std::errc::value_too_large };
    }

    const std::uint64_t max_pow = u64_max_power(base);
    const bool is_pow_2 = (base & (base - 1)) == 0;
    const int piece_max_digits = u64_max_input_digits(base);

    if (is_pow_2) {
        const int bits_per_iteration = std::countr_zero(max_pow);
        COWEL_DEBUG_ASSERT(bits_per_iteration != 0);
        const int leading_bits = 128 % bits_per_iteration;

        char* current_first = first;
        bool first_digit = true;

        // First, we need to take care of the leading "head" bits.
        // For example, for octal, we operate on 63 bits at a time,
        // and 2 leading bits are left over.
        if (leading_bits != 0) {
            const auto head_mask = (std::uint64_t(1) << leading_bits) - 1;
            const auto head = std::uint64_t(x >> (128 - leading_bits)) & head_mask;
            if (head != 0) {
                first_digit = false;
                const std::to_chars_result head_result = std::to_chars(first, last, head, base);
                if (head_result.ec != std::errc {}) {
                    return head_result;
                }
                current_first = head_result.ptr;
            }
        }

        int shift = 128 - leading_bits - bits_per_iteration;
        COWEL_DEBUG_ASSERT(bits_per_iteration <= 64);
        const auto mask = //
            (bits_per_iteration == 64 ? std::uint64_t(0) : (std::uint64_t(1) << bits_per_iteration))
            - 1;

        // Once the head digits are printed out, every subsequent block of bits
        // has exactly the same amount of digits.
        // For example, for octal, there are 126 bits left,
        // handled exactly 63 bits at a time.
        while (true) {
            const auto piece = std::uint64_t(x >> shift) & mask;
            const std::to_chars_result piece_result
                = std::to_chars(current_first, last, piece, base);
            if (piece_result.ec != std::errc {}) {
                return piece_result;
            }
            const auto zero_pad = piece_max_digits - int(piece_result.ptr - current_first);
            if (!first_digit && zero_pad != 0) {
                std::ranges::copy(current_first, piece_result.ptr, current_first + zero_pad);
                std::ranges::fill_n(current_first, zero_pad, '0');
                current_first += piece_max_digits;
            }
            else {
                current_first = piece_result.ptr;
            }

            if (shift <= 0) {
                COWEL_ASSERT(shift == 0);
                return { current_first, std::errc {} };
            }
            shift -= bits_per_iteration;
            first_digit = false;
        }
    }
    // NOLINTNEXTLINE(readability-else-after-return)
    else {
        const std::to_chars_result upper_result = to_chars128(first, last, x / max_pow, base);
        if (upper_result.ec != std::errc {}) {
            return upper_result;
        }

        const std::to_chars_result lower_result
            = std::to_chars(upper_result.ptr, last, std::uint64_t(x % max_pow));
        if (lower_result.ec != std::errc {}) {
            return lower_result;
        }
        const auto lower_length = lower_result.ptr - upper_result.ptr;

        // The remainder (lower part) is mathematically exactly 19 digits long,
        // and we have to zero-pad to the left if it is shorter
        // (because std::to_chars wouldn't give us the leading zeros we need).
        char* const result_end = upper_result.ptr + piece_max_digits;
        std::ranges::copy(
            upper_result.ptr, upper_result.ptr + lower_length, result_end - lower_length
        );
        std::ranges::fill_n(upper_result.ptr, piece_max_digits - lower_length, '0');

        return { result_end, std::errc {} };
    }
    COWEL_ASSERT_UNREACHABLE(u8"Should have returned.");
}

[[nodiscard]]
std::to_chars_result to_chars128(char* const first, char* const last, const Int128 x, int base)
{
    COWEL_DEBUG_ASSERT(base >= 2 && base <= 36);

    if (x >= 0) {
        return to_chars128(first, last, Uint128(x), base);
    }
    if (x == std::int64_t(x)) {
        return std::to_chars(first, last, std::int64_t(x), base);
    }
    if (last - first < 2) {
        return { last, std::errc::value_too_large };
    }
    *first = '-';
    return to_chars128(first + 1, last, -Uint128(x), base);
}

template std::from_chars_result from_characters(std::string_view, float&, std::chars_format);
template std::from_chars_result from_characters(std::string_view, double&, std::chars_format);

} // namespace cowel
